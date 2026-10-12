#include "luascriptcontroller.h"
#include "luascriptadapter.h"

#include "model/luaeditormodelitem.h"
#include "model/apimodel.h"
#include "backend/luascript.h"

#include <QFileInfo>

#include <QDebug>

LuaScriptController::LuaScriptController(QQmlApplicationEngine* qmlEngine, QSharedPointer<LuaScriptAdapter> ptrLuaScriptAdapter, QObject* parent)
    : QObject{parent},
      qmlEngine(qmlEngine),
      ptrLuaScriptAdapter(ptrLuaScriptAdapter),
      luaScriptQmlAdapter(LuaScriptQmlAdapter::instance()),
      luaEditorModel(LuaEditorModel::instance()),
      luaScriptQmlComponent(Q_NULLPTR),
      luaEditorContainer(Q_NULLPTR)
{
    connect(luaEditorModel, &LuaEditorModel::signal_requestAddLuaScript, this, &LuaScriptController::slot_createLuaScript);
    connect(luaEditorModel, &LuaEditorModel::signal_requestRemoveLuaScript, this, &LuaScriptController::slot_removeLuaScript);
    connect(luaEditorModel, &LuaEditorModel::signal_requestSaveLuaScript, this, &LuaScriptController::slot_saveLuaScript);
    connect(ptrLuaScriptAdapter.data(), &LuaScriptAdapter::signal_luaScriptSaved, this, &LuaScriptController::slot_luaScriptSaved);
    connect(luaEditorModel, &LuaEditorModel::signal_requestReloadLuaScript, this, &LuaScriptController::slot_reloadLuaScript);

    // Editors often write a file in several steps, so the changes are collected for a moment
    this->changedFilesTimer.setSingleShot(true);
    this->changedFilesTimer.setInterval(300);
    connect(&this->changedFilesTimer, &QTimer::timeout, this, &LuaScriptController::processChangedFiles);
    connect(&this->fileWatcher, &QFileSystemWatcher::fileChanged, this, &LuaScriptController::onWatchedFileChanged);

    connect(this->luaScriptQmlAdapter, &LuaScriptQmlAdapter::signal_requestSyntaxCheck, ptrLuaScriptAdapter.data(), &LuaScriptAdapter::checkSyntax);

    connect(ptrLuaScriptAdapter.data(), &LuaScriptAdapter::signal_luaApiPrepareInitial, this, &LuaScriptController::prepareLuaApi);

    connect(this->luaScriptQmlAdapter, &LuaScriptQmlAdapter::signal_requestSetLuaApi, this, &LuaScriptController::prepareLuaApi);

    // Connecting backend response to QML

    connect(ptrLuaScriptAdapter.data(), &LuaScriptAdapter::signal_syntaxCheckResult, this->luaScriptQmlAdapter, &LuaScriptQmlAdapter::syntaxCheckResult);
    connect(ptrLuaScriptAdapter.data(), &LuaScriptAdapter::signal_runtimeError, this->luaScriptQmlAdapter, &LuaScriptQmlAdapter::signal_runtimeErrorResult);

    connect(ptrLuaScriptAdapter.data(), &LuaScriptAdapter::signal_luaApiPrepareResult, this->luaScriptQmlAdapter, &LuaScriptQmlAdapter::luaApiPreparationResult);

}

LuaScriptController::~LuaScriptController()
{
    // Removes child pin qml objects first (don't make the lose their parent!)
    for (const auto& luaEditorQml : this->luaEditorQmls)
    {
        luaEditorQml->deleteLater();
    }
    this->luaEditorQmls.clear();
}

