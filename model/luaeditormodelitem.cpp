#include "luaeditormodelitem.h"
#include "apimodel.h"

#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>
#include <QRegularExpression>
#include <QRegularExpressionMatch>

LuaEditorModelItem::LuaEditorModelItem(QObject* parent)
    : QObject{parent},
    hasChanges(false),
    firstTimeContent(true),
    matchClassWorker(Q_NULLPTR),
    matchClassThread(Q_NULLPTR),
    matchMethodWorker(Q_NULLPTR),
    matchMethodThread(Q_NULLPTR),
    printToConsole(false)
{

}

LuaEditorModelItem::~LuaEditorModelItem()
{

}

void LuaEditorModelItem::setFilePathName(const QString& filePathName)
{
    if (this->filePathName == filePathName)
    {
        return;
    }
    this->filePathName = filePathName;

    Q_EMIT filePathNameChanged();
}

QString LuaEditorModelItem::getContent() const
{
    return this->content;
}

void LuaEditorModelItem::setContent(const QString& content)
{
    if (this->content == content && this->oldContent == this->content)
    {
        this->setHasChanges(false);
        return;
    }
    this->content = content;

    if (false == this->firstTimeContent)
    {
        this->setHasChanges(true);
    }
    else
    {
        this->oldContent = this->content;
    }

    if (this->content == this->oldContent)
    {
        this->setHasChanges(false);
    }

    this->firstTimeContent = false;

    Q_EMIT contentChanged();
}

QString LuaEditorModelItem::getTitle() const
{
    return this->title;
}

void LuaEditorModelItem::setTitle(const QString& title)
{
    if (this->title == title)
    {
        return;
    }
    this->title = title;

    Q_EMIT titleChanged();
}

bool LuaEditorModelItem::getHasChanges() const
{
    return this->hasChanges;
}

void LuaEditorModelItem::setHasChanges(bool hasChanges)
{
    this->hasChanges = hasChanges;

    QString tempTitle = this->title;

    if (false == tempTitle.isEmpty() && true == tempTitle.endsWith('*'))
    {
        tempTitle.chop(1);  // Removes the last character if it's '*'
    }

    if (true == this->hasChanges)
    {
        this->setTitle(tempTitle + "*");
    }
    else
    {
        this->setTitle(tempTitle);
    }

    Q_EMIT hasChangesChanged();
}

void LuaEditorModelItem::restoreContent()
{
    // If lua script has been saved, restore the content, so that it has no mor changes;
    this->oldContent = this->content;
    this->setContent(this->oldContent);

    QString tempTitle = this->title;

    if (false == tempTitle.isEmpty() && true == tempTitle.endsWith('*'))
    {
        tempTitle.chop(1);  // Removes the last character if it's '*'
    }
    this->setTitle(tempTitle);
}

void LuaEditorModelItem::openProjectFolder()
{
    // Extract the folder path from the full file path
    QFileInfo fileInfo(this->filePathName);
    QString folderPath = fileInfo.absolutePath();

    // Open the folder in the system's file explorer
    QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath));
}

QString LuaEditorModelItem::getFilePathName() const
{
    return this->filePathName;
}

QString LuaEditorModelItem::extractWordBeforeColon(const QString& currentText, int cursorPos)
{
    // Start from the cursor position and move backwards to find the last word before a colon
    int startPos = cursorPos - 1;

    // Iterate backwards from the cursor position until a delimiter is found
    while (startPos >= 0)
    {
        QChar currentChar = currentText[startPos];

        // Stop if a delimiter (":", "=", space, or line break) is found
        if (currentChar == ':' || currentChar == '=' || currentChar == ' ' || currentChar == '\n')
        {
            break;
        }

        startPos--;
    }

    // Extract the word between the delimiter and the cursor position
    QString wordBeforeColon = currentText.mid(startPos + 1, cursorPos - (startPos + 1)).trimmed();

    // Now handle case-sensitive class recognition
    // Remove "get" prefix and brackets if found
    if (wordBeforeColon.startsWith("get"))
    {
        wordBeforeColon = wordBeforeColon.mid(3);  // Remove the "get"
    }

    // Find the position of the first opening bracket '('
    int bracketPos = wordBeforeColon.indexOf('(');
    if (bracketPos != -1)
    {
        // Remove everything from the first '(' to the closing ')'
        wordBeforeColon = wordBeforeColon.left(bracketPos).trimmed();
    }

    // if (wordBeforeColon.endsWith("FromName"))
    // {
    //     wordBeforeColon = wordBeforeColon.left(wordBeforeColon.size() - 8);
    // }
    // else if (wordBeforeColon.endsWith("FromIndex"))
    // {
    //     wordBeforeColon = wordBeforeColon.left(wordBeforeColon.size() - 9);
    // }

    return wordBeforeColon;
}

