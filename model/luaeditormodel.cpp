#include "model/luaeditormodel.h"
#include "apimodel.h"

#include <QSettings>

LuaEditorModel* LuaEditorModel::ms_pInstance = Q_NULLPTR;
QMutex LuaEditorModel::ms_mutex;

LuaEditorModel::LuaEditorModel(QObject* parent)
    : QAbstractListModel(parent),
    currentIndex(-1)
{
    this->loadRecentFiles();
}

LuaEditorModel* LuaEditorModel::instance()
{
    QMutexLocker lock(&ms_mutex);

    if (ms_pInstance == Q_NULLPTR)
    {
        ms_pInstance = new LuaEditorModel();
    }
    return ms_pInstance;
}

QObject* LuaEditorModel::getSingletonTypeProvider(QQmlEngine* pEngine, QJSEngine* pScriptEngine)
{
    Q_UNUSED(pEngine)
    Q_UNUSED(pScriptEngine)

    return instance();
}

int LuaEditorModel::getCurrentIndex() const
{
    return this->currentIndex;
}

void LuaEditorModel::setCurrentIndex(int currentIndex)
{
    if (this->currentIndex == currentIndex)
    {
        return;
    }

    // A running search belongs to the tab, in which it has been started. The search bar starts it again for the new tab.
    LuaEditorModelItem* oldLuaEditorModelItem = this->getEditorModelItem(this->currentIndex);
    if (Q_NULLPTR != oldLuaEditorModelItem)
    {
        Q_EMIT oldLuaEditorModelItem->signal_clearSearch();
    }

    this->currentIndex = currentIndex;

    Q_EMIT currentIndexChanged();
    // Attention: hasChanges belongs to the current lua script, so it changes with the tab. Formerly it kept the value of the last changed script,
    // so that e.g. the save action stayed disabled for a tab with changes.
    Q_EMIT hasChangesChanged();
}

int LuaEditorModel::getSize() const
{
    return this->luaScripts.size();
}

bool LuaEditorModel::getHasChanges(void) const
{
    LuaEditorModelItem* luaEditorModelItem = this->getEditorModelItem(this->currentIndex);

    if (Q_NULLPTR != luaEditorModelItem)
    {
        return luaEditorModelItem->getHasChanges();
    }

    return false;
}

bool LuaEditorModel::getHasChangesAt(int index) const
{
    LuaEditorModelItem* luaEditorModelItem = this->getEditorModelItem(index);

    if (Q_NULLPTR != luaEditorModelItem)
    {
        return luaEditorModelItem->getHasChanges();
    }

    return false;
}

int LuaEditorModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return this->luaScripts.count();
}

QVariant LuaEditorModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= this->luaScripts.count())
    {
        return QVariant();
    }

    LuaEditorModelItem* item = this->luaScripts.at(index.row());

    if (role == FilePathNameRole)
    {
        return item->getFilePathName();
    }
    else if (role == TitleRole)
    {
        return item->getTitle();
    }

    return QVariant();
}

QHash<int, QByteArray> LuaEditorModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[FilePathNameRole] = "filePathName";
    roles[TitleRole] = "title";
    return roles;
}

void LuaEditorModel::addLuaScript(LuaEditorModelItem* luaScriptModelItem)
{
    beginInsertRows(QModelIndex(), this->luaScripts.count(), this->luaScripts.count());
    this->luaScripts.append(luaScriptModelItem);
    endInsertRows();

    // Connection to update title, each time the content changed and so the title changed
    connect(luaScriptModelItem, &LuaEditorModelItem::titleChanged, this, [=]()
    {
        QModelIndex index = this->index(this->luaScripts.indexOf(luaScriptModelItem));
        Q_EMIT dataChanged(index, index, { TitleRole });
    });

    connect(luaScriptModelItem, &LuaEditorModelItem::hasChangesChanged, this, [=]()
    {
        // Only the current lua script is relevant for the property
        if (this->luaScripts.indexOf(luaScriptModelItem) == this->currentIndex)
        {
            Q_EMIT hasChangesChanged();
        }
    });

    Q_EMIT signal_luaScriptAdded(luaScriptModelItem->getFilePathName(), luaScriptModelItem->getContent());
}

