#include "luascriptadapter.h"

#include <QDebug>

#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QSettings>
#include <QRegularExpression>
#include <QRegularExpressionMatch>

namespace
{
    // Helper function to clean up the input string
    QString cleanString(const QString& str)
    {
        return str.trimmed().remove("\"").remove(",");
    }

    QString cleanString2(const QString& str)
    {
        QString resultString = str;
        // Remove the last comma if it exists
        if (resultString.endsWith(","))
        {
            resultString.chop(1); // Remove the last character
        }
        return resultString.trimmed().remove("\"");
    }
}

LuaScriptAdapter::LuaScriptAdapter(QObject* parent)
    : QObject{parent},
    luaApiCreatedIntially(false)
{

}

LuaScriptAdapter::~LuaScriptAdapter()
{
    for (const auto& luaScript : this->luaScripts)
    {
        luaScript->deleteLater();
    }
    this->luaScripts.clear();
}

QPair<int, LuaEditorModelItem*> LuaScriptAdapter::createLuaScript(const QString& filePathName)
{
    int index = this->findLuaScript(filePathName);

    if (index >= 0)
    {
        return qMakePair(index, Q_NULLPTR);
    }

    LuaScript* luaScript = new LuaScript(filePathName);
    bool success = luaScript->loadScriptFromFile(filePathName);
    if (false == success)
    {
        return qMakePair(-1, Q_NULLPTR);
    }
    this->luaScripts.append(luaScript);

    connect(luaScript, &LuaScript::signal_syntaxCheckResult, this, [this, luaScript](bool valid, int line, int start, int end, const QString& message)
            {
                         Q_EMIT signal_syntaxCheckResult(luaScript->getFilePathName(), valid, line, start, end, message);
            });

    connect(luaScript, &LuaScript::signal_runtimeError, this, [this, luaScript](bool valid, int line, int start, int end, const QString& message)
            {
                Q_EMIT signal_runtimeError(luaScript->getFilePathName(), valid, line, start, end, message);
            });

    LuaEditorModelItem* luaEditorModelItem = new LuaEditorModelItem(this);

    luaEditorModelItem->setFilePathName(filePathName);
    luaEditorModelItem->setContent(luaScript->getContent());

    QFileInfo fileInfo(filePathName);
    QString fileName = fileInfo.completeBaseName(); // Extracts filename without extension
    luaEditorModelItem->setTitle(fileName);

    // Load api, if path has been already stored, once
    if (false == this->luaApiCreatedIntially)
    {
        QSettings settings("NOWA", "NOWALuaScript");

        // Retrieve the saved file path (if it exists), otherwise return an empty string
        QString savedFilePath = settings.value("LuaApiFilePath", QString()).toString();

        if (false == savedFilePath.isEmpty())
        {
            Q_EMIT signal_luaApiPrepareInitial(savedFilePath, true);
        }

        this->luaApiCreatedIntially = true;
    }

    return qMakePair(-1, luaEditorModelItem);
}

bool LuaScriptAdapter::removeLuaScript(const QString& filePathName)
{
    // Check if the luaScript with this filePathName exists in the QHash
    int index = this->findLuaScript(filePathName);
    if (-1 == index)
    {
        qWarning() << "LuaScript with filePathName" << filePathName << "not found.";
        return false;
    }

    // Retrieve the LuaScript instance
    LuaScript* luaScript = this->luaScripts.at(index);

    if (Q_NULLPTR == luaScript)
    {
        qWarning() << "LuaScript for filePathName" << filePathName << "is null.";
        return false;
    }

    // Clean up connections to the LuaScript and stop the thread
    disconnect(luaScript, Q_NULLPTR, this, Q_NULLPTR);

    // Now that the LuaScript has been cleaned up, remove it from the QHash
    this->luaScripts.takeAt(index);

    luaScript->deleteLater();

    qDebug() << "LuaScript with filePathName" << filePathName << "has been removed.";

    return true;
}

