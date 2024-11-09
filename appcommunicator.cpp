#include "appcommunicator.h"
#include "luascriptcontroller.h"

#include <QGuiApplication>
#include <QDebug>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

// TARGET_PATH will be set by CMake at compile time
#ifndef TARGET_PATH
#define TARGET_PATH ""
#endif

AppCommunicator::AppCommunicator(QSharedPointer<LuaScriptController> ptrLuaScriptController, QObject* parent)
    : QObject(parent),
    ptrLuaScriptController(ptrLuaScriptController)
{
    this->watchFilePathName = QString(TARGET_PATH) + "/NOWALuaScript/bin/lua_script_data.xml";

    if (QFileInfo::exists(this->watchFilePathName))
    {
        qDebug() << "File exists. Adding to watch list.";
        this->fileWatcher.addPath(this->watchFilePathName);

        // Call the function when the file changes
        this->eatXmlFile(this->watchFilePathName);
    }
    else
    {
        qDebug() << "File does not exist. Watching directory for creation.";
        this->fileWatcher.addPath(QFileInfo(this->watchFilePathName).absolutePath());
    }



    connect(&this->fileWatcher, &QFileSystemWatcher::fileChanged, this, &AppCommunicator::onFileChanged);
    connect(&this->fileWatcher, &QFileSystemWatcher::directoryChanged, this, &AppCommunicator::onDirectoryChanged);
}

void AppCommunicator::onFileChanged(const QString& path)
{
    qDebug() << "File changed:" << path;

    if (path == this->watchFilePathName)
    {
        // showWindowsMessageBox("onFileChanged", "Received Lua script path: " + path);
        // Call the function when the file changes
        this->eatXmlFile(path);

        // Re-add the file to the watcher if it exists (file may have been deleted/recreated)
        if (!this->fileWatcher.files().contains(path) && QFileInfo::exists(path))
        {
            qDebug() << "Re-adding file to watch list.";
            this->fileWatcher.addPath(path);
        }
    }
}

void AppCommunicator::onDirectoryChanged(const QString& path)
{
    qDebug() << "Directory changed:" << path;

    QString dataFilePath = path + "/lua_script_data.xml";

    if (dataFilePath == this->watchFilePathName)
    {
        // showWindowsMessageBox("onDirectoryChanged", "Received Lua script path: " + dataFilePath);

        // Call the function when the file changes
        this->eatXmlFile(dataFilePath);

        // Check if the file now exists and add it to the watcher
        if (!this->fileWatcher.files().contains(this->watchFilePathName) && QFileInfo::exists(this->watchFilePathName))
        {
            qDebug() << "File created. Adding to watch list.";
            this->fileWatcher.addPath(this->watchFilePathName);
        }
    }
}

void AppCommunicator::showWindowsMessageBox(const QString &title, const QString &message)
{
#ifdef Q_OS_WIN
    MessageBox(
        nullptr,
        reinterpret_cast<LPCWSTR>(message.utf16()),
        reinterpret_cast<LPCWSTR>(title.utf16()),
        MB_OK | MB_ICONINFORMATION
        );
#endif
}


// bool AppCommunicator::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
// {
//     Q_UNUSED(result)
// #ifdef Q_OS_WIN
//     if (eventType == "windows_generic_MSG")
//     {
//         MSG* msg = static_cast<MSG*>(message);
//         if (msg->message == WM_COPYDATA)
//         {
//             COPYDATASTRUCT* cds = reinterpret_cast<COPYDATASTRUCT*>(msg->lParam);
//             if (cds)
//             {
//                 // Parse the received data
//                 QString receivedData = QString::fromUtf8(static_cast<char*>(cds->lpData));

//                 // Split the received data into message ID and Lua script path
//                 QStringList parts = receivedData.split('|');
//                 if (parts.size() == 2)
//                 {
//                     QString messageId = parts[0];
//                     QString receivedLuaScriptPath = parts[1];