void LuaEditorModel::removeLuaScript(LuaEditorModelItem* luaEditorModelItem)
{
    int index = this->luaScripts.indexOf(luaEditorModelItem);
    if (-1 == index)
    {
        return;
    }

    beginRemoveRows(QModelIndex(), index, index);
    LuaEditorModelItem* removedItem = this->luaScripts.takeAt(index);
    endRemoveRows();

    // Attention: Not deleted at once. The editor of this lua script is only deleted later (deleteLater) and its qml still reacts e.g. on the syntax check
    // of the next tab and reads its model. With a deleted model item this crashed, when closing the current tab.
    removedItem->deleteLater();

    // The tab bar corrects its current index itself. If it stays the same number, it is now another lua script.
    if (this->currentIndex >= this->luaScripts.size())
    {
        this->setCurrentIndex(this->luaScripts.size() - 1);
    }
    Q_EMIT hasChangesChanged();
}

LuaEditorModelItem* LuaEditorModel::getEditorModelItemByFilePathName(const QString& filePathName) const
{
    for (const auto& luaEditorModelItem : this->luaScripts)
    {
        if (luaEditorModelItem->getFilePathName() == filePathName)
        {
            return luaEditorModelItem;
        }
    }
    return Q_NULLPTR;
}

void LuaEditorModel::requestAddLuaScript(const QString& filePathName)
{
    Q_EMIT signal_requestAddLuaScript(filePathName);
}

void LuaEditorModel::requestRemoveLuaScript(int index)
{
    Q_EMIT signal_requestRemoveLuaScript(index);
}

void LuaEditorModel::requestSaveLuaScript(void)
{
    LuaEditorModelItem* luaEditorModelItem = this->getEditorModelItem(this->currentIndex);

    if (Q_NULLPTR == luaEditorModelItem)
    {
        qDebug() << "Error: Could not save file because there is no lua script.";
        return;
    }

    // Attention: Always saves, if the user wants to save. Formerly it only saved, if the item had changes, which was not always correct.
    Q_EMIT signal_requestSaveLuaScript(luaEditorModelItem->getFilePathName(), luaEditorModelItem->getContent());
}

void LuaEditorModel::reloadLuaScript(const QString& filePathName)
{
    Q_EMIT signal_requestReloadLuaScript(filePathName);
}

void LuaEditorModel::addRecentFile(const QString& filePathName)
{
    if (true == this->recentFiles.contains(filePathName))
    {
        // Avoids duplicates by removing the existing entry
        this->recentFiles.removeAll(filePathName);
    }

    // Adds the file at the beginning of the list
    this->recentFiles.prepend(filePathName);

    if (this->recentFiles.size() > 5)
    {
        // Removes the oldest entry
        this->recentFiles.removeLast();
    }

    this->saveRecentFiles();
    // Notify QML about the change
    Q_EMIT recentFilesChanged();
}

QStringList LuaEditorModel::getRecentFiles(void)
{
    return this->recentFiles;
}

void LuaEditorModel::loadRecentFiles(void)
{
    QSettings settings = QSettings("NOWA", "NOWALuaScript");
    this->recentFiles = settings.value("RecentFiles", QStringList()).toStringList();
}

void LuaEditorModel::saveRecentFiles(void)
{
    QSettings settings = QSettings("NOWA", "NOWALuaScript");
    settings.setValue("RecentFiles", this->recentFiles);
}

void LuaEditorModel::closePopups(void)
{
    if (true == ApiModel::instance()->getIsIntellisenseShown())
    {
        ApiModel::instance()->closeIntellisense();
    }
    if (true == ApiModel::instance()->getIsMatchedFunctionShown())
    {
        ApiModel::instance()->closeMatchedFunction();
    }
}

