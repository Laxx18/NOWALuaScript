#include "projectmodel.h"
#include "luaeditormodel.h"

#include <QSettings>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <QDebug>

ProjectModel* ProjectModel::ms_pInstance = Q_NULLPTR;
QMutex ProjectModel::ms_mutex;

namespace
{
const int maxRecentProjects = 5;
// Protection against e.g. opening "C:/" by accident
const int maxFolderDepth = 12;
const int maxWatchedFolders = 1000;
// More results do not help anybody and make the result list slow
const int maxSearchResults = 5000;
// Length of the line text before and after the match in the result list
const int maxSearchPreTextLength = 60;
const int maxSearchPostTextLength = 160;

struct SearchResult
{
    QVariantList results;
    int searchedFileCount = 0;
    bool truncated = false;
};

void collectLuaFiles(const QDir& dir, int depth, QStringList& luaFiles)
{
    if (depth > maxFolderDepth)
    {
        return;
    }

    const QFileInfoList subFolders = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo& subFolderInfo : subFolders)
    {
        collectLuaFiles(QDir(subFolderInfo.absoluteFilePath()), depth + 1, luaFiles);
    }

    const QFileInfoList files = dir.entryInfoList(QStringList() << "*.lua", QDir::Files, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo& fileInfo : files)
    {
        luaFiles.append(fileInfo.absoluteFilePath());
    }
}

// Runs in a worker thread. Only gets copies, so that nothing of the gui thread is touched.
SearchResult searchInFiles(const QString& rootFolder, const QList<QPair<QString, QString>>& openLuaScripts, const QString& searchText, bool wholeWord, bool caseSensitive)
{
    SearchResult searchResult;

    QStringList filePathNames;
    if (false == rootFolder.isEmpty())
    {
        collectLuaFiles(QDir(rootFolder), 0, filePathNames);
    }

    // Open lua scripts are searched with their content in the editor (maybe not saved yet). Open lua scripts outside of the project are searched too.
    QHash<QString, QString> openContents;
    for (const auto& openLuaScript : openLuaScripts)
    {
        const QString key = QDir::cleanPath(openLuaScript.first).toLower();
        openContents.insert(key, openLuaScript.second);

        bool isInList = false;
        for (const QString& filePathName : std::as_const(filePathNames))
        {
            if (filePathName.toLower() == key)
            {
                isInList = true;
                break;
            }
        }
        if (false == isInList)
        {
            filePathNames.append(QDir::cleanPath(openLuaScript.first));
        }
    }

    QString pattern = QRegularExpression::escape(searchText);
    if (true == wholeWord)
    {
        pattern = "(?<![A-Za-z0-9_])" + pattern + "(?![A-Za-z0-9_])";
    }
    QRegularExpression regex(pattern, true == caseSensitive ? QRegularExpression::NoPatternOption : QRegularExpression::CaseInsensitiveOption);
    if (false == regex.isValid())
    {
        return searchResult;
    }

    const QDir rootDir(rootFolder);

    for (const QString& filePathName : std::as_const(filePathNames))
    {
        QString content;
        auto it = openContents.constFind(filePathName.toLower());
        if (it != openContents.constEnd())
        {
            content = it.value();
        }
        else
        {
            QFile file(filePathName);
            if (false == file.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                continue;
            }
            content = QString::fromUtf8(file.readAll());
            file.close();
        }

        ++searchResult.searchedFileCount;

        QString relativePath = filePathName;
        if (false == rootFolder.isEmpty())
        {
            const QString relative = rootDir.relativeFilePath(filePathName);
            if (false == relative.startsWith(".."))
            {
                relativePath = relative;
            }
        }

        const QStringList lines = content.split('\n');
        for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex)
        {
            QString lineText = lines.at(lineIndex);
            if (true == lineText.endsWith('\r'))
            {
                lineText.chop(1);
            }

            QRegularExpressionMatchIterator matchIterator = regex.globalMatch(lineText);
            while (true == matchIterator.hasNext())
            {
                const QRegularExpressionMatch match = matchIterator.next();
                if (match.capturedLength() <= 0)
                {
                    continue;
                }

                if (searchResult.results.size() >= maxSearchResults)
                {
                    searchResult.truncated = true;
                    return searchResult;
                }

                const int column = static_cast<int>(match.capturedStart());
                const int length = static_cast<int>(match.capturedLength());

                // Text before the match without the indentation and shortened, so that the match is always visible in the list
                int preStart = 0;
                while (preStart < column && true == lineText.at(preStart).isSpace())
                {
                    ++preStart;
                }
                QString preText = lineText.mid(preStart, column - preStart);
                if (preText.size() > maxSearchPreTextLength)
                {
                    preText = QStringLiteral("...") + preText.right(maxSearchPreTextLength);
                }
                QString postText = lineText.mid(column + length);
                if (postText.size() > maxSearchPostTextLength)
                {
                    postText = postText.left(maxSearchPostTextLength) + QStringLiteral("...");
                }

                QVariantMap result;
                result["filePathName"] = filePathName;
                result["relativePath"] = relativePath;
                result["line"] = lineIndex + 1;
                result["column"] = column;
                result["length"] = length;
                result["preText"] = preText;
                result["matchText"] = lineText.mid(column, length);
                result["postText"] = postText;
                searchResult.results.append(result);
            }
        }
    }

    return searchResult;
}
}

