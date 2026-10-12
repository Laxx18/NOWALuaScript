#ifndef PROJECTMODEL_H
#define PROJECTMODEL_H

#include <QStandardItemModel>
#include <QQmlEngine>
#include <QMutex>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QDir>

/**
 * @brief The project view on the left side: A folder is opened and all lua files below it are shown as tree. Folders without lua files are not shown.
 *        The project (folder and the open lua scripts) can be saved as project file (*.nowaluaproj) and opened again via the recent projects.
 */
class ProjectModel : public QStandardItemModel
{
    Q_OBJECT
public:
    Q_PROPERTY(bool isOpen READ getIsOpen NOTIFY projectChanged)

    Q_PROPERTY(QString rootFolder READ getRootFolder NOTIFY projectChanged)

    Q_PROPERTY(QString projectFilePathName READ getProjectFilePathName NOTIFY projectChanged)

    Q_PROPERTY(QString projectName READ getProjectName NOTIFY projectChanged)

    Q_PROPERTY(QStringList recentProjects READ getRecentProjects NOTIFY recentProjectsChanged)

    Q_PROPERTY(int luaFileCount READ getLuaFileCount NOTIFY projectContentChanged)
public:
    enum ProjectRoles
    {
        FilePathNameRole = Qt::UserRole + 1,
        IsFolderRole
    };

    /**
     * @brief Opens the folder and shows all lua files below it. The folder may also be a file url (from a FolderDialog).
     */
    Q_INVOKABLE bool openFolder(const QString& folder);

    /**
     * @brief Opens the project file: The folder is shown and the lua scripts, which were open when saving, are opened again.
     */
    Q_INVOKABLE bool openProject(const QString& projectFilePathName);

    /**
     * @brief Saves the project (folder and open lua scripts). An empty file path name saves to the current project file.
     */
    Q_INVOKABLE bool saveProject(const QString& projectFilePathName = QString());

    Q_INVOKABLE void closeProject(void);

    /**
     * @brief Scans the folder again. Is also done automatically, if lua files or folders are added or removed.
     */
    Q_INVOKABLE void refresh(void);

    /**
     * @brief Gets the proposed project file for "Save Project As", e.g. "C:/NOWA/media/projects/MyGame/MyGame.nowaluaproj".
     */
    Q_INVOKABLE QString getDefaultProjectFilePathName(void) const;

    Q_INVOKABLE void removeRecentProject(const QString& projectFilePathName);

    /**
     * @brief Converts a file url (file:///C:/...) or a path with backslashes to a local path with slashes.
     */
    Q_INVOKABLE static QString toLocalPath(const QString& pathOrUrl);
public:
    explicit ProjectModel(QObject* parent = Q_NULLPTR);

    QHash<int, QByteArray> roleNames() const override;

    bool getIsOpen(void) const;

    QString getRootFolder(void) const;

    QString getProjectFilePathName(void) const;

    QString getProjectName(void) const;

    QStringList getRecentProjects(void) const;

    int getLuaFileCount(void) const;
public:
    /**
     * @brief instance is the getter used to receive the object of this singleton implementation.
     * @returns singleton instance of this
     */
    static ProjectModel* instance();

    /**
     * @brief The singleton type provider is needed by the Qt(-meta)-system to register this singleton instace in qml world
     * @param pEngine not used but needed by function base
     * @param pSriptEngine not used but needed by function base
     * @returns singleton instance of this
     */
    static QObject* getSingletonTypeProvider(QQmlEngine* pEngine, QJSEngine* pScriptEngine);
Q_SIGNALS:
    void projectChanged();

    /**
     * @brief The tree has been built again. The view expands it then.
     */
    void projectContentChanged();

    void recentProjectsChanged();

    void projectError(const QString& message);
private:
    /**
     * @brief Adds the sub folders and lua files of the given folder. Returns true, if there is at least one lua file in it or below it.
     */
    bool addFolderContent(QStandardItem* parentItem, const QDir& dir, int depth);

    void addRecentProject(const QString& projectFilePathName);

    void loadRecentProjects(void);

    void saveRecentProjects(void);

    /**
     * @brief Writes the folder and the open lua scripts to the project file. Without touching the recent projects, so it can also be used for the auto save.
     */
    bool writeProjectFile(const QString& filePathName);

    /**
     * @brief If the project has been saved as project file, it is saved again a moment after a lua script has been opened or closed or the tab has changed.
     */
    void scheduleAutoSave(void);
private:
    static ProjectModel* ms_pInstance;
    static QMutex ms_mutex;
private:
    QString rootFolder;
    QString projectFilePathName;
    QStringList recentProjects;
    int luaFileCount;

    QFileSystemWatcher folderWatcher;
    QTimer refreshTimer;
    QTimer autoSaveTimer;
};

#endif // PROJECTMODEL_H
