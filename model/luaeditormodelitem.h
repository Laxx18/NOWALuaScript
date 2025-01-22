#ifndef LUAEDITORMODELITEM_H
#define LUAEDITORMODELITEM_H

#include <QObject>
#include <QMap>
#include <QThread>

#include "matchclassworker.h"

class LuaEditorModelItem : public QObject
{
    Q_OBJECT
public:
    Q_PROPERTY(QString filePathName READ getFilePathName NOTIFY filePathNameChanged)

    Q_PROPERTY(QString content READ getContent WRITE setContent NOTIFY contentChanged)

    Q_PROPERTY(QString title READ getTitle NOTIFY titleChanged)
public:
    struct HorizontalChainTypeInfo
    {
        int position = -1;
        QString chainType; // The horizontal chain types are the changed types inside a line at a given position -> getGameProgressModule():getGlobalValue() -> getGameProgressModule then getGlobalValue
    };

    struct VerticalChainTypeInfo
    {
        int line = -1;
        QVector<HorizontalChainTypeInfo> horizontalChainTypes; // The chain types (initially empty, filled when type can be determined), e.g. scene1_barrel_0:getPhysicsActiveComponent() -> chainType -> PhysicsActiveComponent
    };

    struct LuaVariableInfo
    {
        QString name;   // Variable name
        QString type;   // Inferred type (initially empty, filled when type can be determined)
        int line = -1;       // Line number where the variable is declared
        QString scope;  // Scope (e.g., global, local)
        QVector<VerticalChainTypeInfo> verticalChainTypeList;
    };
public:
    explicit LuaEditorModelItem(QObject* parent = Q_NULLPTR);

    virtual ~LuaEditorModelItem();

    QString getFilePathName() const;

    void setFilePathName(const QString& filePathName);

    QString getContent() const;

    void setContent(const QString& content);

    QString getTitle() const;

    void setTitle(const QString& title);

    bool getHasChanges(void) const;

    void setHasChanges(bool hasChanges);

    void restoreContent(void);

    void openProjectFolder(void);

    bool hasUnmatchedOpeningBracket(const QString& text);

    void detectVariables(void);

    LuaVariableInfo getClassForVariableName(const QString& variableName);

    void resetMatchedClass(void);

    bool hasVariablesDetected(void) const;

    QVariantMap processMatchedVariables(bool forSingleton, const QString& text);

    QMap<QString, LuaVariableInfo> getVariableMap(void) const;

public slots:
    void startIntellisenseProcessing(bool forConstant, bool forFunctionParameters, const QString& currentText, const QString& textAfterKeyword, int cursorPos, int mouseX, int mouseY);

    void closeIntellisense(void);

    void closeMatchedFunction();
Q_SIGNALS:
    void filePathNameChanged();

    void contentChanged();

    void titleChanged();

    void hasChangesChanged();

    void signal_commentLines();

    void signal_unCommentLines();

    void signal_addTabToSelection();

    void signal_removeTabFromSelection();

    void signal_breakLine();

    void signal_searchInTextEdit(const QString& searchText, bool wholeWord, bool caseSensitive);

    void signal_searchContinueInTextEdit(const QString& searchText, bool wholeWord, bool caseSensitive);

    void signal_replaceInTextEdit(const QString& searchText, const QString& replaceText);

    void signal_clearSearch();

    void signal_undo();

    void signal_redo();

    void signal_sendTextToEditor(const QString& text);

    void signal_sendVariableTextToEditor(const QString& text);
private:
    void detectLocalVariables(const QString& line, int lineNumber);

    void detectGlobalVariables(const QString& line, int lineNumber);

    void detectSingletons(const QString& line, int lineNumber);

    void handleTableAccess(const QString& statement, int lineNumber);

    bool isLuaNativeType(const QString& typeName);

    void handleTableTypes(const QString& variableName, const QString& statement, int lineNumber);

    void handleCastAssignment(const QString& statement, int lineNumber);

    void handleMethodCallAssignment(const QString& statement, int lineNumber);

    void handleSimpleAssignment(const QString& statement, int lineNumber);

    void handleMethodChainAssignment(const QString& statement, int lineNumber);

    QString resolveMethodChainType(const QString& objectVar);

    void handleMethodChain(const QString& statement, int lineNumber);

    void handleLoop(const QString& statement, int lineNumber, const QStringList& lines);

    bool isCommentOnlyLine(const QString& line);
private:
    QString filePathName;
    QString content;
    QString oldContent;
    QString title;
    bool hasChanges;
    bool firstTimeContent;
    QMap<QString, LuaVariableInfo> variableMap;

    MatchClassWorker* matchClassWorker;
    QThread* matchClassThread;

    QString matchedClassName;
    bool printToConsole;
};

#endif // LUAEDITORMODELITEM_H
