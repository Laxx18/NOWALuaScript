#include "luascriptadapter.h"

#include <QDebug>

#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QSettings>
#include <QRegularExpression>
#include <QRegularExpressionMatch>

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
        Q_EMIT signal_luaScriptSaved(luaScript->getFilePathName());
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
        Q_EMIT signal_luaApiPrepareResult(parseSilent, false, message);
        return apiData;
    }

    // Attention: The api file is parsed by the indentation of the lines and not by its braces and quotes:
    // - Level 1: class name, level 2: class attributes and "childs", level 3: method or constant name, level 4: method attributes.
    // The generated file is not always valid lua: some descriptions contain unescaped quotes, sometimes a closing brace is missing or two are in one line,
    // and some method names have a prefix like "number getMaxLength". The former parser counted braces and split at the first "=", so descriptions were cut,
    // whole classes were merged into the previous one (e.g. GraphicsConfigurationComponent into GoalResult) and lines with "return" were skipped.

    // key = "value" or key = "value",  The value is everything between the first and the last quote, so quotes inside are kept.
    static const QRegularExpression keyValueRegex(R"rx(^(\w+)\s*=\s*"(.*)"\s*,?\s*$)rx");
    // key = value without quotes
    static const QRegularExpression keyRawValueRegex(R"(^(\w+)\s*=\s*([^{}"]*?)\s*,?\s*$)");
    // name =  or  name = {  The name is the last identifier before the "="
    static const QRegularExpression nameRegex(R"(^([^"=]*?)\s*=\s*(\{)?\s*$)");
    static const QRegularExpression identifierRegex(R"([A-Za-z_]\w*)");

    QTextStream in(&file);

    QString currentClass;
    QString currentMethod;
    bool inChildsSection = false;

    auto storeClassAttribute = [&apiData, &currentClass](const QString& key, const QString& value)
    {
        if (true == currentClass.isEmpty())
        {
            return;
        }

        ClassData& classData = apiData[currentClass];
        if (key == "type")
        {
            classData.type = value;
        }
        else if (key == "description")
        {
            classData.description = value;
        }
        else if (key == "inherits")
        {
            classData.inherits = value;
        }
    };

    auto storeMethodAttribute = [&apiData, &currentClass, &currentMethod](const QString& key, const QString& value)
    {
        if (true == currentClass.isEmpty() || true == currentMethod.isEmpty())
        {
            return;
        }

        MethodData& methodData = apiData[currentClass].methods[currentMethod];
        if (key == "type")
        {
            methodData.type = value;
        }
        else if (key == "description")
        {
            methodData.description = value;
        }
        else if (key == "args")
        {
            methodData.args = value;
        }
        else if (key == "returns")
        {
            QString cleanedValue = value;
            static const QRegularExpression bracketsRegex("^\\(|\\)$");
            cleanedValue.replace(bracketsRegex, "");
            methodData.returns = cleanedValue.trimmed();

            if (methodData.returns == "nil")
            {
                methodData.returns = "void";
            }
        }
        else if (key == "valuetype")
        {
            methodData.valuetype = value;
        }
    };

    while (!in.atEnd())
    {
        const QString rawLine = in.readLine();
        const QString line = rawLine.trimmed();

        // Skip empty lines or comments
        if (line.isEmpty() || line.startsWith("--"))
        {
            continue;
        }

        // Indentation level: a tab or 4 spaces is one level
        int tabs = 0;
        int spaces = 0;
        for (const QChar ch : rawLine)
        {
            if (ch == '\t')
            {
                ++tabs;
            }
            else if (ch == ' ')
            {
                ++spaces;
            }
            else
            {
                break;
            }
        }
        const int level = tabs + spaces / 4;

        QRegularExpressionMatch nameMatch = nameRegex.match(line);
        QRegularExpressionMatch keyValueMatch = keyValueRegex.match(line);

        if (false == nameMatch.hasMatch() && true == keyValueMatch.hasMatch())
        {
            QString value = keyValueMatch.captured(2);
            value.replace("\\\"", "\"");

            if (2 == level)
            {
                storeClassAttribute(keyValueMatch.captured(1), value);
            }
            else if (4 == level)
            {
                storeMethodAttribute(keyValueMatch.captured(1), value);
            }
            continue;
        }

        if (true == nameMatch.hasMatch())
        {
            QString name;
            QRegularExpressionMatchIterator it = identifierRegex.globalMatch(nameMatch.captured(1));
            while (it.hasNext())
            {
                name = it.next().captured(0);
            }

            if (true == name.isEmpty())
            {
                continue;
            }

            if (1 == level)
            {
                currentClass = name;
                currentMethod.clear();
                inChildsSection = false;
                // Creates the class, also if it has no attributes
                apiData[currentClass];
            }
            else if (2 == level && "childs" == name)
            {
                inChildsSection = true;
            }
            else if (3 == level && true == inChildsSection && false == currentClass.isEmpty())
            {
                currentMethod = name;
                apiData[currentClass].methods[currentMethod];
            }
            continue;
        }

        // Only braces and commas, nothing to do, the indentation is used instead
        bool onlyBraces = true;
        for (const QChar ch : line)
        {
            if (ch != '{' && ch != '}' && ch != ',' && ch != ';' && false == ch.isSpace())
            {
                onlyBraces = false;
                break;
            }
        }
        if (true == onlyBraces)
        {
            continue;
        }

        QRegularExpressionMatch keyRawValueMatch = keyRawValueRegex.match(line);
        if (true == keyRawValueMatch.hasMatch())
        {
            if (2 == level)
            {
                storeClassAttribute(keyRawValueMatch.captured(1), keyRawValueMatch.captured(2));
            }
            else if (4 == level)
            {
                storeMethodAttribute(keyRawValueMatch.captured(1), keyRawValueMatch.captured(2));
            }
        }
    }

    file.close();

    if (true == apiData.isEmpty())
    {
        QString message = "Lua Api file: " + filePathName + " does not contain any class.";
        qWarning() << message;
        Q_EMIT signal_luaApiPrepareResult(parseSilent, false, message);
        return apiData;
    }

    // Resolve inheritance after parsing
    QSet<QString> visited;

    for (const QString& className : apiData.keys())
    {
        appendInheritedMethods(apiData, className, visited);
    }

    // Save the file path in QSettings for future use
    QSettings settings("NOWA", "NOWALuaScript");
    settings.setValue("LuaApiFilePath", filePathName);

    int methodCount = 0;
    for (auto it = apiData.cbegin(); it != apiData.cend(); ++it)
    {
        methodCount += it.value().methods.size();
    }

    QString message = "Lua Api file: " + filePathName + " parsed successfully. Classes: " + QString::number(apiData.size()) + ", methods and constants (with inherited): " + QString::number(methodCount);
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