ProjectModel::ProjectModel(QObject* parent)
    : QStandardItemModel(parent),
    luaFileCount(0),
    searchGeneration(0),
    isSearching(false)
{
    this->loadRecentProjects();

    // Lua files or folders added or removed outside of the editor. Collected for a moment, because e.g. copying a folder changes a lot at once.
    this->refreshTimer.setSingleShot(true);
    this->refreshTimer.setInterval(500);
    connect(&this->refreshTimer, &QTimer::timeout, this, &ProjectModel::refresh);
    connect(&this->folderWatcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString&)
            {
                this->refreshTimer.start();
            });

    // Attention: Formerly the open lua scripts were only written, when "Save Project" was clicked. Saving the project right after opening the folder
    // therefore stored an empty list and the lua scripts opened afterwards were lost. Now the project file follows the open lua scripts automatically.
    this->autoSaveTimer.setSingleShot(true);
    this->autoSaveTimer.setInterval(500);
    connect(&this->autoSaveTimer, &QTimer::timeout, this, [this]()
            {
                if (false == this->projectFilePathName.isEmpty())
                {
                    this->writeProjectFile(this->projectFilePathName);
                }
            });

    LuaEditorModel* luaEditorModel = LuaEditorModel::instance();
    connect(luaEditorModel, &QAbstractItemModel::rowsInserted, this, &ProjectModel::scheduleAutoSave);
    connect(luaEditorModel, &QAbstractItemModel::rowsRemoved, this, &ProjectModel::scheduleAutoSave);
    connect(luaEditorModel, &LuaEditorModel::currentIndexChanged, this, &ProjectModel::scheduleAutoSave);

    // A pending auto save is done before the application quits
    if (Q_NULLPTR != QCoreApplication::instance())
    {
        connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this]()
                {
                    if (true == this->autoSaveTimer.isActive())
                    {
                        this->autoSaveTimer.stop();
                        if (false == this->projectFilePathName.isEmpty())
                        {
                            this->writeProjectFile(this->projectFilePathName);
                        }
                    }
                });
    }
}

void ProjectModel::scheduleAutoSave(void)
{
    if (false == this->projectFilePathName.isEmpty())
    {
        this->autoSaveTimer.start();
    }
}

ProjectModel* ProjectModel::instance()
{
    QMutexLocker lock(&ms_mutex);

    if (ms_pInstance == Q_NULLPTR)
    {
        ms_pInstance = new ProjectModel();
    }
    return ms_pInstance;
}

QObject* ProjectModel::getSingletonTypeProvider(QQmlEngine* pEngine, QJSEngine* pScriptEngine)
{
    Q_UNUSED(pEngine)
    Q_UNUSED(pScriptEngine)

    return instance();
}

QHash<int, QByteArray> ProjectModel::roleNames() const
{
    QHash<int, QByteArray> roles = QStandardItemModel::roleNames();
    roles[FilePathNameRole] = "filePathName";
    roles[IsFolderRole] = "isFolder";
    return roles;
}

QString ProjectModel::toLocalPath(const QString& pathOrUrl)
{
    QString path = pathOrUrl.trimmed();
    if (true == path.startsWith("file:", Qt::CaseInsensitive))
    {
        path = QUrl(path).toLocalFile();
    }
    path = path.replace("\\", "/");

    while (path.size() > 3 && true == path.endsWith('/'))
    {
        path.chop(1);
    }
    return path;
}