QString LuaEditorModelItem::extractMethodBeforeColon(const QString& currentText, int cursorPos)
{
    // Start from the cursor position and move backwards to find the last word before a colon
    int startPos = cursorPos - 1;

    // Iterate backwards from the cursor position until a delimiter is found
    while (startPos >= 0)
    {
        QChar currentChar = currentText[startPos];

        // Stop if a delimiter (":", "=", space, or line break) is found
        if (currentChar == ':' || currentChar == '=' || currentChar == ' ' || currentChar == '\n')
        {
            break;
        }

        startPos--;
    }

    // Extract the word between the delimiter and the cursor position
    QString methodBeforeColon = currentText.mid(startPos + 1, cursorPos - (startPos + 1)).trimmed();

    // Find the position of the first opening bracket '('
    int bracketPos = methodBeforeColon.indexOf('(');
    if (bracketPos != -1)
    {
        // Remove everything from the first '(' to the closing ')'
        methodBeforeColon = methodBeforeColon.left(bracketPos).trimmed();
    }

    return methodBeforeColon;
}

QString LuaEditorModelItem::extractClassBeforeDot(const QString& currentText, int cursorPosition)
{
    // Extract the current line up to the cursor position
    QString currentLine = currentText.left(cursorPosition).section('\n', -1).trimmed();

    // Define a regular expression to match the word before the dot or colon
    QRegularExpression pattern(R"(\b(\w+)(?:\.\w+)?\s*$)"); // This captures the last word before optional dot

    // Match the pattern against the current line
    QRegularExpressionMatch match = pattern.match(currentLine);
    if (match.hasMatch())
    {
        // Return the class name found
        return match.captured(1); // Return the first captured group, which is the class name
    }

    // Return empty if no match
    return QString();
}

void LuaEditorModelItem::detectVariables(void)
{
    this->variableMap.clear();
    QStringList lines = this->content.split('\n');

    // First Pass: Detect all variables
    for (int i = 0; i < lines.size(); ++i)
    {
        QString line = lines[i].trimmed();
        if (line.isEmpty())
        {
            continue;
        }

        detectLocalVariables(line, i + 1);
        detectGlobalVariables(line, i + 1);
    }

    bool needThirdRound = false;

    do
    {
        // Second Pass: Infer variable types based on assignments and method calls
        for (int i = 0; i < lines.size(); ++i)
        {
            QString line = lines[i].trimmed();

            if (true == line.isEmpty())
            {
                continue;
            }
            if (true == this->isCommentOnlyLine(line))
            {
                continue;
            }

            // Split the line into multiple statements using semicolon ";"
            QStringList statements = line.split(';', Qt::SkipEmptyParts);

            // Process each statement separately
            for (const QString& statement : statements)
            {
                // Handle method chain assignments
                handleCastAssignment(statement, i + 1);
                // handleMethodCallAssignment(statement, i + 1);
                handleSimpleAssignment(statement, i + 1);
                // handleMethodChainAssignment(statement, i + 1);
                handleMethodChain(statement, i + 1);

                handleTableAccess(statement, i + 1);
                // Handle for and while loops (No more necessary)
                // handleLoop(statement, i + 1, lines);
            }
        }

        if (true == needThirdRound)
        {
            break;
        }

        // Iterate over all elements in variableMap
        for (auto it = this->variableMap.begin(); it != this->variableMap.end(); ++it)
        {
            // Check if the type is empty or "nil"
            if (it.value().type.isEmpty() || it.value().type == "nil")
            {
                // If this is the case, not all types for variables had been found, because maybe of ordering issue:
                // E.g. a function is used with variables, but the function comes before the method where dependent variables are assigned
                // So re-check in a third round once
                needThirdRound = true;
                break;
            }
        }
    } while(true == needThirdRound);

    // Additional passes for other types of assignments or specific handling if needed
}