void LuaScriptController::slot_createLuaScript(const QString& filePathName)
{
    if (Q_NULLPTR == this->ptrLuaScriptAdapter)
    {
        return;
    }

    QString tempFilePathName = filePathName;

    tempFilePathName = tempFilePathName.replace("\\", "/");

    // Try creating a node model and associate it with backendmodel, let it be filled and configured
    QPair<int, LuaEditorModelItem*> resultData = this->ptrLuaScriptAdapter->createLuaScript(tempFilePathName);

    LuaEditorModelItem* luaEditorModelItem = resultData.second;

    if (Q_NULLPTR == luaEditorModelItem)
    {
        if (resultData.first >= 0)
        {
            // There is already this lua script, so send event with index to set current tab.
            this->luaScriptQmlAdapter->changeTab(resultData.first);
        }

        return;
    }

    QQmlEngine::setObjectOwnership(luaEditorModel, QQmlEngine::CppOwnership);

    LuaEditorQml* luaEditorQml = this->createNewLuaEditorQml();
    if (Q_NULLPTR == luaEditorQml)
    {
        return;
    }

    this->luaEditorQmls.append(luaEditorQml);

    // this->luaEditorContainer->setProperty("currentIndex", this->luaEditorQmls.size());

    // luaEditorQml->setProperty("filePathName", tempFilePathName);
    luaEditorQml->setModel(luaEditorModelItem);

    // connect(luaEditorQml, &LuaEditorQml::currentTextChanged, ApiModel::instance(), &ApiModel::onCurrentTextChanged);

    this->luaEditorModel->addLuaScript(luaEditorModelItem);

    if (false == this->fileWatcher.files().contains(tempFilePathName))
    {
        this->fileWatcher.addPath(tempFilePathName);
    }

    // Sets the new script as active one
    this->luaScriptQmlAdapter->changeTab(this->luaEditorModel->getSize() - 1);
}

void LuaScriptController::slot_removeLuaScript(int index)
{
    LuaEditorQml* luaEditorQml = Q_NULLPTR;
#if 0
    for (const auto& currentLuaEditorQml : this->luaEditorQmls)
    {
        if (currentLuaEditorQml->property("filePathName") == filePathName)
        {
            luaEditorQml = currentLuaEditorQml;
            break;
        }
    }
#endif

    if (index < 0 || index >= this->luaEditorQmls.size())
    {
        qDebug() << "Error on removing lua script, invalid index: " << index;
        return;
    }

    luaEditorQml = this->luaEditorQmls.at(index);

    if (Q_NULLPTR == luaEditorQml)
    {
        qDebug() << "Error on removing lua script";
        return;
    }

    LuaEditorModelItem* luaEditorModelItem = luaEditorQml->model();

    bool success = this->ptrLuaScriptAdapter->removeLuaScript(luaEditorModelItem->getFilePathName());
    if (false == success)
    {
        qDebug() << "Error on removing lua script for file path name: " << luaEditorModelItem->getFilePathName();
        return;
    }

    this->fileWatcher.removePath(luaEditorModelItem->getFilePathName());
    this->changedFiles.remove(luaEditorModelItem->getFilePathName());

    // Attention: Remove it from the stack layout at once and not only with deleteLater, else the stack layout still has the old editor for a moment
    // and its indices do not fit to the tabs anymore.
    luaEditorQml->setParentItem(Q_NULLPTR);
    luaEditorQml->setVisible(false);
    luaEditorQml->deleteLater();

    this->luaEditorQmls.removeOne(luaEditorQml);

    this->luaEditorModel->removeLuaScript(luaEditorModelItem);
}

void LuaScriptController::slot_saveLuaScript(const QString& filePathName, const QString& content)
{
    bool success = this->ptrLuaScriptAdapter->saveLuaScript(filePathName, content);
    if (false == success)
    {
        qDebug() << "Error on saving lua script for file path name: " << filePathName;
        return;
    }
}

void LuaScriptController::slot_luaScriptSaved(const QString& filePathName)
{
    this->luaEditorModel->luaScriptSaved(filePathName);

    // A runtime error belongs to the code, which has been running. After saving, the code is a new one, so the error is removed.
    this->ptrLuaScriptAdapter->sendLuaScriptRuntimeError(filePathName, "", -1, -1, -1);

    // Some systems remove the file from the watcher, when it is written
    if (false == this->fileWatcher.files().contains(filePathName) && true == QFileInfo::exists(filePathName))
    {
        this->fileWatcher.addPath(filePathName);
    }
}

void LuaScriptController::slot_reloadLuaScript(const QString& filePathName)
{
    QString content;
    if (false == LuaScript::readScriptFile(filePathName, content))
    {
        qDebug() << "Error on reloading lua script for file path name: " << filePathName;
        return;
    }

    this->luaEditorModel->applyReloadedContent(filePathName, content);
}

void LuaScriptController::onWatchedFileChanged(const QString& filePathName)
{
    this->changedFiles.insert(filePathName);
    this->changedFilesTimer.start();
}

