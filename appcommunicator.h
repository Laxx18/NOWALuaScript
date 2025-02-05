#ifndef APPCOMMUNICATOR_H
#define APPCOMMUNICATOR_H

#include <QObject>
// #include <QAbstractNativeEventFilter>
#include <QSharedPointer>
#include <QFileSystemWatcher>

#include <QLocalServer>
#include <QLocalSocket>

class LuaScriptController;

class AppCommunicator : public QObject // : public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    explicit AppCommunicator(QSharedPointer<LuaScriptController> ptrLuaScriptController, QObject* parent = Q_NULLPTR);

     ~AppCommunicator();

    // virtual bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

    void readXmlFile(const QString& filePath);


    QString getRunningFilePath(void) const;

    void deleteCommunicationFile(void);

    void deleteRunningFile(void);

public:
    static bool isInstanceRunning();

    static void sendFileToRunningInstance(const QString& filePath);

    static void showWindowsMessageBox(const QString& title, const QString& message);
Q_SIGNALS:
    void fileReceived(const QString& filePath);
private slots:
    void onFileChanged(const QString& path);

    void onDirectoryChanged(const QString& path);

    void handleNewConnection(void);

private:
    static inline const QString serverName = "NOWALuaScriptInstance";
    static QString runningFilePath;
private:
    QSharedPointer<LuaScriptController> ptrLuaScriptController;

    QString watchFilePathName;
    QFileSystemWatcher fileWatcher;
    QLocalServer* localServer;
};

#endif // APPCOMMUNICATOR_H