bool LuaScriptAdapter::saveLuaScript(const QString& filePathName, const QString& content)
{
    // Check if the luaScript with this filePathName exists in the QHash
    int index = this->findLuaScript(filePathName);
    if (-1 == index)
    {
        qWarning() << "LuaScript with filePathName" << filePathName << "not found.";
        return false;
    }

    // Retrieve the LuaScript instance
    LuaScript* luaScript = this->luaScripts.at(index);

    if (Q_NULLPTR == luaScript)
    {
        qWarning() << "LuaScript for filePathName" << filePathName << "is null.";
        return false;
    }

    bool success = luaScript->saveLuaScript(content);

    if (true == success)
    {
        Q_EMIT signal_luaScriptSaved();
    }

    return success;
}

void LuaScriptAdapter::checkSyntax(const QString& filePathName, const QString& luaCode)
{
    int index = this->findLuaScript(filePathName);
    if (-1 != index)
    {
        LuaScript* luaScript = this->luaScripts.at(index);
        luaScript->checkSyntax(luaCode);
    }
}

bool LuaScriptAdapter::sendLuaScriptRuntimeError(const QString& filePathName, const QString& errorMessage, int line, int start, int end)
{
    int index = this->findLuaScript(filePathName);
    if (-1 != index)
    {
        LuaScript* luaScript = this->luaScripts.at(index);
        luaScript->checkRuntimeError(errorMessage, line, start, end);
        return true;
    }
    return false;
}

QMap<QString, LuaScriptAdapter::ClassData> LuaScriptAdapter::prepareLuaApi(const QString& filePathName, bool parseSilent)
{
    QMap<QString, ClassData> apiData;

    QFile file(filePathName);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QString message = "Unable to open file: " + filePathName;
        qWarning() << message;
        Q_EMIT signal_luaApiPrepareResult(false, false, message);
        return apiData;
    }

    QTextStream in(&file);
    QString currentClass;    // Class name
    ClassData currentClassData; // Current class data being populated
    QString currentMethod;    // Method name
    MethodData currentMethodData; // Current method data
    bool inChildsSection = false; // Flag to check if we are in the "childs" section
    bool classOpened = false; // Flag to check if a class has been opened
    bool methodOpened = false; // Flag to check if a method has been opened

    QRegularExpression classDefRegex(R"((\w+)\s*=\s*)");
    QRegularExpression methodNameRegex(R"(^\s*(\w+)\s*=\s*$)");  // Matches method name followed by the opening brace in the next line

    while (!in.atEnd())
    {
        QString line = in.readLine().trimmed();

        // Skip empty lines or comments
        if (line.isEmpty() || line.startsWith("--"))
        {
            continue;
        }

        // Skip the first 'return' statement
        if (line.contains("return") && !line.contains("returns"))
        {
            continue;
        }

        // Detect class definitions: Match lines containing the class name and "="
        if (!classOpened)
        {
            QRegularExpressionMatch match = classDefRegex.match(line);
            if (match.hasMatch())
            {
                currentClass = match.captured(1);  // Capture the class name
                currentClassData = ClassData();  // Initialize new class data
                classOpened = true; // Mark class as opened
                continue; // Proceed to check the next line for the opening brace
            }
        }

        // Handle class attributes like type, description, inherits
        if (classOpened && !inChildsSection)
        {
            if (line.contains("=") && !line.contains("{"))
            {
                QStringList parts = line.split("=");
                QString key = cleanString(parts[0]);
                QString value = cleanString(parts[1]);

                if (key == "type")
                {
                    currentClassData.type = value;
                }
                else if (key == "description")
                {
                    currentClassData.description = value;
                }
                else if (key == "inherits")
                {
                    currentClassData.inherits = value;
                }
            }
        }

        // If a class was just opened, check the next line for the opening brace
        if (classOpened)
        {
            if (line.contains("{"))
            {
                continue; // Proceed to the next line
            }

            // Check if we are entering the childs section
            if (line.contains("childs"))
            {
                inChildsSection = true; // Set flag for child methods
                continue; // Proceed to the next line
            }

            // If in the child section, look for attributes of the methods
            if (inChildsSection)
            {
                QRegularExpressionMatch methodMatch = methodNameRegex.match(line);

                if (methodMatch.hasMatch())
                {
                    currentMethod = methodMatch.captured(1); // Capture the method name
                    currentMethodData = MethodData(); // New method data
                    methodOpened = true; // Set flag for when brace will be detected
                    continue;
                }

                // Detect attributes for methods (type, description, args, returns, valuetype)
                if (methodOpened && line.contains("=") && !line.contains("{"))
                {
                    QStringList parts = line.split("=");
                    QString key = cleanString(parts[0]);
                    QString value = cleanString(parts[1]);

                    if (key == "type")
                    {
                        currentMethodData.type = value;
                    }
                    else if (key == "description")
                    {
                        currentMethodData.description = value;
                    }
                    else if (key == "args")
                    {
                        currentMethodData.args = cleanString2(parts[1]);
                    }
                    else if (key == "returns")
                    {
                        QString cleanedValue = value;
                        cleanedValue.replace(QRegularExpression("^\\(|\\)$"), "");
                        currentMethodData.returns = cleanedValue.trimmed();

                        if (currentMethodData.returns == "nil")
                        {
                            currentMethodData.returns = "void";
                        }
                    }
                    else if (key == "valuetype")
                    {
                        currentMethodData.valuetype = value;
                    }
                }

                // End of method
                if (line.contains("}") && methodOpened)
                {
                    if (!currentMethod.isEmpty())
                    {
                        currentClassData.methods.insert(currentMethod, currentMethodData);
                        currentMethod.clear();
                        currentMethodData = MethodData();
                        methodOpened = false;
                    }
                }

                // End of current class
                if (line == "}")
                {
                    apiData.insert(currentClass, currentClassData);
                    currentClass.clear();
                    currentClassData = ClassData();
                    inChildsSection = false;
                    classOpened = false;
                }
            }
            else
            {
                // No childs section
                if (line == "},")
                {
                    apiData.insert(currentClass, currentClassData);
                    currentClass.clear();
                    currentClassData = ClassData();
                    inChildsSection = false;
                    classOpened = false;
                }
            }
        }
    }

    if (!currentClass.isEmpty())
    {
        apiData.insert(currentClass, currentClassData);
    }

    file.close();

    // Resolve inheritance after parsing
    QSet<QString> visited;

    for (const QString& className : apiData.keys())
    {
        appendInheritedMethods(apiData, className, visited);
    }

    // Save the file path in QSettings for future use
    QSettings settings("NOWA", "NOWALuaScript");
    settings.setValue("LuaApiFilePath", filePathName);

    QString message = "Lua Api file: " + filePathName + " parsed successfully.";
    qDebug() << message;
    Q_EMIT signal_luaApiPrepareResult(parseSilent, true, message);

    return apiData;
}

