#include "matchclassworker.h"
#include "luaeditormodelitem.h"
#include "apimodel.h"

#include <QRegularExpression>
#include <QStack>

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

    if (this->matchedClassName != oldMatchedClassName)
    {
        ApiModel::instance()->setSelectedClassName(this->matchedClassName);
    }

    QString variableTyped;

    if (this->restTyped.size() > 2 && true == this->forVariable)
    {
        variableTyped = this->restTyped;
    }
    else if (true == this->restTyped.isEmpty() && false == this->typedAfterKeyword.isEmpty() && true == this->forVariable && false == this->matchedClassName.isEmpty())
    {
        variableTyped = this->typedAfterKeyword;
    }
    // Checks if a new variable or singleton Name is being typed, if there is no matched class name so far
    if (false == variableTyped.isEmpty())
    {
        qDebug() << "########variable recognized: " << variableTyped;
        QChar startChar =variableTyped.at(0);
        bool forSingleton = startChar.isUpper();
        QVariantMap variablesMap = this->luaEditorModelItem->processMatchedVariables(forSingleton, variableTyped);
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
        else
        {
            ApiModel::instance()->closeIntellisense();
        }
    }

    this->forVariable = true;

    // Only re-match variables if the menu is not shown and user is not typing something after colon, because else the auto completion would not work, because each time variables would be re-detected
    // But user typed already the next expression, which would no more match!

    if (false == this->typedInsideFunction.isEmpty() || (false == this->typedAfterKeyword.isEmpty() && true == this->forFunctionParameters))
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
                if (false == ApiModel::instance()->getSelectedClassName().isEmpty())
                {
                    ApiModel::instance()->showIntelliSenseMenu("forClass", this->matchedClassName, mouseX, mouseY);
                }
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
    if (segment.isEmpty())
    {
        this->restTyped.clear();
        this->matchedClassName.clear();
        this->matchedMethodName.clear();
        this->typedInsideFunction.clear();
    }

    if (std::abs(this->cursorPosition - this->oldCursorPosition) > 1 || !this->luaEditorModelItem->hasVariablesDetected())
    {
        this->luaEditorModelItem->detectVariables();
    }

    QString textUpToCursor;
    int lastNewlinePos = -1;
    QString currentLine;
    int lineCursorPos = -1;
    int lineIndex = -1;

    // Handle segment and text parsing
    if (segment.isEmpty())
    {
        textUpToCursor = this->currentText.left(this->cursorPosition);
        lastNewlinePos = textUpToCursor.lastIndexOf('\n');
        currentLine = textUpToCursor.mid(lastNewlinePos + 1);
        lineCursorPos = this->cursorPosition - (lastNewlinePos + 1);
        lineIndex = textUpToCursor.count('\n') + 1;
    }
    else
    {
        QString wholeTextUpToCursor = this->currentText.left(this->cursorPosition);
        lineIndex = wholeTextUpToCursor.count('\n') + 1;

        currentLine = segment;

        // Own segment area
        if (false == currentLine.contains(","))
        {
            if (false == this->containsDelimiterWithQuoteBefore(currentLine))
            {
                handleOuterSegment = false;
            }
            else
            {
                handleOuterSegment = true;
            }
        }

        lineCursorPos = currentLine.size();
    }

    QChar currentCharAtCursor = currentLine[lineCursorPos - 1];
    if (')' == currentCharAtCursor)
    {
        return currentLine;
    }

    // Bracket matching to track function parameters
    QStack<int> bracketStack;
    bool insideBrackets = false;
    int lastCommaPos = -1;
    QString localSegment;

    // Iterate over current line to process brackets and commas
    for (int i = 0; i < currentLine.length(); ++i)
    {
        QChar ch = currentLine[i];

        if (ch == '(')
        {
            bracketStack.push(i);
            insideBrackets = true; // Inside parentheses
        }
        else if (ch == ')')
        {
            if (!bracketStack.isEmpty())
            {
                bracketStack.pop();
            }
            if (bracketStack.isEmpty())
            {
                insideBrackets = false; // Exit parentheses
            }
        }
        else if (ch == ',' && bracketStack.size() == 1)
        {
            lastCommaPos = i; // Track last comma within function parameters
        }
    }

    // Handle parameters inside function call if lastCommaPos is found
    if (lastCommaPos != -1 && lastCommaPos < lineCursorPos)
    {
        // Capture the function parameter after the last comma
        localSegment = currentLine.mid(lastCommaPos + 1, lineCursorPos - lastCommaPos - 1).trimmed();

        if (false == localSegment.isEmpty() && false == bracketStack.isEmpty())
        {
            // Update typedInsideFunction to only include the function call's parameters inside parentheses
            this->typedInsideFunction = currentLine.mid(bracketStack.top(), lineCursorPos - bracketStack.top());

            // Call handleCurrentLine recursively with the extracted parameter segment
            handleOuterSegment = false;
            return this->handleCurrentLine(localSegment, handleOuterSegment);
        }
    }

    // Handle isolated segments after a delimiter with a preceding space
    for (int i = 0; i < currentLine.length(); ++i)
    {
        QChar ch = currentLine[i];

        if ((ch == ':' || ch == '.') && i > 0 && currentLine[i - 1].isSpace())
        {
            QString potentialSegment = currentLine.mid(i + 1).trimmed(); // Extract after the delimiter
            if (!potentialSegment.isEmpty())
            {
                // Check if the segment ends with a ","
                if (potentialSegment.endsWith(','))
                {
                    qDebug() << "-> Potential segment ends with a comma, skipping:" << potentialSegment;
                    break; // Skip evaluation for segments ending with a comma
                }

                qDebug() << "-> Found potential segment after space and delimiter:" << potentialSegment;

                // Extract the part after the space up to the end
                int segmentStart = potentialSegment.lastIndexOf(' ', i - 1) + 1;
                QString localSegment = potentialSegment.mid(segmentStart).trimmed();
                if (!localSegment.isEmpty())
                {
                    this->typedInsideFunction = localSegment;

                    qDebug() << "Isolated segment detected after space and delimiter:" << localSegment;
                    // Call handleCurrentLine recursively with the extracted parameter segment
                    handleOuterSegment = false;
                    return this->handleCurrentLine(localSegment, handleOuterSegment);
                }
            }
        }
    }

    if (true == insideBrackets && false == bracketStack.isEmpty())
    {
        // Get the position of the last unmatched opening parenthesis
        int unmatchedOpenBracketPos = bracketStack.top();

        // Ensure the cursor is after the unmatched opening bracket
        if (lineCursorPos > unmatchedOpenBracketPos)
        {
            // Extract the segment from the last unmatched '(' to the current cursor
            QString textWithinBrackets = currentLine.mid(unmatchedOpenBracketPos + 1, lineCursorPos - unmatchedOpenBracketPos - 1).trimmed();

            // Check if a parameter is completed by finding the last comma
            int lastCommaPos = textWithinBrackets.lastIndexOf(',');

            if (lastCommaPos != -1)
            {
                // Extract the completed parameter (from last '(' to last ',')
                this->typedInsideFunction = currentLine.mid(unmatchedOpenBracketPos + 1, lastCommaPos + 1).trimmed();
                this->forFunctionParameters = true;
                this->forVariable = false;
            }
        }
    }

    // If no parameters or unmatched parentheses, continue with the rest of the logic
    if (true == localSegment.isEmpty() && lastCommaPos != -1 && lastCommaPos < lineCursorPos)
    {
        localSegment = currentLine.mid(lastCommaPos + 1, lineCursorPos - lastCommaPos - 1).trimmed();
        if (false == localSegment.isEmpty() && false == bracketStack.isEmpty())
        {
            this->typedInsideFunction = currentLine.mid(bracketStack.top(), lineCursorPos - bracketStack.top() + 1);
            this->handleCurrentLine(localSegment, handleOuterSegment);
        }
    }

    int unmatchedOpenBracketPos = -1;
    if (false == bracketStack.isEmpty())
    {
        unmatchedOpenBracketPos = bracketStack.top();
    }
    if (true == localSegment.isEmpty() && unmatchedOpenBracketPos != -1 && currentCharAtCursor != ",")
    {
        // int segmentCursorPos = lineCursorPos + 1 - unmatchedOpenBracketPos;
        // localSegment = currentLine.mid(unmatchedOpenBracketPos, segmentCursorPos);
        localSegment = currentLine.mid(unmatchedOpenBracketPos + 1, lineCursorPos - unmatchedOpenBracketPos - 1).trimmed();

        qDebug() << "-->Calling handleCurrentLine for segment: " << localSegment;
        if (false == localSegment.isEmpty())
        {
            this->typedInsideFunction = localSegment;
            currentLine = this->handleCurrentLine(localSegment, handleOuterSegment);
        }
    }

    if (true == segment.isEmpty() && false == handleOuterSegment)
    {
        return "";
    }

    // Process text further if necessary
    QString wholeTextUpToCursor = this->currentText.left(this->cursorPosition);
    int wholeLastNewlinePos = wholeTextUpToCursor.lastIndexOf('\n');
    QString wholeCurrentLine = wholeTextUpToCursor.mid(wholeLastNewlinePos + 1);
    // Extract text up to the first unmatched parenthesis if needed
    QString leftFreeCurrentLine = currentLine;
    int upToIndex = wholeCurrentLine.lastIndexOf(currentLine);
    if (upToIndex > 0 && segment.isEmpty())
    {
        leftFreeCurrentLine = wholeCurrentLine.left(upToIndex);
        lineCursorPos -= currentLine.size();
    }

    // Split the line for further processing
    QString textToSplit = leftFreeCurrentLine.left(lineCursorPos);

    // Trim irrelevant prefixes like assignments using a regex
    // E.g. "         local boss1Defeated = AppStateManager:getGameProgressModule():"
    // -> local boss1Defeated = shall not be used
    // QRegularExpression assignmentRegex(R"(\s*(local\s+)?[a-zA-Z_][a-zA-Z0-9_]*\s*=\s*)");
    QRegularExpression assignmentRegex(R"(\s*=\s*)");
    QRegularExpressionMatch match = assignmentRegex.match(textToSplit);
    if (match.hasMatch())
    {
        textToSplit = textToSplit.mid(match.capturedEnd()); // Remove the matched prefix
    }

    // Consider brackets if necessary
    if (false == bracketStack.isEmpty() && -1 != bracketStack.top() && textToSplit.contains('('))
    {
        textToSplit = textToSplit.left(bracketStack.top() + 1);
    }

    qDebug() << "->textToSplit ---> " << textToSplit;