int LuaEditorModel::count(void)
{
    return this->luaScripts.size();
}

LuaEditorModelItem* LuaEditorModel::getEditorModelItem(int index) const
{
    if (index < 0 || index >= this->luaScripts.count())
    {
        return Q_NULLPTR;
    }

    return this->luaScripts.at(index);
}

// Attention: The functions below formerly did nothing at all, if the function popup was flagged as shown. That flag stayed true, when the popup
// was closed by a click outside, so Tab, search, undo etc. did not work anymore. Now the popups are closed and the action is done.

void LuaEditorModel::commentLines()
{
    this->closePopups();

    const auto& luaEditorModelItem = this->getEditorModelItem(this->currentIndex);
    if (Q_NULLPTR != luaEditorModelItem)
    {
        Q_EMIT luaEditorModelItem->signal_commentLines();
    }
}

void LuaEditorModel::unCommentLines()
{
    this->closePopups();

    const auto& luaEditorModelItem = this->getEditorModelItem(this->currentIndex);
    if (Q_NULLPTR != luaEditorModelItem)
    {
        Q_EMIT luaEditorModelItem->signal_unCommentLines();
    }
}

void LuaEditorModel::addTabToSelection()
{
    this->closePopups();

    const auto& luaEditorModelItem = this->getEditorModelItem(this->currentIndex);
    if (Q_NULLPTR != luaEditorModelItem)
    {
        Q_EMIT luaEditorModelItem->signal_addTabToSelection();
    }
}

void LuaEditorModel::removeTabFromSelection()
{
    this->closePopups();

    const auto& luaEditorModelItem = this->getEditorModelItem(this->currentIndex);
    if (Q_NULLPTR != luaEditorModelItem)
    {
        Q_EMIT luaEditorModelItem->signal_removeTabFromSelection();
    }
}

void LuaEditorModel::breakLine()
{
    const auto& luaEditorModelItem = this->getEditorModelItem(this->currentIndex);
    if (Q_NULLPTR != luaEditorModelItem)
    {
        Q_EMIT luaEditorModelItem->signal_breakLine();
    }
}

void LuaEditorModel::searchInTextEdit(const QString& searchText, bool wholeWord, bool caseSensitive)
{
    this->closePopups();

    const auto& luaEditorModelItem = this->getEditorModelItem(this->currentIndex);
    if (Q_NULLPTR != luaEditorModelItem)
    {
        Q_EMIT luaEditorModelItem->signal_searchInTextEdit(searchText, wholeWord, caseSensitive);
    }
}

void LuaEditorModel::searchNextInTextEdit(const QString& searchText, bool wholeWord, bool caseSensitive, bool backwards)
{
    this->closePopups();

    const auto& luaEditorModelItem = this->getEditorModelItem(this->currentIndex);
    if (Q_NULLPTR != luaEditorModelItem)
    {
        Q_EMIT luaEditorModelItem->signal_searchNextInTextEdit(searchText, wholeWord, caseSensitive, backwards);
    }
}

void LuaEditorModel::replaceInTextEdit(const QString& searchText, const QString& replaceText, bool wholeWord, bool caseSensitive)
{
    this->closePopups();

    const auto& luaEditorModelItem = this->getEditorModelItem(this->currentIndex);
    if (Q_NULLPTR != luaEditorModelItem)
    {
        Q_EMIT luaEditorModelItem->signal_replaceInTextEdit(searchText, replaceText, wholeWord, caseSensitive);
    }
}

void LuaEditorModel::replaceAllInTextEdit(const QString& searchText, const QString& replaceText, bool wholeWord, bool caseSensitive)
{
    this->closePopups();

    const auto& luaEditorModelItem = this->getEditorModelItem(this->currentIndex);
    if (Q_NULLPTR != luaEditorModelItem)
    {
        Q_EMIT luaEditorModelItem->signal_replaceAllInTextEdit(searchText, replaceText, wholeWord, caseSensitive);
    }
}