void LuaScriptController::processChangedFiles(void)
{
    const QSet<QString> changedFiles = this->changedFiles;
    this->changedFiles.clear();

    for (const QString& filePathName : changedFiles)
    {
        // Is the script still opened?
        if (Q_NULLPTR == this->luaEditorModel->getEditorModelItemByFilePathName(filePathName))
        {
            continue;
        }

        if (false == QFileInfo::exists(filePathName))
        {
            // Deleted or being replaced. If it comes back, it is detected by the next change of an other file or when saving.
            continue;
        }

        // Editors, which replace the file, remove it from the watcher
        if (false == this->fileWatcher.files().contains(filePathName))
        {
            this->fileWatcher.addPath(filePathName);
        }

        QString content;
        if (false == LuaScript::readScriptFile(filePathName, content))
        {
            continue;
        }

        this->luaEditorModel->luaScriptChangedExternally(filePathName, content);
    }
}

void LuaScriptController::slot_sendLuaScriptError(const QString& filePathName, const QString& errorMessage, int line, int start, int end)
{
    bool success = this->ptrLuaScriptAdapter->sendLuaScriptRuntimeError(filePathName, errorMessage, line, start, end);
    if (false == success)
    {
        // An empty message only removes an error. Opening the lua script just for that makes no sense.
        if (true == errorMessage.isEmpty())
        {
            return;
        }

        // If there is no such lua script so far, create it first and then send the error
        this->slot_createLuaScript(filePathName);
        this->ptrLuaScriptAdapter->sendLuaScriptRuntimeError(filePathName, errorMessage, line, start, end);
    }
}

void LuaScriptController::prepareLuaApi(const QString& filePathName, bool parseSilent)
{
    const auto& apiData = this->ptrLuaScriptAdapter->prepareLuaApi(filePathName, parseSilent);
    if (true == apiData.empty())
    {
        qDebug() << "Error parsing lua api for file path name: " << filePathName;
        return;
    }

    ApiModel::instance()->setApiData(apiData);
}

LuaEditorQml* LuaScriptController::createNewLuaEditorQml(void)
{
    if (Q_NULLPTR == this->luaScriptQmlComponent)
    {
        if (Q_NULLPTR == this->qmlEngine)
        {
            return Q_NULLPTR;
        }

        if (true == this->qmlEngine->rootObjects().isEmpty())
        {
            return Q_NULLPTR;
        }

        this->luaScriptQmlComponent = new QQmlComponent(this->qmlEngine, QUrl(QStringLiteral("qrc:/qml_files/LuaEditor.qml")), this);
    }

    // Find the root object (the ApplicationWindow)
    this->luaEditorContainer = this->qmlEngine->rootObjects()[0]->findChild<QQuickItem*>("luaEditorContainer");
    if (Q_NULLPTR == this->luaEditorContainer)
    {
        qWarning() << "Could not find luaEditorContainer!";
        return Q_NULLPTR;
    }
    else
    {
        qDebug() << "luaEditorContainer found!";
    }

    // Initialize node qml item
    LuaEditorQml* luaEditorQml;
    QObject* object = this->luaScriptQmlComponent->create();

    luaEditorQml = qobject_cast<LuaEditorQml*>(object);
    if (Q_NULLPTR != luaEditorQml)
    {
        QQmlEngine::setObjectOwnership(luaEditorQml, QQmlEngine::CppOwnership);
    }
    else
    {
        qDebug() << "Error on creating LuaEditorQml";
        for (auto& error : this->luaScriptQmlComponent->errors())
        {
            qDebug() << error.description();
        }
        return Q_NULLPTR;
    }

    luaEditorQml->setParentItem(this->luaEditorContainer);
    luaEditorQml->setParent(this->qmlEngine);
    luaEditorQml->setWidth(this->luaEditorContainer->width() - 12);
    // luaEditorQml->setHeight(1000.0f);

    return luaEditorQml;
}

QQuickItem *LuaScriptController::findChildRecursive(QObject* parent, const QString& childName)
{
    // Check if the current parent is valid
    if (Q_NULLPTR == parent)
    {
        return Q_NULLPTR;
    }

    // Iterate through all children of the parent
    for (QObject* child : parent->children())
    {
        QQuickItem* childItem = qobject_cast<QQuickItem*>(child);
        if (childItem)
        {
            // Print the current child name for debugging
            qDebug() << "Checking child:" << childItem->objectName() << ", type:" << childItem->metaObject()->className();

            // Check if the child matches the desired name
            if (childItem->objectName() == childName)
            {
                return childItem;
            }

            // Recursively search in the child
            QQuickItem* foundChild = this->findChildRecursive(childItem, childName);
            if (Q_NULLPTR != foundChild)
            {
                return foundChild;
            }
        }
    }
    return Q_NULLPTR;
}
