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

bool LuaScript::readScriptFile(const QString& filePathName, QString& content)
{
    QFile file(filePathName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "Failed to open file:" << filePathName;
        return false;
    }

    content = QString::fromUtf8(file.readAll());
    file.close();

    // Must be replaced, else addTab, indent does not work correctly, as it messes up with \t
    content.replace("\t", "    ");

    return true;
}

bool LuaScript::loadScriptFromFile(const QString& filePathName)
{
    QString luaCode;
    if (false == LuaScript::readScriptFile(filePathName, luaCode))
    {
        return false;
    }

    // this->checkSyntax(luaCode);  // Perform a syntax check when loading the script

    // Parse the Lua script and update internal data structures for intellisense
    qDebug() << "Lua script loaded from file:" << filePathName;

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

void LuaScript::checkSyntax(const QString& luaCode)
{
    // Attempt to load the Lua code
    const QByteArray luaCodeUtf8 = luaCode.toUtf8();
    int result = luaL_loadbuffer(this->lua, luaCodeUtf8.constData(), luaCodeUtf8.size(), "script");

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
            // qDebug() << "Error line set from 'file:line:message' to: " << errorLine;
        }

        // Step 3: Check for "at line X" in the error message
        errorRegex.setPattern("at line ([0-9]+)");
        match = errorRegex.match(errorString);

        if (match.hasMatch())
        {
            // Found "at line X", set this as the error line
            errorLine = match.captured(1).toInt();
            // qDebug() << "Error line set from 'at line X' to: " << errorLine;
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

    // Attention: The compiled chunk is only removed and NOT executed. Formerly it was executed each second while typing: an endless loop in the script froze the editor,
    // and the return values were never removed from the stack, so it grew all the time.
    lua_pop(this->lua, 1);
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

        // Attention: Also an error without line is an error. Formerly it was sent as "valid", which removed the error. The message is shown, just no line is marked.
        // Note: Nothing has been pushed onto the lua stack, so nothing must be popped (formerly lua_pop corrupted the stack here).
        Q_EMIT signal_runtimeError(false, line, 0, 0, errorMessage);
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