void LuaEditorModelItem::detectLocalVariables(const QString& line, int lineNumber)
{
    // Detect local variables
    QRegularExpression localVarRegex(R"(local\s+(\w+)\s*=)");
    QRegularExpressionMatch match = localVarRegex.match(line);

    if (match.hasMatch())
    {
        QString varName = match.captured(1);
        this->variableMap[varName] = LuaVariableInfo{varName, "", lineNumber, "local"};
    }
}

void LuaEditorModelItem::detectGlobalVariables(const QString& line, int lineNumber)
{
    // Detect global assignments
    QRegularExpression globalVarRegex(R"((\w+)\s*=)");
    QRegularExpressionMatch match = globalVarRegex.match(line);

    if (match.hasMatch() && !line.startsWith("local"))
    {
        QString varName = match.captured(1);
        this->variableMap[varName] = LuaVariableInfo{varName, "", lineNumber, "global"};
    }
}

void LuaEditorModelItem::handleCastAssignment(const QString& statement, int lineNumber)
{
    // Handle the detection of "cast" and assignment of type
    int castIndex = statement.indexOf("cast");
    if (castIndex != -1)
    {
        // We found the word "cast", so now we need to extract the class name and variable
        int classNameStart = castIndex + 4;  // "cast" is 4 characters long

        // Find the start of the class name (skip the parentheses)
        int openParenIndex = statement.indexOf('(', classNameStart);
        int closeParenIndex = statement.indexOf(')', openParenIndex);

        QString className = statement.mid(classNameStart, openParenIndex - classNameStart).trimmed();

        // Now go backward to find the variable before the "="
        int equalsIndex = statement.lastIndexOf('=', castIndex);
        QString varName = "";

        if (equalsIndex != -1)
        {
            // Find the variable name before the "=" by iterating backward
            int varNameEnd = equalsIndex - 1;

            while (varNameEnd >= 0 && statement[varNameEnd].isSpace())
            {
                varNameEnd--;
            }

            int varNameStart = varNameEnd;
            while (varNameStart >= 0 && (statement[varNameStart].isLetterOrNumber() || statement[varNameStart] == '_'))
            {
                varNameStart--;
            }

            varName = statement.mid(varNameStart + 1, varNameEnd - varNameStart).trimmed();
        }

        if (!varName.isEmpty() && !className.isEmpty())
        {
            // Assign the detected class name as the type for the variable
            this->variableMap[varName] = LuaVariableInfo{varName, className, lineNumber, this->variableMap[varName].scope};
        }
    }
}

void LuaEditorModelItem::handleMethodCallAssignment(const QString& statement, int lineNumber)
{
    // Handle method call assignments and infer variable types
    QRegularExpression assignmentRegex(R"((\w+)\s*=\s*(\w+):(\w+)\(\))");
    QRegularExpressionMatch match = assignmentRegex.match(statement);

    if (match.hasMatch())
    {
        QString leftVar = match.captured(1);  // The variable being assigned
        QString objectVar = match.captured(2);  // The object being called
        QString methodName = match.captured(3);  // The method being invoked

        // Check if the objectVar already exists in the variableMap
        if (this->variableMap.contains(objectVar) && !this->variableMap[objectVar].type.isEmpty())
        {
            QString type = this->variableMap[objectVar].type;

            // Set the selected class name in ApiModel for method lookup
            ApiModel::instance()->setSelectedClassName(this->variableMap[objectVar].type);

            // Fetch the methods for this class
            QVariantList methods = ApiModel::instance()->getMethodsForSelectedClass();

            // Find the method in the class's methods and get its return type
            for (const QVariant& methodVariant : methods)
            {
                QVariantMap methodMap = methodVariant.toMap();
                QString currentMethodName = methodMap["name"].toString();

                if (currentMethodName == methodName)
                {
                    // Removes the brackets around the return type
                    QString returnType = methodMap["returns"].toString().remove(QRegularExpression("[()]"));

                    // Assign the return type to the leftVar
                    if (!returnType.isEmpty())
                    {
                        this->variableMap[leftVar] = LuaVariableInfo{leftVar, returnType, lineNumber, this->variableMap[leftVar].scope};
                        break;
                    }

                    break;
                }
            }
        }
    }
}