#if 0
    // Define a list of reserved keywords to filter
    QStringList reservedKeywords = {"if", "while", "for", "switch", "else", "return"};

    // Regular expression to match reserved keywords at the start, optionally followed by "(" or " "
    QRegularExpression keywordRegex(R"(^\s*(\b(?:if|while|for|switch|else|return)\b)(?=\s|\())");

    // Remove reserved keywords at the start of the string
    QRegularExpressionMatch keywordMatch = keywordRegex.match(textToSplit);
    if (keywordMatch.hasMatch())
    {
        // Cut off the matched reserved keyword from the start
        textToSplit = textToSplit.mid(keywordMatch.capturedLength()).trimmed();
    }

    // Split the remaining text into tokens
    QStringList tokens = textToSplit.split(QRegularExpression(R"([:.(),])"), Qt::SkipEmptyParts);

    // Remove any reserved keywords from the tokens list
    tokens.removeIf([&reservedKeywords](const QString &token) {
        return reservedKeywords.contains(token);
    });
#else

    // Define a list of reserved keywords to filter
    QStringList reservedKeywords = {"if", "while", "for", "switch", "else", "return"};

    // Regular expression to match reserved keywords at the start, optionally followed by "(" or " "
    QRegularExpression keywordRegex(R"(^\s*(\b(?:if|while|for|switch|else|return)\b)(?=\s|\())");

    // Remove reserved keywords at the start of the string
    QRegularExpressionMatch keywordMatch = keywordRegex.match(textToSplit);
    if (keywordMatch.hasMatch())
    {
        // Cut off the matched reserved keyword from the start
        textToSplit = textToSplit.mid(keywordMatch.capturedLength()).trimmed();
    }

    // Regular expression to split based on punctuation and operators, while preserving comparison operators
    QStringList tokens = textToSplit.split(QRegularExpression(R"([:.(),\s]+|==|!=|>=|<=|>|<|=)"), Qt::SkipEmptyParts);

    // Remove any reserved keywords from the tokens list
    tokens.removeIf([&reservedKeywords](const QString &token) {
        return reservedKeywords.contains(token);
    });

    // Debug: Output the resulting tokens
    qDebug() << "Filtered tokens ---> " << tokens;


