#ifndef LUAEDITORMODEL_H
#define LUAEDITORMODEL_H

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QMutex>

#include "luaeditormodelitem.h"

class LuaEditorModel : public QAbstractListModel
{
    Q_OBJECT
public:
    Q_INVOKABLE void requestAddLuaScript(const QString& filePathName);

    Q_INVOKABLE void requestRemoveLuaScript(int index);

    Q_INVOKABLE void requestSaveLuaScript(void);

    Q_INVOKABLE void addRecentFile(const QString& filePathName);

    Q_INVOKABLE int count(void);

    Q_INVOKABLE LuaEditorModelItem* getEditorModelItem(int index) const;

    /**
     * @brief Gets whether the lua script at the given tab index has unsaved changes. Used when closing a tab, which is not the current one.
     */
    Q_INVOKABLE bool getHasChangesAt(int index) const;

    Q_INVOKABLE void commentLines(void);

    Q_INVOKABLE void unCommentLines(void);

    Q_INVOKABLE void addTabToSelection(void);

    Q_INVOKABLE void removeTabFromSelection(void);

    Q_INVOKABLE void breakLine(void);

    Q_INVOKABLE void searchInTextEdit(const QString& searchText, bool wholeWord, bool caseSensitive);

    Q_INVOKABLE void searchNextInTextEdit(const QString& searchText, bool wholeWord, bool caseSensitive, bool backwards);

    Q_INVOKABLE void replaceInTextEdit(const QString& searchText, const QString& replaceText, bool wholeWord, bool caseSensitive);

    Q_INVOKABLE void replaceAllInTextEdit(const QString& searchText, const QString& replaceText, bool wholeWord, bool caseSensitive);

    Q_INVOKABLE void clearSearch(void);

    Q_INVOKABLE void undo(void);

    Q_INVOKABLE void redo(void);

    Q_INVOKABLE void openProjectFolder(void);

    Q_INVOKABLE void sendTextToEditor(const QString& text);

    Q_INVOKABLE void sendVariableTextToEditor(const QString& text);

    /**
     * @brief Reloads the lua script from the file and discards the changes in the editor. Used, if the file has been changed outside of the editor.
     */
    Q_INVOKABLE void reloadLuaScript(const QString& filePathName);

public:
    Q_PROPERTY(bool hasChanges READ getHasChanges NOTIFY hasChangesChanged FINAL)

    Q_PROPERTY(int currentIndex READ getCurrentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged FINAL)

    Q_PROPERTY(QStringList recentFiles READ getRecentFiles NOTIFY recentFilesChanged)
public:
    enum LuaScriptRoles
    {
        FilePathNameRole = Qt::UserRole + 1,
        TitleRole
    };

    explicit LuaEditorModel(QObject* parent = Q_NULLPTR);

    // Override required functions
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void addLuaScript(LuaEditorModelItem* luaScriptModelItem);

    void removeLuaScript(LuaEditorModelItem* luaEditorModelItem);

    /**
     * @brief Gets whether the current lua script has unsaved changes.
     */
    bool getHasChanges(void) const;

    void updateTitle(int row, const QString& newTitle);

    /**
     * @brief Called, when the lua script with the given file path name has been saved. Attention: Must not use the current index, because the
     *        current tab may already be another one.
     */
    void luaScriptSaved(const QString& filePathName);

    /**
     * @brief Called, when the file of a lua script has been changed outside of the editor. If there are no unsaved changes, the new content is taken over directly,
     *        else the user is asked.
     */
    void luaScriptChangedExternally(const QString& filePathName, const QString& content);

    /**
     * @brief Takes over the given content as the new saved content of the lua script and shows it in the editor.
     */
    void applyReloadedContent(const QString& filePathName, const QString& content);

    LuaEditorModelItem* getEditorModelItemByFilePathName(const QString& filePathName) const;
public:
    /**
     * @brief instance is the getter used to receive the object of this singleton implementation.
     * @returns singleton instance of this
     */
    static LuaEditorModel* instance();

    /**
     * @brief The singleton type provider is needed by the Qt(-meta)-system to register this singleton instace in qml world
     * @param pEngine not used but needed by function base
     * @param pSriptEngine not used but needed by function base
     * @returns singleton instance of this
     */
    static QObject* getSingletonTypeProvider(QQmlEngine* pEngine, QJSEngine* pScriptEngine);

    int getCurrentIndex() const;

    void setCurrentIndex(int currentIndex);

    int getSize(void) const;

    QStringList getRecentFiles(void);
Q_SIGNALS:
    void signal_requestAddLuaScript(const QString& filePathName);

    void signal_requestRemoveLuaScript(int index);

    void signal_requestSaveLuaScript(const QString& filePathName, const QString& content);

    void signal_requestReloadLuaScript(const QString& filePathName);

    void signal_luaScriptAdded(const QString& filePathName, const QString& content);

    /**
     * @brief The content of the lua script has been reloaded from the file. The editor must show the new content.
     */
    void signal_luaScriptReloaded(const QString& filePathName, const QString& content);

    /**
     * @brief The file has been changed outside of the editor, while there are unsaved changes in the editor. The user must decide.
     */
    void signal_luaScriptChangedExternally(const QString& filePathName);

    void hasChangesChanged();

    void currentIndexChanged();

    void recentFilesChanged();
private:
    void loadRecentFiles(void);

    void saveRecentFiles(void);

    void closePopups(void);
private:
    static LuaEditorModel* ms_pInstance;
    static QMutex ms_mutex;
private:
    QList<LuaEditorModelItem*> luaScripts;
    int currentIndex;

    QStringList recentFiles;
};

#endif // LUAEDITORMODEL_H