void LuaEditorModelItem::handleSimpleAssignment(const QString& statement, int lineNumber)
{
    // Handle simple assignments (no colons) (e.g., temp2 = temp), so temp2 should exactly be of type temp
    QRegularExpression simpleAssignmentRegex(R"((\w+)\s*=\s*(\w+)\s*;?)");
    QRegularExpressionMatch simpleMatch = simpleAssignmentRegex.match(statement);

    if (simpleMatch.hasMatch())
    {
        QString leftVar = simpleMatch.captured(1);  // Variable being assigned
        QString rightVar = simpleMatch.captured(2);  // Variable from which to inherit type

        // Ensure this is a "naked" assignment (no colons involved)
        if (!statement.contains(":") && this->variableMap.contains(rightVar))
        {
            // Inherit the type from the right-hand variable
            QString rightVarType = this->variableMap[rightVar].type;

            if (!rightVarType.isEmpty())
            {
                this->variableMap[leftVar] = LuaVariableInfo{leftVar, rightVarType, lineNumber, this->variableMap[leftVar].scope};
            }
        }
    }
}

bool LuaEditorModelItem::isCommentOnlyLine(const QString& line)
{
    // Regular expression to match lines starting with optional spaces followed by "--"
    QRegularExpression commentOnlyRegex(R"(^\s*--)");
    return commentOnlyRegex.match(line).hasMatch();
}

void LuaEditorModelItem::handleMethodChainAssignment(const QString& statement, int lineNumber)
{
    // QRegularExpression assignmentChainRegex(R"((\w+)\s*=\s*(\w+(:\w+\(\))+))");
    // Handle method chain assignment (e.g., variable = object:method1():method2())
    // Or even:
    // Handle method chain assignment (e.g., variable = object:method1():method2().x)
    QRegularExpression assignmentChainRegex(R"((\w+)\s*=\s*(\w+(:\w+\(\))+(?:\.\w+)?))");
    QRegularExpressionMatch match = assignmentChainRegex.match(statement);
    if (match.hasMatch())
    {
        QString assignedVar = match.captured(1);  // Variable on the left-hand side
        QString objectVar = match.captured(2);    // Object with method chain on the right-hand side

        // Resolve the type of the object variable through method chain
        QString resolvedType = resolveMethodChainType(objectVar);
        if (!resolvedType.isEmpty())
        {
            this->variableMap[assignedVar] = LuaVariableInfo{assignedVar, resolvedType, lineNumber + 1, this->variableMap[assignedVar].scope};
            // continue;
            return;
        }
    }
}

QString LuaEditorModelItem::resolveMethodChainType(const QString& objectVar)
{
    // Split object:method chain by ":" and then handle any dot properties
    QStringList parts = objectVar.split(QRegularExpression("(:|\\.)"), Qt::SkipEmptyParts);

    if (parts.isEmpty())
    {
        return "";
    }

    QString baseVar = parts.first();  // Base variable
    QString currentType = this->variableMap.contains(baseVar) ? this->variableMap[baseVar].type : "";

    if (currentType.isEmpty())
    {
        return "";  // No known type for the base variable
    }

    for (int i = 1; i < parts.size(); ++i)
    {
        QString part = parts[i];

        // If the part is a method (ends with parentheses), resolve its return type
        if (part.endsWith("()"))
        {
            QString methodName = part.left(part.size() - 2);  // Remove "()" from method name

            // Get methods available for the current type
            ApiModel::instance()->setSelectedClassName(currentType);
            QVariantList methods = ApiModel::instance()->getMethodsForSelectedClass();

            bool methodFound = false;
            for (const QVariant& methodVariant : methods)
            {
                QVariantMap methodMap = methodVariant.toMap();
                if (methodMap["name"].toString() == methodName)
                {
                    currentType = methodMap["returns"].toString().remove('(').remove(')');
                    methodFound = true;
                    break;
                }
            }

            if (!methodFound)
            {
                return "";  // If method not found, stop processing
            }
        }
        else
        {
            // No property handling, just set type to 'value', so no further recognition
            currentType = "value";
            return currentType;
        }
    }

    return currentType;  // Return the resolved type of the last property or method
}