void LuaScriptAdapter::resolveInheritance(QMap<QString, ClassData>& apiData)
{
    QSet<QString> visited;

    for (const QString& className : apiData.keys())
    {
        this->appendInheritedMethods(apiData, className, visited);
    }
}

void LuaScriptAdapter::appendInheritedMethods(QMap<QString, ClassData>& apiData, const QString& className, QSet<QString>& visited)
{
    if (!apiData.contains(className))
    {
        qWarning() << "Class" << className << "not found in the API data!";
        return;
    }

    if (visited.contains(className))
    {
        qWarning() << "Cyclic inheritance detected for class" << className;
        return;
    }

    visited.insert(className);

    ClassData& classData = apiData[className];

    if (!classData.inherits.isEmpty() && apiData.contains(classData.inherits))
    {
        appendInheritedMethods(apiData, classData.inherits, visited);

        const ClassData& parentClassData = apiData[classData.inherits];
        for (auto it = parentClassData.methods.constBegin(); it != parentClassData.methods.constEnd(); ++it)
        {
            if (!classData.methods.contains(it.key()))
            {
                classData.methods[it.key()] = it.value();
            }
        }
    }

    visited.remove(className);
}

QStringList LuaScriptAdapter::splitPathTail(const QString& path, int segmentCount)
{
    QString normalizedPath = QDir::fromNativeSeparators(path);
    QStringList segments = normalizedPath.split('/', Qt::SkipEmptyParts);
    return segments.mid(qMax(0, segments.size() - segmentCount));
}

int LuaScriptAdapter::findLuaScript(const QString& filePathName)
{
    const int segmentCountToCompare = 3;
    QStringList inputSegments = this->splitPathTail(filePathName, segmentCountToCompare);

    int index = -1;
    for (const auto& luaScript : this->luaScripts)
    {
        ++index;
        QStringList scriptSegments = this->splitPathTail(luaScript->getFilePathName(), segmentCountToCompare);
        if (scriptSegments == inputSegments)
        {
            return index;
        }
    }

    return -1;
}