//                     // Check for your custom message ID
//                     if (messageId == "LuaScriptPath")
//                     {
//                         // Handle the Lua script path
//                         qDebug() << "Received Lua script path:" << receivedLuaScriptPath;

//                         // Show Windows MessageBox
//                         // showWindowsMessageBox("Received via sendMessage", "Received Lua script path: " + receivedLuaScriptPath);

//                         // If NOWALuaScript.exe is opened, a message is send out from NOWA-Design with a lua path. Add it!
//                         this->ptrLuaScriptController->slot_createLuaScript(receivedLuaScriptPath);

//                         // Emit signals or trigger actions in NOWALuaScript
//                         return true;  // Message handled
//                     }
//                 }
//             }
//         }
//     }
// #endif
//     return false;
// }

void AppCommunicator::eatXmlFile(const QString& filePath)
{
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return;
    }

    QXmlStreamReader xmlReader(&file);

    // Variables to store extracted data
    QString messageId;
    QString filePathName;
    QString errorMessage;
    int line = -1;
    int start = -1;
    int end = -1;

    // Parse the XML file
    while (!xmlReader.atEnd() && !xmlReader.hasError())
    {
        xmlReader.readNext();

        // Check if the node is a start element
        if (xmlReader.isStartElement())
        {
            if (xmlReader.name().toString() == "MessageId")
            {
                messageId = xmlReader.readElementText();
            }
            else if (xmlReader.name().toString() == "FilePath")
            {
                filePathName = xmlReader.readElementText();
            }
            else if (xmlReader.name().toString() == "error")
            {
                // Get attributes of the error element
                line = xmlReader.attributes().value("line").toInt();
                start = xmlReader.attributes().value("start").toInt();
                end = xmlReader.attributes().value("end").toInt();

                // Read the error message
                errorMessage = xmlReader.readElementText();
            }
        }
    }

    file.close();

    // Handle any parsing errors
    if (xmlReader.hasError())
    {
        qWarning() << "Error while parsing XML:" << xmlReader.errorString();
        return;
    }

    // Print the parsed data
    qDebug() << "Message ID:" << messageId;
    qDebug() << "File Path:" << filePathName;
    qDebug() << "Error Message:" << errorMessage;
    qDebug() << "Line:" << line;
    qDebug() << "Start:" << start;
    qDebug() << "End:" << end;

    // Check for specific message ID
    if (messageId == "LuaRuntimeErrors")
    {
        this->ptrLuaScriptController->slot_sendLuaScriptError(filePathName, errorMessage, line, start, end);
    }
    else if (messageId == "LuaScriptPath")
    {
        // Handle the Lua script path
        qDebug() << "Received Lua script path:" << filePathName;

        // If NOWALuaScript.exe is opened, a message is send out from NOWA-Design with a lua path. Add it!
        this->ptrLuaScriptController->slot_createLuaScript(filePathName);
    }

#if 1
    // Delete the file after reading
    if (QFile::remove(filePath))
    {
        qDebug() << "XML file deleted successfully.";
    }
    else
    {
        qWarning() << "Failed to delete the XML file.";
    }
#endif
}

bool AppCommunicator::isInstanceRunning(void)
{
    // Define the path for the "NOWALuaScript.running" file
    this->runningFilePath = QString(TARGET_PATH) + "/NOWALuaScript/bin/NOWALuaScript.running";

    QFile runningFile(this->runningFilePath);
    if (runningFile.open(QIODevice::ReadOnly))
    {
        return true;
    }

    // Create the file when the application starts
    if (runningFile.open(QIODevice::WriteOnly))
    {
        runningFile.write("Instance is running");
        runningFile.close();
    }
    else
    {
        qWarning() << "Failed to create running file at" << runningFilePath;
    }

    return false;
}

QString AppCommunicator::getRunningFilePath(void) const
{
    return this->runningFilePath;
}