void LuaEditorModelItem::handleMethodChain(const QString& statement, int lineNumber)
{
    // Handle chain of methods: e.g.
    // local main = AppStateManager:getController():getFromId(MAIN__ID);
    // So first determine the AppStateManager -> which is a class and getController is its method. So the "returns" would deliver "Controller".
    // So this is the new class to look further for its method via ":". So its method would be: "getFromId", which its "returns" would deliver "".
    // And since this is the last part in the statement, the "main" could be added to the variable map as type "".
    // QRegularExpression chainAssignmentRegex(R"(\s*(\w+)\s*=\s*(.+))");
    // QRegularExpression assignmentOrMethodChainRegex(
      //   R"((?:(\w+)\s*=\s*)?(\w+(:\w+\(\))+(?:\.\w+)?))");

    QRegularExpression assignmentOrMethodChainRegex(
        R"((?:(\w+)\s*=\s*)?(\w+(:\w+\((?:[^()]*|'[^']*'|"[^"]*")\))+(?:\.\w+)?))");

    QRegularExpressionMatch match = assignmentOrMethodChainRegex.match(statement);
    if (match.hasMatch())
    {
        QString variableName = match.captured(1);  // e.g., "main"
        QString expression = match.captured(2);    // e.g., "AppStateManager:getController():getFromId(MAIN__ID)"

        // Split the expression by ":" to get the method chain
        QStringList methodChain = expression.split(':');
        if (false == methodChain.isEmpty())
        {
            // Initial class or variable name
            QString baseClassOrVar = methodChain[0].trimmed();

            // Determine if the base is a known variable or a class
            QString currentType = variableMap.contains(baseClassOrVar)
                                      ? variableMap[baseClassOrVar].type
                                      : baseClassOrVar;  // Start with base class if it's a known type

            // Iterate through the method chain
            for (int i = 1; i < methodChain.size(); ++i)
            {
                QString methodCall = methodChain[i].split('(').first().trimmed();  // Extract method name

                if (currentType.isEmpty())
                {
                    if (true == this->printToConsole)
                    {
                        qDebug() << "Cannot resolve type for base class/variable" << baseClassOrVar;
                    }
                    break;
                }

                // Use ApiModel to get method details
                QVariantMap methodDetails = ApiModel::instance()->getMethodDetails(currentType, methodCall);
                if (methodDetails.isEmpty())
                {
                    if (true == this->printToConsole)
                    {
                        qDebug() << "Method" << methodCall << "not found in class" << currentType;
                    }
                    break;
                }

                // Get the return type for the next step
                currentType = methodDetails["returns"].toString();
                if (currentType.isEmpty())
                {
                    if (true == this->printToConsole)
                    {
                        qDebug() << "Method" << methodCall << "has no return type information";
                    }
                    break;
                }

                if (true == this->printToConsole)
                {
                    qDebug() << "Method" << methodCall << "returns type" << currentType;
                }
            }

            // Add the variable with the resolved type to variableMap
            if (!currentType.isEmpty())
            {
                // this->handleTableTypes(variableName, currentType, lineNumber);

                bool alreadyExisting = false;
                // If it has already a type, to not overwrite it
                if (this->variableMap.contains(variableName))
                {
                    if (false == this->variableMap[variableName].type.isEmpty())
                    {
                        alreadyExisting = true;
                    }
                }
                if (false == alreadyExisting)
                {
                    LuaVariableInfo varInfo;

                    bool hasChainType = false;
                    ChainTypeInfo chainTypeInfo;
                    if (true == variableName.isEmpty())
                    {
                        variableName = baseClassOrVar;
                        chainTypeInfo.line = lineNumber;
                        chainTypeInfo.chainType = currentType;

                        varInfo.chainTypeList.append(chainTypeInfo);
                        hasChainType = true;
                    }

                    if (!this->variableMap.contains(variableName))
                    {
                        varInfo.name = variableName;
                        if (false == hasChainType)
                        {
                            varInfo.type = currentType;
                        }
                        varInfo.line = lineNumber;
                        varInfo.scope = this->variableMap[variableName].scope;
                        this->variableMap[variableName] = varInfo;
                        if (true == this->printToConsole)
                        {
                            qDebug() << "Added variable" << variableName << "with type" << currentType << "to variableMap";
                        }
                    }
                    else
                    {
                        if (true == hasChainType)
                        {
                            this->variableMap[variableName].chainTypeList.append(chainTypeInfo);
                        }
                        else
                        {
                            this->variableMap[variableName].type = currentType;
                        }
                    }
                }
            }
        }
    }
}

