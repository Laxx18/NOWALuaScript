#include "matchclassworker.h"
#include "luaeditormodelitem.h"
#include "apimodel.h"

#include <QRegularExpression>

MatchClassWorker::MatchClassWorker(LuaEditorModelItem* luaEditorModelItem, bool forConstant, bool forFunctionParameters, const QString& currentText,  const QString& textAfterKeyword, int cursorPosition, int mouseX, int mouseY)
    : luaEditorModelItem(luaEditorModelItem),
    currentText(currentText),
    forConstant(forConstant),
    forFunctionParameters(forFunctionParameters),
    typedAfterKeyword(textAfterKeyword),
    cursorPosition(cursorPosition),
    oldCursorPosition(cursorPosition),
    mouseX(mouseX),
    mouseY(mouseY),
    isProcessing(false),
    isStopped(true),
    forVariable(true),
    variableFound(false)
{

}

void MatchClassWorker::setParameters(bool forConstant, bool forFunctionParameters, const QString& currentText, const QString& textAfterKeyword, int cursorPos, int mouseX, int mouseY)
{
    this->forConstant = forConstant;
    this->forFunctionParameters = forFunctionParameters;
    this->currentText = currentText;
    this->typedAfterKeyword = textAfterKeyword;
    this->oldCursorPosition = this->cursorPosition;
    this->cursorPosition = cursorPos;
    this->mouseX = mouseX;
    this->mouseY = mouseY;
}

void MatchClassWorker::stopProcessing(void)
{
    this->isStopped = true;
}

void MatchClassWorker::process(void)
{
    // Set the flag to indicate processing has started
    this->isProcessing = true;
    this->isStopped = false; // Reset the stop flag

    // Set the content for processing
    this->luaEditorModelItem->setContent(this->currentText);

    QString oldMatchedClassName = this->matchedClassName;

    bool handleOuterSegment = true;
    this->handleCurrentLine("", handleOuterSegment);

    if (this->matchedClassName != oldMatchedClassName/* && false == oldMatchedClassName.isEmpty()*/)
    {
        ApiModel::instance()->setSelectedClassName(this->matchedClassName);
    }

    // Checks if a new variable or singleton Name is being typed, if there is no matched class name so far
    if (this->restTyped.size() > 2 && true == this->forVariable)
    {
        QChar startChar = this->restTyped.at(0);
        bool forSingleton = startChar.isUpper();
        QVariantMap variablesMap = this->luaEditorModelItem->processMatchedVariables(forSingleton, this->restTyped);
        if (false == variablesMap.empty())
        {
            ApiModel::instance()->setMatchedVariables(variablesMap);

            if (false == variablesMap.empty())
            {
                // Retrieves the first key from the QVariantMap
                QString firstKey = variablesMap.keys().first();

                // Accesses the QVariantMap associated with the first key
                QVariantMap firstVariable = variablesMap.value(firstKey).toMap();

                // Retrieves the "type" field
                QString variableType = firstVariable.value("type").toString();

                // Passes the type to getMethodsForClassName
                // const auto& methods = ApiModel::instance()->getMethodsForClassName(variableType);

                // Causes to much other trouble
                // if (false == methods.isEmpty())
                {
                    this->variableFound = true;
                    // Emit signal directly since the data structure has already been updated
                    Q_EMIT ApiModel::instance()->signal_showIntelliSenseMenu("forVariable", mouseX, mouseY);
                    this->isProcessing = false;
                    return;
                }
                // else
                // {
                //     ApiModel::instance()->showNothingFound("noVariableFound:" + variableType, mouseX, mouseY);
                //     return;
                // }
            }
            else
            {
                this->variableFound = false;
                this->forVariable = false;
                bool handleOuterSegment = true;
                this->handleCurrentLine("", handleOuterSegment);
            }
        }
    }

    this->forVariable = true;

    // Only re-match variables if the menu is not shown and user is not typing something after colon, because else the auto completion would not work, because each time variables would be re-detected
    // But user typed already the next expression, which would no more match!

    if (false == this->typedInsideFunction.isEmpty())
    {
        if (false == this->matchedClassName.isEmpty() && false == this->matchedMethodName.isEmpty())
        {
            if (true == this->forFunctionParameters)
            {
                if (ApiModel::instance()->getIsIntellisenseShown())
                {
                    ApiModel::instance()->closeIntellisense();
                }

                if (true == this->handleInsideFunctionParameters())
                {
                    // Check if we should stop before triggering the menu
                    if (this->isStopped)
                    {
                        this->isProcessing = false; // Reset the flag before exiting
                        return; // Exit if stopping
                    }
                    return;
                }
            }
        }
    }

    // Check if we should stop before processing matched methods
    if (!this->typedAfterKeyword.isEmpty())
    {
        // Ensure we check again for stop condition
        if (this->isStopped)
        {
            this->isProcessing = false; // Reset the flag before exiting
            return; // Exit if stopping
        }

        // Removes any proceeding ":", "."
        QString cleanTypedAfterKeyword = this->typedAfterKeyword;
        cleanTypedAfterKeyword.remove(QRegularExpression(R"([():.\s])"));
        if (false == this->forConstant)
        {
            if (false == this->matchedClassName.isEmpty())
            {
                ApiModel::instance()->processMatchedMethodsForSelectedClass(this->matchedClassName, cleanTypedAfterKeyword);
                ApiModel::instance()->showIntelliSenseMenu("forClass", this->matchedClassName, mouseX, mouseY);
            }
            else
            {
                // Causes to much other trouble
                // ApiModel::instance()->showNothingFound("noClassFound", mouseX, mouseY);
            }
        }
        else
        {
            if (false == this->matchedClassName.isEmpty())
            {
                ApiModel::instance()->processMatchedConstantsForSelectedClass(this->matchedClassName, cleanTypedAfterKeyword);
                ApiModel::instance()->showIntelliSenseMenu("forConstant", this->matchedClassName, mouseX, mouseY);
            }
            else
            {
                // Causes to much other trouble
                // ApiModel::instance()->showNothingFound("noConstantFound", mouseX, mouseY);
            }
        }
        this->typedAfterKeyword.clear();
    }

    // Reset the processing flag before exiting
    this->isProcessing = false;
}

