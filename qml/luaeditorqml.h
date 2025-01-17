#ifndef LUAEDITORQML_H
#define LUAEDITORQML_H

#include <QQuickItem>
#include <QtQml>
#include <QQuickTextDocument>

#include "luahighlighter.h"

class LuaEditorModelItem;

class LuaEditorQml : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
public:

     Q_PROPERTY(LuaEditorModelItem* model READ model NOTIFY modelChanged)

     Q_PROPERTY(QString currentText READ getCurrentText WRITE setCurrentText NOTIFY currentTextChanged)
public:
     Q_INVOKABLE void highlightError(int line, int start, int end);

     Q_INVOKABLE void clearError();

     Q_INVOKABLE void highlightRuntimeError(int line, int start, int end);

     Q_INVOKABLE void clearRuntimeError();

     Q_INVOKABLE void cursorPositionChanged(int cursorPosition);

     Q_INVOKABLE void handleKeywordPressed(QChar keyword);

     Q_INVOKABLE void updateContentY(qreal contentY);

     Q_INVOKABLE void highlightWordUnderCursor(const QString& word);

     Q_INVOKABLE void clearHighlightWordUnderCursor(void);
public:
    // IMPORANT: parent MUST be by default Q_NULLPTR! Else: If creating for QML: Element is not createable will occur!!!
    // See: https://forum.qt.io/topic/155986/qt6-qabstractlistmodel-constructor-with-two-arguments-qml-element-is-not-creatable/2
    explicit LuaEditorQml(QQuickItem* parent = Q_NULLPTR);

    /**
     * @brief Gets the lua editor model item..
     * @returns                 The lua seditor model item..
     */
    LuaEditorModelItem* model() const;

    /**
     * @brief Sets the lua editor model item.
     * @param pModel             The lua editor model item. to set.
     */
    void setModel(LuaEditorModelItem* luaEditorModelItem);

    void setCurrentText(const QString& currentText);

    QString getCurrentText(void) const;

public slots:
    void onParentChanged(QQuickItem* newParent);
Q_SIGNALS:
    void modelChanged();

    void currentTextChanged();

    void requestIntellisenseProcessing(bool forConstant, bool forFunctionParameters, const QString& currentText, const QString& textAfterColon, int cursorPos, int mouseX, int mouseY, bool forMatchedFunctionMenu);

    void requestCloseIntellisense();

    void requestCloseMatchedFunctionContextMenu();

    void signal_insertingNewLine(bool inserting);

    void signal_resultSearchMatchCount(int matchCount);

    void signal_resultSearchContinuePosition(const QRectF& cursorRectangle);
private:
    void showIntelliSenseContextMenuAtCursor(bool forConstant, bool forFunctionParameters, const QString& text, int cursorPosition, const QString& textAfterColon);

    // Helper function to get the cursor rectangle
    QPointF cursorAtPosition(const QString& currentText, int cursorPos);

    void resetTextAfterColon(void);

    void resetTextAfterDot(void);

    QString getCurrentLineUpToCursor(void);

    bool processVariableBeingTyped(void);

    bool processFunctionBeingTyped(void);

    bool processFunctionParametersBeingTyped(void);

    int getCharsToReplace(const QString& originalText, const QString& suggestedText);
private:
    LuaEditorModelItem* luaEditorModelItem;
    QQuickItem* lineNumbersEdit;
    QQuickItem* luaEditorTextEdit;
    QQuickTextDocument* quickTextDocument;
    LuaHighlighter* highlighter;  // Add a member for the highlighter

    qreal scrollY;

    bool isAfterColon = false; // Track if we are after a colon
    QString typedAfterColon;    // Store text typed after colon
    bool isAfterDot = false; // Track if we are after a colon
    QString typedAfterDot;    // Store text typed after colon
    bool charDeleted;
    int cursorPosition;
    int oldCursorPosition;
    int lastColonIndex;
    int lastDotIndex;
    QString currentText;
    QString currentLineTextVariable;
    bool isInMatchedFunctionProcessing;
};

#endif // LUAEDITORQML_H