void LuaEditorModelItem::handleTableTypes(const QString& variableName, const QString& statement, int lineNumber)
{
    // Regex to match all types inside square brackets (e.g., [number], [42], [GameObject])
    QRegularExpression dimensionRegex(R"(\[([^\]]+)\])");
    QRegularExpressionMatchIterator dimensionMatches = dimensionRegex.globalMatch(statement);

    QStringList extractedTypes;
    while (dimensionMatches.hasNext())
    {
        QRegularExpressionMatch match = dimensionMatches.next();
        extractedTypes.append(match.captured(1)); // Append the type or index inside the brackets
    }

    // Handle the extracted types for the variable
    if (!extractedTypes.isEmpty())
    {
        for (int i = 0; i < extractedTypes.size(); ++i)
        {
            QString currentType = extractedTypes[i];

            if ("void" == currentType || "string" == currentType || "number" == currentType || "boolean" == currentType)
            {
                continue;
            }

            this->variableMap[variableName] = LuaVariableInfo{variableName, currentType, lineNumber, "local"};
        }
    }
}

void LuaEditorModelItem::handleTableAccess(const QString& statement, int lineNumber)
{
    // Handle complex type index access (e.g., positionAndNormal[0])
    // QRegularExpression tableAccessRegex(R"(local\s+(\w+)\s*=\s*(\w+)\[(\d+)\])");
    QRegularExpression tableAccessRegex(R"(local\s+(\w+)\s*=\s*(\w+)\[([^\]]+)\])");
    QRegularExpressionMatch tableAccessMatch = tableAccessRegex.match(statement);

    if (tableAccessMatch.hasMatch())
    {
        QString leftVar = tableAccessMatch.captured(1);   // Variable being assigned
        QString rightVar = tableAccessMatch.captured(2);  // Source table variable
        int index = tableAccessMatch.captured(3).toInt(); // Access index

        // Check if rightVar is a known table with complex type
        if (this->variableMap.contains(rightVar))
        {
            LuaVariableInfo sourceInfo = this->variableMap[rightVar];
            if (!sourceInfo.type.isEmpty() && sourceInfo.type.startsWith("Table"))
            {
                // Extract subtypes for the table in a generic way
                QString valueType = sourceInfo.type;
                QStringList subTypes;
                QRegularExpression subTypeRegex(R"(\[(\w+)\])");
                QRegularExpressionMatchIterator it = subTypeRegex.globalMatch(valueType);

                while (it.hasNext())
                {
                    QRegularExpressionMatch subTypeMatch = it.next();
                    QString subType = subTypeMatch.captured(1); // Extract each subtype
                    if ("void" == subType || "string" == subType || "number" == subType || "boolean" == subType)
                    {
                        continue;
                    }
                    this->variableMap[leftVar] = LuaVariableInfo{leftVar, subType, lineNumber, "local"};
                }
            }
        }
    }
}