QString MatchClassWorker::handleCurrentLine(const QString& segment, bool& handleOuterSegment)
{
    if (true == segment.isEmpty())
    {
        this->restTyped.clear();
        this->matchedClassName.clear();
        this->matchedMethodName.clear();
        this->typedInsideFunction.clear();
    }

    if (std::abs(this->cursorPosition - this->oldCursorPosition) > 1 || false == this->luaEditorModelItem->hasVariablesDetected())
    {
        this->luaEditorModelItem->detectVariables();
    }

    QString textUpToCursor;
    int lastNewlinePos = -1;
    QString currentLine;
    int lineCursorPos = -1;
    int lineIndex = -1;

    if (true == segment.isEmpty())
    {
        textUpToCursor = this->currentText.left(this->cursorPosition);
        lastNewlinePos = textUpToCursor.lastIndexOf('\n');
        currentLine = textUpToCursor.mid(lastNewlinePos + 1);
        lineCursorPos = this->cursorPosition - (lastNewlinePos + 1);
        lineIndex = textUpToCursor.count('\n') + 1;
    }
    else
    {
        currentLine = segment;
        lineCursorPos = currentLine.size();

        // Own segment area
        if (false == currentLine.contains(","))
        {
            if (false == this->containsDelimiterWithQuoteBefore(currentLine))
            {
                handleOuterSegment = false;
            }
        }

        QString wholeTextUpToCursor = this->currentText.left(this->cursorPosition);
        lineIndex = wholeTextUpToCursor.count('\n') + 1;
    }

    // Detect the position of the first unbalanced opening parenthesis before the cursor
    int bracketBalance = 0;
    int unmatchedOpenBracketPos = -1;

    // Skip the last open bracket if it's at the cursor position
    int adjustedCursorPos = lineCursorPos;
    if (adjustedCursorPos > 0 && currentLine[adjustedCursorPos - 1] == '(')
    {
        --adjustedCursorPos; // Ignore this bracket in the detection
    }

    unmatchedOpenBracketPos = this->findUnmatchedOpenBracket(currentLine, adjustedCursorPos);

    QChar foundDelimeter = this->containsColonOrDotBeforeComma(currentLine, adjustedCursorPos, unmatchedOpenBracketPos);
    if ( '\0' != foundDelimeter)
    {
        this->forVariable = false;
        if ('.' == foundDelimeter)
        {
            this->forConstant = true;
        }
    }

    QString localSegment;

    // If there's an unmatched '(' before the cursor, use it to split the tokens

    // Handle cases where there's a comma followed by the typed parameter
    // e.g., `, ene` => remove the comma and trim the "ene"
    // Find the position of the last comma
    int lastCommaPos = currentLine.lastIndexOf(',');

    QChar currentCharAtCursor = currentLine[adjustedCursorPos - 1];

    // Function , highlight will no more work!
    // If there's a comma before the cursor position, extract the part after the comma
    if (lastCommaPos != -1 && lastCommaPos < lineCursorPos)
    {
        // Extract the text from the last comma to the lineCursorPos
        localSegment = currentLine.mid(lastCommaPos + 1, lineCursorPos - lastCommaPos - 1).trimmed();
        if (false == localSegment.isEmpty())
        {
            this->typedInsideFunction = currentLine.mid(unmatchedOpenBracketPos, adjustedCursorPos - unmatchedOpenBracketPos + 1);
            currentLine = this->handleCurrentLine(localSegment, handleOuterSegment);
            // AppStateManager:getGameObjectController():activatePlayerController(true, ene
            // localSegment = "ene" -> restTyped = ene, to not handle outer segment, its inside for itself
            if (false == this->restTyped.isEmpty() && true == this->variableFound)
            {
                handleOuterSegment = false;
                return currentLine;
            }
        }
        else
        {
            // Just pressed "," determine typed inside function
            if ("," == currentCharAtCursor)
            {
                this->typedInsideFunction = currentLine.mid(unmatchedOpenBracketPos, adjustedCursorPos - unmatchedOpenBracketPos + 1);
                this->forVariable = false;
            }
        }
    }

    // If there's an open bracket before the cursor position, extract the part after the open bracket
    if (unmatchedOpenBracketPos != -1 && unmatchedOpenBracketPos < lineCursorPos && currentCharAtCursor != ",")
    {
        // Extract the text from the last open bracket to the lineCursorPos
        localSegment = currentLine.mid(unmatchedOpenBracketPos + 1, lineCursorPos - unmatchedOpenBracketPos - 1).trimmed();
        if (false == localSegment.isEmpty())
        {
            this->typedInsideFunction = currentLine.mid(unmatchedOpenBracketPos, adjustedCursorPos - unmatchedOpenBracketPos + 1);
            currentLine = this->handleCurrentLine(localSegment, handleOuterSegment);
        }
    }

    if (true == localSegment.isEmpty() && unmatchedOpenBracketPos != -1 && currentCharAtCursor != ",")
    {
        int segmentCursorPos = lineCursorPos - unmatchedOpenBracketPos;
        localSegment = currentLine.mid(unmatchedOpenBracketPos, segmentCursorPos);

        qDebug() << "-->Calling handleCurrentLine for segment: " << localSegment;
        if (false == localSegment.isEmpty())
        {
            this->typedInsideFunction = currentLine.mid(unmatchedOpenBracketPos, adjustedCursorPos - unmatchedOpenBracketPos + 1);
            currentLine = this->handleCurrentLine(localSegment, handleOuterSegment);
        }
    }

    if (true == segment.isEmpty() && false == handleOuterSegment)
    {
        return "";
    }

    // Match left free part

    // AppStateManager:getGameObjectController():activatePlayerController(energy:getName(),
    QString wholeTextUpToCursor = this->currentText.left(this->cursorPosition);
    int wholeLastNewlinePos = wholeTextUpToCursor.lastIndexOf('\n');
    QString wholeCurrentLine = wholeTextUpToCursor.mid(wholeLastNewlinePos + 1);

    // energy:getName(),
    QString leftFreeCurrentLine = currentLine;

    int upToIndex = wholeCurrentLine.lastIndexOf(currentLine);
    if (upToIndex > 0 && true == segment.isEmpty())
    {
        // Extract the substring up to the segment, so that outer line is parsed
        leftFreeCurrentLine = wholeCurrentLine.left(upToIndex);
        // AppStateManager:getGameObjectController():activatePlayerController(
        lineCursorPos -= currentLine.size();
    }

    QString textToSplit = leftFreeCurrentLine.left(lineCursorPos);

    // Split up to first unmatched (
    // E.g.: gameObjectTitleComponent:setCaption(rightLowerArm:getPhysicsRagDollComponent():getAngularDamping());
    // -> gameObjectTitleComponent:setCaption
    if (-1 != unmatchedOpenBracketPos)
    {
        textToSplit = textToSplit.left(unmatchedOpenBracketPos + 1);
    }

    qDebug() << "->textToSplit ---> " << textToSplit;

    QStringList tokens = textToSplit.split(QRegularExpression(R"([:.(),])"), Qt::SkipEmptyParts);

    QList<int> delimiterPositions;
    QRegularExpression delimiterRegex(R"([:.])");
    QRegularExpressionMatchIterator it = delimiterRegex.globalMatch(textToSplit);

    while (it.hasNext())
    {
        delimiterPositions.append(it.next().capturedStart());
    }

    // Initialize variables for parsing
    QChar lastDelimiter = '\0';
    QString rootClassName;

    const auto& variableMap = this->luaEditorModelItem->getVariableMap();

    for (int i = 0; i < tokens.size(); ++i)
    {
        QString token = tokens[i].trimmed();
        qDebug() << "Detected Token:" << token;
        int tokenPos = -1;
        if (i > 0)
        {
            tokenPos = delimiterPositions.value(i - 1, -1);
        }
        bool isMatched = false;

        if (i == 0)
        {
            if (variableMap.contains(token))
            {
                const auto& luaVariableInfo = variableMap[token];
                rootClassName = luaVariableInfo.type;
                this->matchedClassName = rootClassName;

                if (1 == tokens.size())
                {
                    return currentLine;
                }

                for (const auto& chainTypeInfo : luaVariableInfo.chainTypeList)
                {
                    if (!chainTypeInfo.chainType.isEmpty() && chainTypeInfo.line == lineIndex)
                    {
                        this->matchedClassName = chainTypeInfo.chainType;
                        break;
                    }
                }
            }
            else
            {
                // Case e.g. inside function parameters: physicsActiveComponent:setDirection(Vector3.
                // Segment: Vector3, delimeter = ".", check constant
                const auto& constants = ApiModel::instance()->getConstantsForClassName(token);

                if (false == constants.isEmpty())
                {
                    rootClassName = token;
                    this->matchedClassName = rootClassName;
                    isMatched = true;
                    break;
                }
                else
                {
                    // Case e.g. inside function parameters: physicsActiveComponent:setDirection(physicsActiveComponent:getDirection() +...
                    // Segment: Vector3, delimeter = ":", check methods
                    const auto& methods = ApiModel::instance()->getMethodsForClassName(rootClassName);

                    if (false == methods.isEmpty())
                    {
                        for (const QVariant& methodVariant : methods)
                        {
                            QVariantMap methodMap = methodVariant.toMap();
                            if (methodMap["name"].toString() == token)
                            {
                                this->matchedMethodName = token;
                                rootClassName = this->matchedClassName;
                                isMatched = true;
                                break;
                            }
                        }
                    }
                    else
                    {
                        qDebug() << "Token not found in variable map:" << token << " using token for rest typed.";
                        this->restTyped = token;
                        // For now do not handle outer segment
                        if (true == this->forVariable && this->restTyped.size() > 2)
                        {
                            handleOuterSegment = false;
                        }
                        return currentLine;
                    }
                }
            }
        }
        else
        {
            if (tokenPos > 0)
            {
                lastDelimiter = textToSplit[tokenPos];
            }

            if (lastDelimiter == ':')
            {
                const auto& methods = ApiModel::instance()->getMethodsForClassName(rootClassName);

                for (const QVariant& methodVariant : methods)
                {
                    QVariantMap methodMap = methodVariant.toMap();
                    if (methodMap["name"].toString() == token)
                    {
                        this->matchedMethodName = token;

                        QString valueType = methodMap["valuetype"].toString();
                        if (false == this->isLuaNativeType(valueType))
                        {
                            this->matchedClassName = methodMap["valuetype"].toString();
                        }
                        rootClassName = this->matchedClassName;

                        this->forConstant = false;
                        isMatched = true;
                        break;
                    }
                }
            }
            else if (lastDelimiter == '.')
            {
                const auto& constants = ApiModel::instance()->getConstantsForClassName(rootClassName);

                for (const QVariant& constantVariant : constants)
                {
                    QVariantMap constantMap = constantVariant.toMap();
                    if (constantMap["name"].toString() == token)
                    {
                        this->matchedMethodName = token;
                        this->forConstant = true;
                        isMatched = true;
                        break;
                    }
                }
            }
            else if (token.startsWith("\"") || token.startsWith("'")) // Handle strings
            {
                this->restTyped = token;
                return currentLine;
            }

            if (!isMatched)
            {
                // Track the unmatched token as part of the restTyped
                this->restTyped = token;
                qDebug() << "Unmatched token stored in restTyped:" << this->restTyped;
                break; // Stop further processing as this token is unmatched
            }
        }
    }

    qDebug() << "Matched Class Name:" << this->matchedClassName;
    qDebug() << "Matched Method Name:" << this->matchedMethodName;
    qDebug() << "Matched Typed Inside Function:" << this->typedInsideFunction;
    qDebug() << "Rest Typed:" << this->restTyped;

    return currentLine;
}

