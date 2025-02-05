#include "appcommunicator.h"
#include "luascriptcontroller.h"

#include <QGuiApplication>
#include <QQuickWindow>
#include <QDebug>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

// TARGET_PATH will be set by CMake at compile time
#ifndef TARGET_PATH
#define TARGET_PATH ""
#endif

QString AppCommunicator::runningFilePath = "";

AppCommunicator::AppCommunicator(QSharedPointer<LuaScriptController> ptrLuaScriptController, QObject* parent)
    : QObject(parent),
    ptrLuaScriptController(ptrLuaScriptController),
    localServer(nullptr)
{
    // Create a local server
    this->localServer = new QLocalServer(this);
    if (this->localServer->listen(serverName))
    {
        connect(this->localServer, &QLocalServer::newConnection, this, &AppCommunicator::handleNewConnection);
        qDebug() << "NOWALuaScript local server started.";
    }
    else
    {
        qDebug() << "Failed to start NOWALuaScript local server:" << this->localServer->errorString();
    }

    // Set the path to the directory instead of a specific file
    QString watchDirectory = QString(TARGET_PATH) + "/NOWALuaScript/bin/";
    this->fileWatcher.addPath(watchDirectory);

    // Connect to the directoryChanged signal
    connect(&fileWatcher, &QFileSystemWatcher::directoryChanged, this, [=](const QString& path)
            {
                QTimer::singleShot(100, [=]() {  // Adjust the delay as needed
                    QDir dir(watchDirectory);
                    QStringList xmlFiles = dir.entryList({"*.xml"}, QDir::Files);

                    for (const QString& fileName : xmlFiles)
                    {
                        qDebug() << "Detected change in XML file:" << fileName;
                        this->readXmlFile(watchDirectory + "/" + fileName);
                    }
                });
            });


    QDir dir(watchDirectory);
    QStringList xmlFiles = dir.entryList({"*.xml"}, QDir::Files);

    for (const QString& fileName : xmlFiles)
    {
        qDebug() << "Detected change in XML file:" << fileName;
        this->readXmlFile(watchDirectory + "/" + fileName);
    }
}

AppCommunicator::~AppCommunicator()
{
    if (nullptr != this->localServer)
    {
        this->localServer->close();
    }
}

void AppCommunicator::onFileChanged(const QString& path)
{
    qDebug() << "File changed:" << path;

    if (path == this->watchFilePathName)
    {
        // showWindowsMessageBox("onFileChanged", "Received Lua script path: " + path);
        // Call the function when the file changes
        this->readXmlFile(path);

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
        this->readXmlFile(dataFilePath);

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

void AppCommunicator::readXmlFile(const QString& filePath)
{
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "Error opening file:" << filePath;
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

#if 0
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
    AppCommunicator::runningFilePath = QString(TARGET_PATH) + "/NOWALuaScript/bin/NOWALuaScript.running";

#if 0
    QFile runningFile(AppCommunicator::runningFilePath);
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
        qWarning() << "Failed to create running file at" << AppCommunicator::runningFilePath;
    }
    return false;
#endif

    QLocalSocket socket;
    socket.connectToServer(AppCommunicator::serverName);

    if (socket.waitForConnected(100))
    {
        // Another instance is running
        socket.disconnectFromServer();
        return true;
    }

    return false;
}

QString AppCommunicator::getRunningFilePath(void) const
{
    return AppCommunicator::runningFilePath;
}

void AppCommunicator::deleteCommunicationFile(void)
{
    QString communicationFile = QString(TARGET_PATH) + "/NOWALuaScript/bin/lua_script_data.xml";
    // Delete the file after reading
    if (QFile::remove(communicationFile))
    {
        qDebug() << "Commuication XML file deleted successfully.";
    }
}

void AppCommunicator::deleteRunningFile(void)
{
    QString watchDirectory = QString(TARGET_PATH) + "/NOWALuaScript/bin/";
    QDir dir(watchDirectory);

    // Check if the directory exists
    if (dir.exists())
    {
        // List all files with ".xml" extension in the directory
        QStringList xmlFiles = dir.entryList(QStringList() << "*.xml", QDir::Files);

        // Remove each XML file
        for (const QString& file : xmlFiles)
        {
            QString filePath = dir.filePath(file);
            QFile::remove(filePath);
        }
    }

    // Delete the file after reading
    if (QFile::remove(AppCommunicator::runningFilePath))
    {
        qDebug() << "Running file deleted successfully.";
    }
}

void AppCommunicator::sendFileToRunningInstance(const QString& filePath)
{
    QLocalSocket socket;
    socket.connectToServer(AppCommunicator::serverName);

    if (socket.waitForConnected(100))
    {
        QTextStream stream(&socket);
        stream << filePath << Qt::endl;
        socket.flush();
        socket.waitForBytesWritten();
        qDebug() << "Sent file path to running instance:" << filePath;
        socket.disconnectFromServer();
    }
    else
    {
        qDebug() << "Failed to connect to running instance";
    }
}

void AppCommunicator::handleNewConnection()
{
    QLocalSocket *clientConnection = localServer->nextPendingConnection();
    if (!clientConnection)
    {
        return;
    }

    connect(clientConnection, &QLocalSocket::disconnected, clientConnection, &QLocalSocket::deleteLater);

    clientConnection->waitForReadyRead(100); // Ensure the message is received
    QString filePath = QString::fromUtf8(clientConnection->readAll()).trimmed();

    if (!filePath.isEmpty())
    {
        qDebug() << "Received file via IPC:" << filePath;

        // Send to LuaScriptController
        Q_EMIT fileReceived(filePath);
    }

    clientConnection->disconnectFromServer();
}