void LuaEditorModelItem::handleLoop(const QString& statement, int lineNumber, const QStringList& lines)
{
    // Handle for-loop variables (e.g., for i = 0, #nodes do ...)
    QRegularExpression loopVarRegex(R"(for\s+(\w+)\s*=\s*0\s*,\s*#(\w+))");
    QRegularExpressionMatch match = loopVarRegex.match(statement);

    if (match.hasMatch())
    {
        QString loopVarName = match.captured(1);  // e.g., "i"
        QString arrayVarName = match.captured(2);  // e.g., "nodes"

        // Assign "number" type to loop variable
        this->variableMap[loopVarName] = LuaVariableInfo{loopVarName, "number", lineNumber, "local"};

        // Check if arrayVarName is a known table
        if (this->variableMap.contains(arrayVarName))
        {
            QString arrayType = this->variableMap[arrayVarName].type;
            if (arrayType.startsWith("Table[number][") && arrayType.endsWith("]"))
            {
                // Extract element type from "Table[number][ElementType]"
                QString elementType = arrayType.mid(14, arrayType.length() - 15);

                // Allow access to elements, e.g., nodeGameObjects[i]
                QString indexedAccessRegex = QString(R"(%1\[%2\])").arg(arrayVarName, loopVarName);
                QRegularExpression indexedAccess(indexedAccessRegex);

                for (int j = lineNumber; j < lines.size(); ++j)
                {
                    QString line = lines[j].trimmed();

                    // Check for direct usage of nodeGameObjects[i]
                    if (indexedAccess.match(line).hasMatch())
                    {
                        // Add nodeGameObjects[i] to variableMap directly
                        QString indexedVariableName = QString("%1[%2]").arg(arrayVarName, loopVarName);
                        this->variableMap[indexedVariableName] = LuaVariableInfo{indexedVariableName, elementType, j + 1, "local"};

                        // Check if it's being assigned to another variable (e.g., local x = nodeGameObjects[i])
                        // QRegularExpression localVarAssignRegex(R"(local\s+(\w+)\s*=\s*%1\[%2\])".arg(arrayVarName, loopVarName));
                        QString localVarAssignRegexPattern = QString(R"(local\s+(\w+)\s*=\s*%1\[%2\])").arg(arrayVarName, loopVarName);
                        QRegularExpression localVarAssignRegex(localVarAssignRegexPattern);
                        QRegularExpressionMatch indexedMatch = localVarAssignRegex.match(line);

                        if (indexedMatch.hasMatch())
                        {
                            QString localVar = indexedMatch.captured(1);
                            this->variableMap[localVar] = LuaVariableInfo{localVar, elementType, j + 1, "local"};
                        }
                    }
                }
            }
        }
    }
    else
    {
        // Checks if its a while loop
        // Handle `while` loop variables (e.g., while i <= #nodes do ...)
        QRegularExpression whileLoopRegex(R"(while\s+(\w+)\s*<=\s*#(\w+))");
        QRegularExpressionMatch whileMatch = whileLoopRegex.match(statement);

        if (whileMatch.hasMatch())
        {
            QString loopVarName = whileMatch.captured(1);  // e.g., "i"
            QString arrayVarName = whileMatch.captured(2);  // e.g., "nodeGameObjects"

            if (this->variableMap.contains(arrayVarName))
            {
                QString arrayType = this->variableMap[arrayVarName].type;
                if (arrayType.startsWith("Table[number][") && arrayType.endsWith("]"))
                {
                    QString elementType = arrayType.mid(14, arrayType.length() - 15);

                    QString indexedAccessRegex = QString(R"(%1\[%2\])").arg(arrayVarName, loopVarName);
                    QRegularExpression indexedAccess(indexedAccessRegex);

                    for (int j = lineNumber; j < lines.size(); ++j)
                    {
                        QString line = lines[j].trimmed();

                        if (indexedAccess.match(line).hasMatch())
                        {
                            QString indexedVariableName = QString("%1[%2]").arg(arrayVarName, loopVarName);
                            this->variableMap[indexedVariableName] = LuaVariableInfo{indexedVariableName, elementType, j + 1, "local"};

                            QString localVarAssignRegexPattern = QString(R"(local\s+(\w+)\s*=\s*%1\[%2\])").arg(arrayVarName, loopVarName);
                            QRegularExpression localVarAssignRegex(localVarAssignRegexPattern);
                            QRegularExpressionMatch indexedMatch = localVarAssignRegex.match(line);

                            if (indexedMatch.hasMatch())
                            {
                                QString localVar = indexedMatch.captured(1);
                                this->variableMap[localVar] = LuaVariableInfo{localVar, elementType, j + 1, "local"};
                            }
                        }

                        if (line == "end")
                        {
                            break;
                        }
                    }
                }
            }
        }
    }
}

LuaEditorModelItem::LuaVariableInfo LuaEditorModelItem::getClassForVariableName(const QString& variableName)
{
    LuaEditorModelItem::LuaVariableInfo luaVariableInfo;
    if (this->variableMap.contains(variableName))
    {
        luaVariableInfo = this->variableMap[variableName];
    }

    return luaVariableInfo;
}

void LuaEditorModelItem::resetMatchedClass()
{
    this->matchedClassName.clear();
    this->closeIntellisense();
}