void LuaEditorModel::clearSearch()
{
    const auto& luaEditorModelItem = this->getEditorModelItem(this->currentIndex);
    if (Q_NULLPTR != luaEditorModelItem)
    {
        Q_EMIT luaEditorModelItem->signal_clearSearch();
    }
}

void LuaEditorModel::undo()
{
    this->closePopups();

    const auto& luaEditorModelItem = this->getEditorModelItem(this->currentIndex);
    if (Q_NULLPTR != luaEditorModelItem)
    {
        Q_EMIT luaEditorModelItem->signal_undo();
    }
}

void LuaEditorModel::redo()
{
    this->closePopups();

    const auto& luaEditorModelItem = this->getEditorModelItem(this->currentIndex);
    if (Q_NULLPTR != luaEditorModelItem)
    {
        Q_EMIT luaEditorModelItem->signal_redo();
    }
}

void LuaEditorModel::openProjectFolder()
{
    const auto& luaEditorModelItem = this->getEditorModelItem(this->currentIndex);
    if (Q_NULLPTR != luaEditorModelItem)
    {
        luaEditorModelItem->openProjectFolder();
    }
}

void LuaEditorModel::sendTextToEditor(const QString& text)
{
    const auto& luaEditorModelItem = this->getEditorModelItem(this->currentIndex);
    if (Q_NULLPTR != luaEditorModelItem)
    {
        // ApiModel::instance()->setSelectedMethodName(text);
        luaEditorModelItem->signal_sendTextToEditor(text);
    }
}

void LuaEditorModel::sendVariableTextToEditor(const QString &text)
{
    const auto& luaEditorModelItem = this->getEditorModelItem(this->currentIndex);
    if (Q_NULLPTR != luaEditorModelItem)
    {
        // ApiModel::instance()->setSelectedMethodName(text);
        luaEditorModelItem->signal_sendVariableTextToEditor(text);
    }
}

void LuaEditorModel::updateTitle(int row, const QString& newTitle)
{
    if (row < 0 || row >= this->luaScripts.count())
    {
        return;
    }

    LuaEditorModelItem* item = this->luaScripts.at(row);
    item->setTitle(newTitle);

    // Notify view about the data change for the TitleRole
    QModelIndex index = this->index(row);
    Q_EMIT dataChanged(index, index, { TitleRole });
}

void LuaEditorModel::luaScriptSaved(const QString& filePathName)
{
    LuaEditorModelItem* luaEditorModelItem = this->getEditorModelItemByFilePathName(filePathName);

    if (Q_NULLPTR == luaEditorModelItem)
    {
        qDebug() << "Error: Could not find the saved lua script: " << filePathName;
        return;
    }

    luaEditorModelItem->restoreContent();
}

void LuaEditorModel::luaScriptChangedExternally(const QString& filePathName, const QString& content)
{
    LuaEditorModelItem* luaEditorModelItem = this->getEditorModelItemByFilePathName(filePathName);

    if (Q_NULLPTR == luaEditorModelItem)
    {
        return;
    }

    // E.g. the own save, or a tool which just touched the file
    if (true == luaEditorModelItem->isSavedContent(content))
    {
        return;
    }

    if (false == luaEditorModelItem->getHasChanges())
    {
        this->applyReloadedContent(filePathName, content);
    }
    else
    {
        Q_EMIT signal_luaScriptChangedExternally(filePathName);
    }
}

void LuaEditorModel::applyReloadedContent(const QString& filePathName, const QString& content)
{
    LuaEditorModelItem* luaEditorModelItem = this->getEditorModelItemByFilePathName(filePathName);

    if (Q_NULLPTR == luaEditorModelItem)
    {
        return;
    }

    // First the item, so that the following text change of the editor is not seen as a change of the user
    luaEditorModelItem->setReloadedContent(content);

    Q_EMIT signal_luaScriptReloaded(filePathName, content);
}