#endif

    qDebug() << "->tokens ---> " << tokens;

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

                bool foundChainType = false;
                for (const auto& verticalChainTypeInfo : luaVariableInfo.verticalChainTypeList)
                {
                    if (false == verticalChainTypeInfo.horizontalChainTypes.isEmpty() && verticalChainTypeInfo.line == lineIndex)
                    {
                        // Loop through the horizontal chain types and set positions up to the cursor position
                        for (auto horizontalChainTypeInfo : verticalChainTypeInfo.horizontalChainTypes)
                        {
                            // If the variable position is less than or equal to the cursor position, the matched horizontal chain type has been found
                            if (horizontalChainTypeInfo.position <= lineCursorPos)
                            {
                                this->matchedClassName = horizontalChainTypeInfo.chainType;
                                foundChainType = true;
                                break;
                            }
                        }
                        if (true == foundChainType)
                        {
                            break;  // Exit after processing the correct line
                        }
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
                this->forVariable = false;
                this->forConstant = false;
                const auto& methods = ApiModel::instance()->getMethodsForClassName(rootClassName);

                for (const QVariant& methodVariant : methods)
                {
                    QVariantMap methodMap = methodVariant.toMap();
                    if (methodMap["name"].toString() == token)
                    {
                        this->matchedMethodName = token;
                        QString args = methodMap["args"].toString();

                        if (true == this->forFunctionParameters)
                        {
                            if (rootClassName == this->matchedClassName && "()" == args)
                            {
                                QString valueType = methodMap["valuetype"].toString();
                                if (false == this->isLuaNativeType(valueType))
                                {
                                    this->matchedClassName = valueType;
                                    rootClassName = this->matchedClassName;
                                }
                            }
                        }
                        else
                        {
                            QString valueType = methodMap["valuetype"].toString();
                            if (false == this->isLuaNativeType(valueType))
                            {
                                this->matchedClassName = valueType;
                                rootClassName = this->matchedClassName;
                            }
                            else
                            {
                                rootClassName = this->matchedClassName;
                            }
                        }

#if 0
                        if (rootClassName != this->matchedClassName)
                        {
                            rootClassName = this->matchedClassName;
                        }

                        else
                        {
                            // Does not work for this case:
                            //gameObjectTitleComponent:getOffsetOrientation():angleBetween(
                            // getOffsetOrientation = Vector3 class
                            //     angleBetween = Radian
                            //
                            //            Problem:
                            //                      methodName: angleBetween
                            //                      className must be: Vector3, instead Radian, which comes from valuetype
                            // QString valueType = methodMap["valuetype"].toString();
                            // if (false == this->isLuaNativeType(valueType))
                            // {
                            //     this->matchedClassName = methodMap["valuetype"].toString();
                            // }
                            QString valueType = methodMap["valuetype"].toString();
                            if (false == this->isLuaNativeType(valueType))
                            {
                                this->matchedClassName = methodMap["valuetype"].toString();
                                rootClassName = this->matchedClassName;
                            }
                        }
#endif

                        this->forConstant = false;
                        isMatched = true;
                        break;
                    }
                }
            }
            else if (lastDelimiter == '.')
            {
                this->forVariable = false;
                this->forConstant = true;

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

int MatchClassWorker::findPriorCommaPos(const QString& line, int cursorPos)
{
    int priorCommaPos = -1;

    // Loop through the string backward from the adjusted cursor position
    for (int i = cursorPos - 1; i >= 0; --i)
    {
        QChar ch = line[i];

        if (ch == ',')
        {
            priorCommaPos = i;
            break;
        }
    }

    // Return the position of the unmatched '(' or -1 if all brackets are balanced
    return priorCommaPos;
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

        bool valid = false;
        // Determine the start and end index for the parameter to be highlighted
        for (int i = 0; i < params.size(); ++i)
        {
            int paramLength = params[i].length();
            int currentParamEnd = currentParamStart + paramLength;

            // Highlight if the cursor position aligns with the parameter
            if (i == commaCount)  // Check if this is the current parameter based on commas typed
            {
                valid = true;
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

        if (false == valid)
        {
            QMetaObject::invokeMethod(ApiModel::instance(), "signal_noHighlightFunctionParameter", Qt::QueuedConnection);
            ApiModel::instance()->closeIntellisense();
        }

        this->typedInsideFunction.clear();
        this->typedAfterKeyword.clear();

        return true;
    }
    return false;
}
