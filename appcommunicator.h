#ifndef APPCOMMUNICATOR_H
#define APPCOMMUNICATOR_H

#include <QObject>
// #include <QAbstractNativeEventFilter>
#include <QSharedPointer>
#include <QFileSystemWatcher>

class LuaScriptController;

class AppCommunicator : public QObject // : public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    explicit AppCommunicator(QSharedPointer<LuaScriptController> ptrLuaScriptController, QObject* parent = Q_NULLPTR);

    // virtual bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

    void showWindowsMessageBox(const QString& title, const QString& message);

    void readAndDeleteXmlFile(const QString& filePath);

    bool isInstanceRunning(void);

    QString getRunningFilePath(void) const;
private slots:
    void onFileChanged(const QString& path);

    void onDirectoryChanged(const QString& path);
private:
    QSharedPointer<LuaScriptController> ptrLuaScriptController;
    QString runningFilePath;
    QString watchFilePathName;
    QFileSystemWatcher fileWatcher;
};

#endif // APPCOMMUNICATOR_H