bool MatchClassWorker::isLuaNativeType(const QString& typeName)
{
    // List of Lua native types
    const QSet<QString> luaNativeTypes = {
        "nil", "boolean", "number", "string", "function", "userdata", "thread", "table", "void"
    };

    // Check if the provided typeName matches any Lua native type
    return luaNativeTypes.contains(typeName.trimmed());
}

int MatchClassWorker::findUnmatchedOpenBracket(const QString& line, int cursorPos)
{
    // Initialize variables
    int bracketBalance = 0;
    int unmatchedOpenBracketPos = -1;

    // Loop through the string backward from the adjusted cursor position
    for (int i = cursorPos - 1; i >= 0; --i)
    {
        QChar ch = line[i];

        if (ch == ')')
        {
            // Increase the balance for each closing bracket
            ++bracketBalance;
        }
        else if (ch == '(')
        {
            // Decrease the balance for each opening bracket
            --bracketBalance;

            if (bracketBalance == 0)
            {
                // Balanced parenthesis - reset potential unmatched position
                unmatchedOpenBracketPos = -1;
            }
            else if (bracketBalance < 0)
            {
                // Found an unmatched opening bracket
                unmatchedOpenBracketPos = i;
                break;
            }
        }
    }

    // Return the position of the unmatched '(' or -1 if all brackets are balanced
    return unmatchedOpenBracketPos;
}

