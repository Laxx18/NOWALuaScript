#ifndef LUAHIGHLIGHTER_H
#define LUAHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextDocument>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextBlock>
#include <QQuickItem>
#include <QTimer>
#include <QPointer>
#include <QVector>
#include <QPair>

class LuaHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
public:
    explicit LuaHighlighter(QQuickItem* luaEditorTextEdit, QObject* parent = Q_NULLPTR);

    QTextCursor getCursor();

    void setErrorLine(int line, int start, int end);

    void clearErrors();

    void setRuntimeErrorLine(int line, int start, int end);

    void clearRuntimeErrors();

    void commentSelection();

    void uncommentSelection();

    void setCursorPosition(int cursorPosition);

    void addTabToSelection(void);

    void removeTabFromSelection(void);

    void breakLine(void);

    /**
     * @brief Marks all matches in yellow and reports the match count. Does not move the cursor.
     */
    void searchInTextEdit(const QString& searchText, bool wholeWord, bool caseSensitive);

    /**
     * @brief Selects the next (or previous) match, starting at the current match or at the cursor. Wraps around at the end of the document.
     */
    void searchNextInTextEdit(const QString& searchText, bool wholeWord, bool caseSensitive, bool backwards);

    /**
     * @brief Replaces the current match, if it is selected, and selects the next one. If no match is selected, only the next one is selected.
     */
    void replaceInTextEdit(const QString& searchText, const QString& replaceText, bool wholeWord, bool caseSensitive);

    /**
     * @brief Replaces all matches in one undo step. Returns the number of replacements.
     */
    int replaceAllInTextEdit(const QString& searchText, const QString& replaceText, bool wholeWord, bool caseSensitive);

    void clearSearch(void);

    /**
     * @brief Marks all occurrences of the given word (whole word, case sensitive) in light blue. Empty word clears it. Is not shown while a search is active.
     */
    void setWordHighlight(const QString& word);

    void undo(void);

    void redo(void);

    void insertSentText(int sizeToReplace, const QString& text);
Q_SIGNALS:
    void insertingNewLineChanged(bool isInserting);

    /**
     * @brief currentMatch is 1 based, 0 if no match is selected.
     */
    void resultSearch(int currentMatch, int matchCount);

    void resultReplaceAll(int replacedCount);
protected:
    void highlightBlock(const QString& text) override;

    void highlightMatchingBrackets();

    QPair<int, int> findMatchingBrackets(const QString& text, int cursorPos);
private:
    /**
     * @brief Builds the search expression and highlights the lines with the old and new matches again. Returns true, if something changed.
     */
    bool setSearchParameters(const QString& searchText, bool wholeWord, bool caseSensitive);

    /**
     * @brief All matches of the current search in the whole document as (start, length).
     */
    QVector<QPair<int, int>> collectMatches(void) const;

    void emitSearchResult(void);

    void selectMatch(int start, int length);

    void rehighlightBlockNumber(int blockNumber);

    /**
     * @brief Highlights the visible lines again, whose search or word highlight is outdated. Much faster than rehighlight() for big lua scripts.
     */
    void rehighlightVisibleStaleBlocks(void);

    QString getMarkState(void) const;

    QQuickItem* getFlickable(void);

    void applyBackground(int start, int length, const QColor& color);

    QPair<QTextBlock, QTextBlock> selectedBlocks(void) const;
private:
    struct HighlightingRule
    {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    QQuickItem* luaEditorTextEdit;

    QVector<HighlightingRule> highlightingRules;

    // 1 based line of the syntax error, -1 if there is none
    int errorLine;
    QTextCharFormat errorFormat;

    // The runtime error is bound to its block by a text cursor, so that it moves with the text, if lines are inserted above.
    QTextCursor runtimeErrorCursor;
    QString runtimeErrorBlockText;
    QTextCharFormat runtimeErrorFormat;

    QTextCursor cursor;
    int lastCursorBlockNumber;

    QString searchText;
    bool wholeWord;
    bool caseSensitive;
    QRegularExpression searchExpression;
    QTextCursor currentMatchCursor;
    QTimer* searchResultTimer;
    QTimer* visibleMarksTimer;
    QPointer<QQuickItem> flickable;

    QString wordHighlight;
    QRegularExpression wordHighlightExpression;
};

#endif // LUAHIGHLIGHTER_H