void LuaEditorModelItem::startIntellisenseProcessing(bool forConstant, const QString& currentText, const QString& textAfterKeyword, int cursorPos, int mouseX, int mouseY, bool forMatchedFunctionMenu)
{
    // If there's already a worker, interrupt its process
    if (Q_NULLPTR != this->matchClassWorker)
    {
        // Stop the ongoing process
        this->matchClassWorker->stopProcessing();

        this->matchClassWorker->setParameters(forConstant, currentText, textAfterKeyword, cursorPos, mouseX, mouseY, forMatchedFunctionMenu);

        // Emit a request to process in the worker thread
        Q_EMIT this->matchClassWorker->signal_requestProcess();
    }
    else
    {
        // Create a new worker instance
        this->matchClassWorker = new MatchClassWorker(this, forConstant, currentText, textAfterKeyword, cursorPos, mouseX, mouseY, forMatchedFunctionMenu);

        // Create and start a new thread for processing
        this->matchClassThread = QThread::create([this]
             {
                // Connect the signal to the process method
                QObject::connect(this->matchClassWorker, &MatchClassWorker::signal_requestProcess, this->matchClassWorker, &MatchClassWorker::process);
                QObject::connect(this->matchClassWorker, &MatchClassWorker::signal_deliverData, this, [this](const QString& matchedClassName, const QString& matchedMethodName, const QString& textAfterKeyword, int cursorPos, int mouseX, int mouseY)
                    {
                        // Special case: If match class worker is finished with detecting variables etc. it delivers its data to the match method worker, which does work with the data
                        this->matchedClassName = matchedClassName;
                        this->startMatchedFunctionProcessing(matchedMethodName, cursorPos, mouseX, mouseY);
                    }, Qt::QueuedConnection);
                this->matchClassWorker->process();
             });

        QObject::connect(matchClassWorker, &MatchClassWorker::finished, this, [this]() {
            // Cleanup and reset when finished
            delete this->matchClassWorker; // Delete the worker instance when done
            this->matchClassWorker = Q_NULLPTR; // Reset the worker pointer
            this->matchClassThread->quit();  // Quit the thread
        });

        QObject::connect(this->matchClassWorker, &MatchClassWorker::finished, this->matchClassWorker, &MatchClassWorker::deleteLater);
        QObject::connect(this->matchClassThread, &QThread::finished, this->matchClassThread, &QThread::deleteLater);

        // Start processing
        this->matchClassThread->start();
    }
}

void LuaEditorModelItem::closeIntellisense()
{
    ApiModel::instance()->closeIntellisense();
}

void LuaEditorModelItem::startMatchedFunctionProcessing(const QString& textAfterKeyword, int cursorPos, int mouseX, int mouseY)
{
    // If no matched class so far (e.g. user typed in the middle of the line '(', so first intellisense must be processed
    if (true == this->matchedClassName.isEmpty())
    {
        // Start for matchedFunction (true flag, so that no intellisense will be shown)
        this->startIntellisenseProcessing(false, this->content, "", cursorPos, mouseY, mouseY, true);
    }

    // If there's already a worker, interrupt its process
    if (Q_NULLPTR != this->matchMethodWorker)
    {
        // Stop the ongoing process
        this->matchMethodWorker->stopProcessing();

        // if intelliseSense processing prior started, there is so far no typed text after keyword, but in the past it has been already set for the method worker, so is it in this special case
        if (true == textAfterKeyword.isEmpty())
        {
            this->matchMethodWorker->setParameters(this->matchedClassName, this->matchMethodWorker->getTypedAfterKeyword(), cursorPos, mouseX, mouseY);
        }
        else
        {
             this->matchMethodWorker->setParameters(this->matchedClassName, textAfterKeyword, cursorPos, mouseX, mouseY);
        }

        if (!textAfterKeyword.endsWith('.') && !textAfterKeyword.endsWith(':'))
        {
            // Emit a request to process in the worker thread
            Q_EMIT this->matchMethodWorker->signal_requestProcess();
        }
    }
    else
    {
        // Create a new worker instance
        this->matchMethodWorker = new MatchMethodWorker(this, this->matchedClassName, textAfterKeyword, cursorPos, mouseX, mouseY);

        // Create and start a new thread for processing
        this->matchMethodThread = QThread::create([this]
                                           {
                                               // Connect the signal to the process method
                                               QObject::connect(this->matchMethodWorker, &MatchMethodWorker::signal_requestProcess, this->matchMethodWorker, &MatchMethodWorker::process);
                                               this->matchMethodWorker->process();
                                           });

        QObject::connect(matchMethodWorker, &MatchMethodWorker::finished, this, [this]() {
            // Cleanup and reset when finished
            delete matchMethodWorker; // Delete the worker instance when done
            matchMethodWorker = Q_NULLPTR; // Reset the worker pointer
            this->matchMethodThread->quit();  // Quit the thread
        });

        QObject::connect(matchMethodWorker, &MatchMethodWorker::finished, matchMethodWorker, &MatchMethodWorker::deleteLater);
        QObject::connect(this->matchMethodThread, &QThread::finished, this->matchMethodThread, &QThread::deleteLater);

        // Start processing
        this->matchMethodThread->start();
    }
}

void LuaEditorModelItem::closeMatchedFunction()
{
    ApiModel::instance()->closeMatchedFunction();
}
