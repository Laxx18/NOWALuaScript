#include "backend/luascript.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QTextStream>
#include <QStack>
#include <QRegularExpression>
#include <QDebug>
#include <QTimer>

LuaScript::LuaScript(const QString& filePathName, QObject* parent)
    : QObject(parent),
      filePathName(filePathName)
{
    this->lua = lua_open();  // Create a new Lua state
    luaL_openlibs(this->lua);     // Load standard Lua libraries
}

LuaScript::~LuaScript()
{
    if (nullptr != this->lua)
    {
        lua_close(this->lua);  // Close the Lua interpreter when done
    }
}

bool LuaScript::loadScriptFromFile(const QString& filePathName)
{
    QFile file(filePathName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "Failed to open file:" << filePathName;
        return false;
    }

    QString luaCode = file.readAll();
    file.close();

    // this->checkSyntax(luaCode);  // Perform a syntax check when loading the script

    // Parse the Lua script and update internal data structures for intellisense
    qDebug() << "Lua script loaded from file:" << filePathName;

    // Must be replaced, else addTab, indent does not work correctly, as it messes up with \t
    luaCode.replace("\t", "    ");

    this->content = luaCode;

    Q_EMIT signal_luaScriptLoaded(luaCode);

    return true;
}

QString LuaScript::getFilePathName() const
{
    return this->filePathName;
}

QString LuaScript::getContent(void) const
{
    return this->content;
}

#if 0
bool LuaScript::checkSyntax(const QString& luaCode)
{
    int result = luaL_loadstring(this->lua, luaCode.toStdString().c_str());

    int errorLine = 0;
    QString errorMessage;

    if (result == /*LUA_OK*/ 0)
    {
        Q_EMIT signal_syntaxCheckFinished(true, errorLine, errorMessage);
        return true;  // No syntax errors
    }
    else
    {
        const char* errorMsg = lua_tostring(this->lua, -1);
        QString errorStr = QString(errorMsg);
        QRegularExpression regex(R"(\[string\s\".*\"\]:(\d+):\s(.*))");
        QRegularExpressionMatch match = regex.match(errorStr);
        if (match.hasMatch())
        {
            errorLine = match.captured(1).toInt();  // Capture the line number
            errorMessage = match.captured(2);       // Capture the error message
        }
        Q_EMIT signal_syntaxCheckFinished(false, errorLine, errorMessage);
        return false;  // Syntax error found
    }
}
#endif

#include <QRegularExpression>

void LuaScript::checkSyntax(const QString& luaCode)
{
    // Attempt to load the Lua code
    int result = luaL_loadstring(this->lua, luaCode.toStdString().c_str());

    if (result != 0)
    {
        // Get the error message from the Lua stack
        const char* errorMsg = lua_tostring(this->lua, -1);
        QString errorString = QString::fromUtf8(errorMsg);

        // Extract line number and error message using QRegularExpression
        int errorLine = -1;

        // Step 2: Check for "file:line:message" format
        QRegularExpression errorRegex(".*:([0-9]+):");
        QRegularExpressionMatch match = errorRegex.match(errorString);

        if (match.hasMatch())
        {
            errorLine = match.captured(1).toInt();
            qDebug() << "Error line set from 'file:line:message' to: " << errorLine;
        }

        // Step 3: Check for "at line X" in the error message
        errorRegex.setPattern("at line ([0-9]+)");
        match = errorRegex.match(errorString);

        if (match.hasMatch())
        {
            // Found "at line X", set this as the error line
            errorLine = match.captured(1).toInt();
            qDebug() << "Error line set from 'at line X' to: " << errorLine;
        }

        if (-1 != errorLine)
        {
            Q_EMIT signal_syntaxCheckResult(false, errorLine, 0, 0, errorString);
        }
        else
        {
            // Emit the syntax check result
            Q_EMIT signal_syntaxCheckResult(true, -1, 0, 0, "");
        }

        lua_pop(this->lua, 1); // Pop the error message from the stack
        return;
    }
    else
    {
        Q_EMIT signal_syntaxCheckResult(true, -1, 0, 0, "");
    }

    // If the code loads correctly, execute it
    result = lua_pcall(this->lua, 0, LUA_MULTRET, 0);
}

void LuaScript::checkRuntimeError(const QString& errorMessage, int line, int start, int end)
{
    if (false == errorMessage.isEmpty())
    {
        // Step 2: Check for "file:line:message" format
        QRegularExpression errorRegex(".*:([0-9]+):");
        QRegularExpressionMatch match = errorRegex.match(errorMessage);

        if (match.hasMatch() && -1 == line)
        {
            line = match.captured(1).toInt();
            qDebug() << "Error line set from 'file:line:message' to: " << line;
        }

        // Step 3: Check for "at line X" in the error message
        errorRegex.setPattern("at line ([0-9]+)");
        match = errorRegex.match(errorMessage);

        if (match.hasMatch() && -1 == line)
        {
            // Found "at line X", set this as the error line
            line = match.captured(1).toInt();
            qDebug() << "Error line set from 'at line X' to: " << line;
        }

        if (-1 != line)
        {
            Q_EMIT signal_runtimeError(false, line, 0, 0, errorMessage);
        }
        else
        {
            Q_EMIT signal_runtimeError(true, -1, 0, 0, "");
        }

        lua_pop(this->lua, 1); // Pop the error message from the stack
        return;
    }
    else
    {
        Q_EMIT signal_runtimeError(true, -1, 0, 0, "");
    }
}

bool LuaScript::saveLuaScript(const QString& content)
{
    bool success = false;

    // Write the lua content to a file
    QFile file(this->filePathName);

    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream out(&file);
        out << content;
        file.close();
        success = true;
    }
    else
    {
        QString message = "Failed to save GraphML file: \n" + filePathName + " Error: " + file.errorString();
        qDebug() << message;
        success = false;
    }

    return success;
}