bool MatchClassWorker::containsDelimiterWithQuoteBefore(const QString& currentLine)
{
    // Find positions of ':' and '.'
    QRegularExpression delimiterRegex(R"([:.])");
    QRegularExpressionMatchIterator it = delimiterRegex.globalMatch(currentLine);

    while (it.hasNext())
    {
        QRegularExpressionMatch match = it.next();
        int delimiterPos = match.capturedStart();

        // Check for quotes before the delimiter
        for (int i = delimiterPos - 1; i >= 0; --i)
        {
            QChar ch = currentLine[i];
            if (ch == '"' || ch == '\'')
            {
                // Quote found before the delimiter
                return true;
            }
            else if (!ch.isSpace())
            {
                // Non-quote character encountered, stop checking further
                return false;
            }
        }
    }

    // No quote found before any delimiter
    return true;
}

QChar MatchClassWorker::containsColonOrDotBeforeComma(const QString& text, int cursorPos, int unbalancedOpenBracketPos)
{
    QChar delimeterType = '\0';
    if (text.size() < 2)
    {
        return delimeterType;
    }
    // Goes either to the first "," or if there is an unbalancedOpenBracketPos, to that index, or to the beginning and checks if method or constant matching is involved and returns either ":", or "." or if nothing found '\0'
    int upFromIndex = unbalancedOpenBracketPos;
    if (-1 == upFromIndex)
    {
        upFromIndex = 0;
    }
    for (int i = cursorPos - 1; i >= upFromIndex; --i)
    {
        QChar ch = text[i];

        if (ch == ',')
        {
            // Stop the search when encountering a comma
            return delimeterType;
        }
        else if (ch == ':' || ch == '.')
        {
            delimeterType = ch;
        }
    }

    // No colon or dot found before a comma
    return delimeterType;
}