bool ProjectModel::openFolder(const QString& folder)
{
    const QString localFolder = toLocalPath(folder);
    QFileInfo folderInfo(localFolder);
    if (true == localFolder.isEmpty() || false == folderInfo.isDir())
    {
        Q_EMIT projectError("The folder does not exist: " + localFolder);
        return false;
    }

    this->rootFolder = folderInfo.absoluteFilePath();
    // A new folder is not yet saved as project
    this->projectFilePathName.clear();

    this->refresh();

    Q_EMIT projectChanged();
    return true;
}

bool ProjectModel::openProject(const QString& projectFilePathName)
{
    const QString localProjectFilePathName = toLocalPath(projectFilePathName);

    QFile file(localProjectFilePathName);
    if (false == file.open(QIODevice::ReadOnly))
    {
        this->removeRecentProject(localProjectFilePathName);
        Q_EMIT projectError("Unable to open project file: " + localProjectFilePathName);
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (parseError.error != QJsonParseError::NoError || false == document.isObject())
    {
        Q_EMIT projectError("Invalid project file: " + localProjectFilePathName + "\n" + parseError.errorString());
        return false;
    }

    const QJsonObject projectObject = document.object();

    // The paths in the project file are relative to the project file, so that the project can be moved together with its folder
    const QDir projectDir = QFileInfo(localProjectFilePathName).absoluteDir();
    const QString folder = QDir::cleanPath(projectDir.absoluteFilePath(projectObject.value("rootFolder").toString(".")));

    if (false == this->openFolder(folder))
    {
        return false;
    }

    this->projectFilePathName = QFileInfo(localProjectFilePathName).absoluteFilePath();
    this->addRecentProject(this->projectFilePathName);
    Q_EMIT projectChanged();

    // Opens the lua scripts, which were open when saving. The current one at last, so that it becomes the current tab.
    QStringList filesToOpen;
    const QJsonArray openFiles = projectObject.value("openFiles").toArray();
    for (const QJsonValue& value : openFiles)
    {
        filesToOpen.append(QDir::cleanPath(projectDir.absoluteFilePath(value.toString())));
    }

    const QString currentFile = projectObject.value("currentFile").toString();
    if (false == currentFile.isEmpty())
    {
        const QString currentFilePathName = QDir::cleanPath(projectDir.absoluteFilePath(currentFile));
        filesToOpen.removeAll(currentFilePathName);
        filesToOpen.append(currentFilePathName);
    }

    for (const QString& filePathName : std::as_const(filesToOpen))
    {
        if (true == QFileInfo::exists(filePathName))
        {
            LuaEditorModel::instance()->requestAddLuaScript(filePathName);
        }
    }

    return true;
}

bool ProjectModel::saveProject(const QString& projectFilePathName)
{
    if (false == this->getIsOpen())
    {
        return false;
    }

    QString localProjectFilePathName = toLocalPath(projectFilePathName);
    if (true == localProjectFilePathName.isEmpty())
    {
        localProjectFilePathName = this->projectFilePathName;
    }
    if (true == localProjectFilePathName.isEmpty())
    {
        return false;
    }
    if (false == localProjectFilePathName.endsWith(".nowaluaproj", Qt::CaseInsensitive))
    {
        localProjectFilePathName += ".nowaluaproj";
    }

    if (false == this->writeProjectFile(localProjectFilePathName))
    {
        return false;
    }

    this->projectFilePathName = QFileInfo(localProjectFilePathName).absoluteFilePath();
    this->addRecentProject(this->projectFilePathName);
    Q_EMIT projectChanged();
    return true;
}

bool ProjectModel::writeProjectFile(const QString& filePathName)
{
    const QDir projectDir = QFileInfo(filePathName).absoluteDir();

    QJsonObject projectObject;
    projectObject["type"] = "NOWALuaScriptProject";
    projectObject["version"] = 1;
    projectObject["rootFolder"] = projectDir.relativeFilePath(this->rootFolder);

    QJsonArray openFiles;
    LuaEditorModel* luaEditorModel = LuaEditorModel::instance();
    for (int i = 0; i < luaEditorModel->count(); ++i)
    {
        LuaEditorModelItem* item = luaEditorModel->getEditorModelItem(i);
        if (Q_NULLPTR != item)
        {
            openFiles.append(projectDir.relativeFilePath(item->getFilePathName()));
        }
    }
    projectObject["openFiles"] = openFiles;

    LuaEditorModelItem* currentItem = luaEditorModel->getEditorModelItem(luaEditorModel->getCurrentIndex());
    if (Q_NULLPTR != currentItem)
    {
        projectObject["currentFile"] = projectDir.relativeFilePath(currentItem->getFilePathName());
    }

    QFile file(filePathName);
    if (false == file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        Q_EMIT projectError("Unable to save project file: " + filePathName);
        return false;
    }
    file.write(QJsonDocument(projectObject).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

void ProjectModel::closeProject(void)
{
    if (false == this->folderWatcher.directories().isEmpty())
    {
        this->folderWatcher.removePaths(this->folderWatcher.directories());
    }
    this->refreshTimer.stop();

    // E.g. a lua script has just been closed: The pending auto save is done, before the project is gone
    if (true == this->autoSaveTimer.isActive())
    {
        this->autoSaveTimer.stop();
        if (false == this->projectFilePathName.isEmpty())
        {
            this->writeProjectFile(this->projectFilePathName);
        }
    }

    this->rootFolder.clear();
    this->projectFilePathName.clear();
    this->luaFileCount = 0;
    this->clear();

    Q_EMIT projectContentChanged();
    Q_EMIT projectChanged();
}

void ProjectModel::refresh(void)
{
    if (true == this->rootFolder.isEmpty())
    {
        return;
    }

    if (false == this->folderWatcher.directories().isEmpty())
    {
        this->folderWatcher.removePaths(this->folderWatcher.directories());
    }

    QStandardItemModel newContent;
    this->luaFileCount = 0;
    this->addFolderContent(newContent.invisibleRootItem(), QDir(this->rootFolder), 0);

    // Attention: Saving a file may also notify a folder change. Rebuilding the tree would collapse it, so it is only rebuilt, if something has really changed.
    bool isEqual = this->rowCount() == newContent.rowCount();
    if (true == isEqual)
    {
        QList<QPair<QStandardItem*, QStandardItem*>> stack;
        for (int i = 0; i < this->rowCount(); ++i)
        {
            stack.append(qMakePair(this->item(i), newContent.item(i)));
        }
        while (false == stack.isEmpty() && true == isEqual)
        {
            auto pair = stack.takeLast();
            if (pair.first->data(FilePathNameRole) != pair.second->data(FilePathNameRole) || pair.first->rowCount() != pair.second->rowCount())
            {
                isEqual = false;
                break;
            }
            for (int i = 0; i < pair.first->rowCount(); ++i)
            {
                stack.append(qMakePair(pair.first->child(i), pair.second->child(i)));
            }
        }
    }

    if (false == isEqual)
    {
        this->clear();
        while (newContent.rowCount() > 0)
        {
            this->appendRow(newContent.takeRow(0));
        }

        Q_EMIT projectContentChanged();
    }
}

bool ProjectModel::addFolderContent(QStandardItem* parentItem, const QDir& dir, int depth)
{
    if (depth > maxFolderDepth)
    {
        return false;
    }

    if (this->folderWatcher.directories().size() < maxWatchedFolders)
    {
        this->folderWatcher.addPath(dir.absolutePath());
    }

    bool hasLuaFiles = false;

    // Folders first, then the lua files, both sorted by name
    const QFileInfoList subFolders = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo& subFolderInfo : subFolders)
    {
        QStandardItem* folderItem = new QStandardItem(subFolderInfo.fileName());
        folderItem->setData(subFolderInfo.absoluteFilePath(), FilePathNameRole);
        folderItem->setData(true, IsFolderRole);
        folderItem->setEditable(false);

        if (true == this->addFolderContent(folderItem, QDir(subFolderInfo.absoluteFilePath()), depth + 1))
        {
            parentItem->appendRow(folderItem);
            hasLuaFiles = true;
        }
        else
        {
            // No lua files in it: Not shown
            delete folderItem;
        }
    }

    const QFileInfoList luaFiles = dir.entryInfoList(QStringList() << "*.lua", QDir::Files, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo& luaFileInfo : luaFiles)
    {
        QStandardItem* fileItem = new QStandardItem(luaFileInfo.fileName());
        fileItem->setData(luaFileInfo.absoluteFilePath(), FilePathNameRole);
        fileItem->setData(false, IsFolderRole);
        fileItem->setEditable(false);
        parentItem->appendRow(fileItem);
        ++this->luaFileCount;
        hasLuaFiles = true;
    }

    return hasLuaFiles;
}

QString ProjectModel::getDefaultProjectFilePathName(void) const
{
    if (false == this->projectFilePathName.isEmpty())
    {
        return this->projectFilePathName;
    }
    if (true == this->rootFolder.isEmpty())
    {
        return QString();
    }
    return this->rootFolder + "/" + QFileInfo(this->rootFolder).fileName() + ".nowaluaproj";
}

bool ProjectModel::getIsOpen(void) const
{
    return false == this->rootFolder.isEmpty();
}

QString ProjectModel::getRootFolder(void) const
{
    return this->rootFolder;
}

QString ProjectModel::getProjectFilePathName(void) const
{
    return this->projectFilePathName;
}

QString ProjectModel::getProjectName(void) const
{
    if (false == this->projectFilePathName.isEmpty())
    {
        return QFileInfo(this->projectFilePathName).completeBaseName();
    }
    return QFileInfo(this->rootFolder).fileName();
}

QStringList ProjectModel::getRecentProjects(void) const
{
    return this->recentProjects;
}

bool ProjectModel::getIsSearching(void) const
{
    return this->isSearching;
}

void ProjectModel::searchInProject(const QString& searchText, bool wholeWord, bool caseSensitive)
{
    const int generation = ++this->searchGeneration;

    if (true == searchText.isEmpty())
    {
        // A running search is ignored then
        if (true == this->isSearching)
        {
            this->isSearching = false;
            Q_EMIT isSearchingChanged();
        }
        Q_EMIT searchFinished(searchText, QVariantList(), 0, false);
        return;
    }

    // The content of the open lua scripts is copied here in the gui thread, the worker thread only gets the copies
    QList<QPair<QString, QString>> openLuaScripts;
    LuaEditorModel* luaEditorModel = LuaEditorModel::instance();
    for (int i = 0; i < luaEditorModel->count(); ++i)
    {
        LuaEditorModelItem* item = luaEditorModel->getEditorModelItem(i);
        if (Q_NULLPTR != item)
        {
            openLuaScripts.append(qMakePair(item->getFilePathName(), item->getContent()));
        }
    }

    if (false == this->isSearching)
    {
        this->isSearching = true;
        Q_EMIT isSearchingChanged();
    }

    QFutureWatcher<SearchResult>* watcher = new QFutureWatcher<SearchResult>(this);
    connect(watcher, &QFutureWatcher<SearchResult>::finished, this, [this, watcher, generation, searchText]()
    {
        watcher->deleteLater();

        // A newer search has been started in the meantime
        if (generation != this->searchGeneration)
        {
            return;
        }

        this->isSearching = false;
        Q_EMIT isSearchingChanged();

        const SearchResult searchResult = watcher->result();
        Q_EMIT searchFinished(searchText, searchResult.results, searchResult.searchedFileCount, searchResult.truncated);
    });

    watcher->setFuture(QtConcurrent::run(searchInFiles, this->rootFolder, openLuaScripts, searchText, wholeWord, caseSensitive));
}

int ProjectModel::getLuaFileCount(void) const
{
    return this->luaFileCount;
}

void ProjectModel::addRecentProject(const QString& projectFilePathName)
{
    this->recentProjects.removeAll(projectFilePathName);
    this->recentProjects.prepend(projectFilePathName);

    while (this->recentProjects.size() > maxRecentProjects)
    {
        this->recentProjects.removeLast();
    }

    this->saveRecentProjects();
    Q_EMIT recentProjectsChanged();
}

void ProjectModel::removeRecentProject(const QString& projectFilePathName)
{
    if (this->recentProjects.removeAll(toLocalPath(projectFilePathName)) > 0)
    {
        this->saveRecentProjects();
        Q_EMIT recentProjectsChanged();
    }
}

void ProjectModel::loadRecentProjects(void)
{
    QSettings settings;
    this->recentProjects = settings.value("RecentProjects", QStringList()).toStringList();
}

void ProjectModel::saveRecentProjects(void)
{
    QSettings settings;
    settings.setValue("RecentProjects", this->recentProjects);
}