bool MatchClassWorker::handleInsideFunctionParameters(void)
{
    if (true == this->matchedMethodName.isEmpty())
    {
        this->isProcessing = false;
        QMetaObject::invokeMethod(ApiModel::instance(), "signal_noHighlightFunctionParameter", Qt::QueuedConnection);
        return false;
    }

    // Get the method details from ApiModel
    const auto& methodDetails = ApiModel::instance()->getMethodDetails(this->matchedClassName, this->matchedMethodName);

    if (true == methodDetails.isEmpty())
    {
        this->isProcessing = false;
        QMetaObject::invokeMethod(ApiModel::instance(), "signal_noHighlightFunctionParameter", Qt::QueuedConnection);
        return false;
    }

    if (!methodDetails.isEmpty() && !this->isStopped)
    {
        ApiModel::instance()->closeIntellisense();
        ApiModel::instance()->showMatchedFunctionMenu(this->mouseX, this->mouseY);

        QString returns = methodDetails["returns"].toString();
        QString methodName = methodDetails["name"].toString();
        QString args = methodDetails["args"].toString();  // args should be formatted as "(type1 param1, type2 param2, ...)"
        QString description = methodDetails["description"].toString();

        QString tempTyped = this->typedInsideFunction;

        if (true == tempTyped.isEmpty())
        {
            tempTyped = this->restTyped;
        }

        if (true == tempTyped.isEmpty())
        {
            tempTyped = this->typedAfterKeyword;
        }

        if (true == tempTyped.isEmpty())
        {
            this->isProcessing = false;
            return false;
        }

        // Combine method signature
        QString fullSignature = returns + " " + methodName + args;

        // Calculate cursor position within typed arguments
        int cursorPositionInArgs = tempTyped.length();

        // Strip the enclosing parentheses from `args`
        QString strippedArgs = args.mid(1, args.length() - 2);
        if (true == strippedArgs.isEmpty())
        {
            return false;
        }

        // Split arguments by comma and keep track of indices
        QStringList params = strippedArgs.split(", ");

        // Count commas in `tempTyped` to determine the parameter index
        int commaCount = tempTyped.count(',');

        // Calculate base index just after the method name and opening parenthesis
        int currentParamStart = returns.length() + 1 + methodName.length() + 1;

        // Determine the start and end index for the parameter to be highlighted
        for (int i = 0; i < params.size(); ++i)
        {
            int paramLength = params[i].length();
            int currentParamEnd = currentParamStart + paramLength;

            // Highlight if the cursor position aligns with the parameter
            if (i == commaCount)  // Check if this is the current parameter based on commas typed
            {
                QMetaObject::invokeMethod(ApiModel::instance(), "signal_highlightFunctionParameter",
                                          Qt::QueuedConnection, Q_ARG(QString, fullSignature),
                                          Q_ARG(QString, description),
                                          Q_ARG(int, currentParamStart),
                                          Q_ARG(int, currentParamEnd));
                break;
            }

            // Move to the start of the next parameter (including comma and space)
            currentParamStart = currentParamEnd + 2;
        }

        this->typedInsideFunction.clear();
        this->typedAfterKeyword.clear();

        return true;
    }
}
