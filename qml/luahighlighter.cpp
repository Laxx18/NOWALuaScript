#include "qml/luahighlighter.h"

#include <QQuickTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QStack>
#include <QDebug>

LuaHighlighter::LuaHighlighter(QQuickItem* luaEditorTextEdit, QObject* parent)
    : QSyntaxHighlighter{parent},
    luaEditorTextEdit(luaEditorTextEdit),
    errorLine(-1),
    lastCursorBlockNumber(-1),
    wholeWord(false),
    caseSensitive(true),
    searchResultTimer(Q_NULLPTR),
    visibleMarksTimer(Q_NULLPTR)
{
    this->errorFormat.setBackground(Qt::transparent); // No background
    this->errorFormat.setForeground(Qt::red); // Set error color to red
    this->errorFormat.setFontUnderline(true); // Underline the text
    // // Does not work
    // // this->errorFormat.setUnderlineStyle(QTextCharFormat::WaveUnderline);  // Squiggle underline

    this->runtimeErrorFormat.setBackground(Qt::transparent); // No background
    this->runtimeErrorFormat.setForeground(Qt::darkMagenta); // Set error color to red
    this->runtimeErrorFormat.setFontUnderline(true); // Underline the text

    HighlightingRule rule;

    // Define keyword formats
    QTextCharFormat keywordFormat;
    keywordFormat.setForeground(QColor("#00008B"));  // Navy Blue

    // Keywords pattern
    rule.pattern = QRegularExpression("\\b(if|else|elseif|while|for|end|function|local|return|do|then|break|in|repeat|until|and|or|not|true|false|nil)\\b");
    rule.format = keywordFormat;
    this->highlightingRules.append(rule);

    // Single-line comment format
    QTextCharFormat commentFormat;
    // commentFormat.setForeground(QColor("#696969"));  // Dim Gray
    commentFormat.setForeground(Qt::darkGreen);

    rule.pattern = QRegularExpression("(--[^\n]*)");
    rule.format = commentFormat;
    this->highlightingRules.append(rule);

    // Quotation format for single and double quotes
    QTextCharFormat quotationFormat;
    quotationFormat.setForeground(QColor("#008B8B"));  // Dark Cyan
    rule.pattern = QRegularExpression("\"[^\"]*\"|'[^']*'");
    rule.format = quotationFormat;
    this->highlightingRules.append(rule);

    // The match count is updated after the user stopped typing for a moment, and not for each key stroke.
    this->searchResultTimer = new QTimer(this);
    this->searchResultTimer->setSingleShot(true);
    this->searchResultTimer->setInterval(150);
    connect(this->searchResultTimer, &QTimer::timeout, this, &LuaHighlighter::emitSearchResult);

    // While scrolling, the lines which become visible are highlighted, if their search or word highlight is outdated
    this->visibleMarksTimer = new QTimer(this);
    this->visibleMarksTimer->setSingleShot(true);
    this->visibleMarksTimer->setInterval(30);
    connect(this->visibleMarksTimer, &QTimer::timeout, this, &LuaHighlighter::rehighlightVisibleStaleBlocks);

    if (Q_NULLPTR != this->document())
    {
        connect(this->document(), &QTextDocument::contentsChanged, this, [this]()
        {
            if (false == this->searchText.isEmpty())
            {
                this->searchResultTimer->start();
            }
        });
    }
}

namespace
{
    // Remembers per line, with which search or word highlight the line has been highlighted
    class MarkStateUserData : public QTextBlockUserData
    {
    public:
        QString markState;
        bool hasMarks = false;
    };
}

QString LuaHighlighter::getMarkState(void) const
{
    if (false == this->searchText.isEmpty())
    {
        return QStringLiteral("S") + (this->wholeWord ? "1" : "0") + (this->caseSensitive ? "1" : "0") + this->searchText;
    }
    return QStringLiteral("W") + this->wordHighlight;
}

QQuickItem* LuaHighlighter::getFlickable(void)
{
    if (true == this->flickable.isNull() && Q_NULLPTR != this->luaEditorTextEdit)
    {
        QQuickItem* item = this->luaEditorTextEdit->parentItem();
        while (Q_NULLPTR != item)
        {
            if (true == item->inherits("QQuickFlickable"))
            {
                this->flickable = item;
                // Lines, which become visible by scrolling, are highlighted then
                connect(item, SIGNAL(contentYChanged()), this->visibleMarksTimer, SLOT(start()));
                connect(item, SIGNAL(heightChanged()), this->visibleMarksTimer, SLOT(start()));
                break;
            }
            item = item->parentItem();
        }
    }
    return this->flickable.data();
}

void LuaHighlighter::rehighlightVisibleStaleBlocks(void)
{
    // Attention: rehighlight() lets the text edit layout the whole document again, which takes seconds for big lua scripts. Even highlighting each line with a match
    // takes long, if there are a lot of matches. Hence only the visible lines are highlighted again, if their search or word highlight is outdated.
    // The other lines are highlighted, when they are scrolled into view.
    if (Q_NULLPTR == this->document())
    {
        return;
    }

    QTextBlock firstBlock = this->document()->begin();
    QTextBlock lastBlock = this->document()->lastBlock();

    QQuickItem* flickableItem = this->getFlickable();
    if (Q_NULLPTR != flickableItem && Q_NULLPTR != this->luaEditorTextEdit)
    {
        const QPointF topLeft = this->luaEditorTextEdit->mapFromItem(flickableItem, QPointF(0.0, 0.0));
        const QPointF bottomLeft = this->luaEditorTextEdit->mapFromItem(flickableItem, QPointF(0.0, flickableItem->height()));

        int firstPosition = 0;
        int lastPosition = 0;
        QMetaObject::invokeMethod(this->luaEditorTextEdit, "positionAt", Q_RETURN_ARG(int, firstPosition), Q_ARG(qreal, 0.0), Q_ARG(qreal, topLeft.y()));
        QMetaObject::invokeMethod(this->luaEditorTextEdit, "positionAt", Q_RETURN_ARG(int, lastPosition), Q_ARG(qreal, 0.0), Q_ARG(qreal, bottomLeft.y()));

        // Some lines more, so that nothing outdated is seen while scrolling a bit
        const int margin = 50;
        const int firstBlockNumber = qMax(0, this->document()->findBlock(firstPosition).blockNumber() - margin);
        const int lastBlockNumber = qMin(this->document()->blockCount() - 1, this->document()->findBlock(lastPosition).blockNumber() + margin);

        firstBlock = this->document()->findBlockByNumber(firstBlockNumber);
        lastBlock = this->document()->findBlockByNumber(lastBlockNumber);
    }

    if (false == firstBlock.isValid() || false == lastBlock.isValid())
    {
        return;
    }

    const QString markState = this->getMarkState();
    const QRegularExpression& activeExpression = false == this->searchText.isEmpty() ? this->searchExpression : this->wordHighlightExpression;
    const bool hasActiveExpression = false == activeExpression.pattern().isEmpty() && true == activeExpression.isValid();

    for (QTextBlock block = firstBlock; true == block.isValid(); block = block.next())
    {
        MarkStateUserData* userData = static_cast<MarkStateUserData*>(block.userData());
        if (Q_NULLPTR == userData || userData->markState != markState)
        {
            const bool needsMarks = true == hasActiveExpression && true == activeExpression.match(block.text()).hasMatch();
            if (Q_NULLPTR == userData || true == userData->hasMarks || true == needsMarks)
            {
                // Sets the user data in highlightBlock
                this->rehighlightBlock(block);
            }
            else
            {
                // Nothing to see in this line, neither before nor now
                userData->markState = markState;
            }
        }

        if (block == lastBlock)
        {
            break;
        }
    }
}

void LuaHighlighter::rehighlightBlockNumber(int blockNumber)
{
    if (blockNumber < 0 || Q_NULLPTR == this->document())
    {
        return;
    }

    QTextBlock block = this->document()->findBlockByNumber(blockNumber);
    if (true == block.isValid())
    {
        this->rehighlightBlock(block);
    }
}

void LuaHighlighter::setErrorLine(int line, int start, int end)
{
    Q_UNUSED(start)
    Q_UNUSED(end)

    if (this->errorLine == line)
    {
        return;  // Same error, no need to rehighlight
    }

    // Only the old and the new line must be highlighted again, not the whole document
    int oldErrorLine = this->errorLine;
    this->errorLine = line;
    this->rehighlightBlockNumber(oldErrorLine - 1);
    this->rehighlightBlockNumber(this->errorLine - 1);
}

void LuaHighlighter::clearErrors()
{
    // Attention: Is called after each valid syntax check (each second while typing). Formerly the whole document has been highlighted again each time, which made the editor slow with a lot of text.
    if (-1 == this->errorLine)
    {
        return;
    }

    int oldErrorLine = this->errorLine;
    this->errorLine = -1;
    this->rehighlightBlockNumber(oldErrorLine - 1);
}

void LuaHighlighter::setRuntimeErrorLine(int line, int start, int end)
{
    Q_UNUSED(start)
    Q_UNUSED(end)

    if (line <= 0 || Q_NULLPTR == this->document())
    {
        return;
    }

    QTextBlock block = this->document()->findBlockByNumber(line - 1);
    if (false == block.isValid())
    {
        return;
    }

    if (false == this->runtimeErrorCursor.isNull() && this->runtimeErrorCursor.block() == block && this->runtimeErrorBlockText == block.text())
    {
        return;  // Same error, no need to rehighlight
    }

    this->clearRuntimeErrors();

    this->runtimeErrorCursor = QTextCursor(block);
    this->runtimeErrorBlockText = block.text();
    this->rehighlightBlock(block);
}

void LuaHighlighter::clearRuntimeErrors()
{
    if (true == this->runtimeErrorCursor.isNull())
    {
        return;
    }

    QTextBlock oldBlock = this->runtimeErrorCursor.block();
    this->runtimeErrorCursor = QTextCursor();
    this->runtimeErrorBlockText.clear();

    if (true == oldBlock.isValid())
    {
        this->rehighlightBlock(oldBlock);
    }
}

QPair<QTextBlock, QTextBlock> LuaHighlighter::selectedBlocks(void) const
{
    QTextBlock startBlock = this->cursor.block();
    QTextBlock endBlock = this->cursor.block();

    if (true == this->cursor.hasSelection())
    {
        startBlock = this->cursor.document()->findBlock(this->cursor.selectionStart());
        endBlock = this->cursor.document()->findBlock(this->cursor.selectionEnd());

        // If the selection ends at the very start of a line (e.g. whole lines selected with the mouse), that line is not part of the selection
        if (endBlock != startBlock && this->cursor.selectionEnd() == endBlock.position())
        {
            endBlock = endBlock.previous();
        }
    }

    return qMakePair(startBlock, endBlock);
}

void LuaHighlighter::commentSelection()
{
    // Attention: Always take the current cursor and selection of the text edit. The stored cursor may be outdated, e.g. after a selection with the mouse.
    this->cursor = this->getCursor();

    const auto blocks = this->selectedBlocks();
    QTextBlock startBlock = blocks.first;
    QTextBlock endBlock = blocks.second;

    this->cursor.beginEditBlock();

    for (QTextBlock block = startBlock; block.isValid() && block != endBlock.next(); block = block.next())
    {
        QString blockText = block.text();

        // Only comment if the line is not already commented
        if (!blockText.trimmed().isEmpty() && !blockText.trimmed().startsWith("--"))
        {
            QTextCursor blockCursor(block);
            blockCursor.movePosition(QTextCursor::StartOfBlock);

            // Get the leading whitespace
            QString leadingWhitespace = blockText.left(blockText.indexOf(blockText.trimmed()));

            // Select the entire line to replace it
            blockCursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, blockText.length());

            // Remove the original line
            blockCursor.removeSelectedText();
            // Insert the comment marker without additional space, followed by the original text
            blockCursor.insertText(leadingWhitespace + "--" + blockText.trimmed()); // Ensure no extra spaces
        }
    }

    this->cursor.endEditBlock();
}

void LuaHighlighter::uncommentSelection()
{
    this->cursor = this->getCursor();

    const auto blocks = this->selectedBlocks();
    QTextBlock startBlock = blocks.first;
    QTextBlock endBlock = blocks.second;

    static const QRegularExpression commentRegex("^\\s*--");

    this->cursor.beginEditBlock();

    for (QTextBlock block = startBlock; block.isValid() && block != endBlock.next(); block = block.next())
    {
        QString blockText = block.text();

        // If the line starts with "--", uncomment it
        if (commentRegex.match(blockText).hasMatch())
        {
            // Find the index of the comment marker
            int commentIndex = blockText.indexOf("--");
            // Get the leading whitespace
            QString leadingWhitespace = blockText.left(commentIndex);
            // Get the uncommented text, preserving original spacing
            QString uncommentedText = blockText.mid(commentIndex + 2).trimmed(); // Get the rest of the line after "--"

            // Create a cursor for the current block
            QTextCursor blockCursor(block);
            blockCursor.movePosition(QTextCursor::StartOfBlock);
            // Select the entire line including the leading whitespace and comment
            blockCursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, blockText.length()); // Select entire line

            // Remove the selected text
            blockCursor.removeSelectedText();

            // Insert the uncommented text with leading whitespace, preserving original alignment
            blockCursor.insertText(leadingWhitespace + uncommentedText); // Insert leading whitespace followed by uncommented text
        }
    }

    this->cursor.endEditBlock();
}

void LuaHighlighter::highlightMatchingBrackets()
{
    // Get the position of the cursor relative to the current block.
    int cursorPos = this->cursor.position() - this->cursor.block().position();  // Position relative to the block
    if (cursorPos < 0 || cursorPos >= this->cursor.block().length())
    {
        return;
    }

    QString text = this->cursor.block().text();

    // Find the matching brackets, if any.
    QPair<int, int> bracketPair = this->findMatchingBrackets(text, cursorPos);

    if (bracketPair.first != -1 && bracketPair.second != -1)
    {
        QTextCharFormat matchFormat;
        matchFormat.setBackground(QColor("#377dbd"));  // Customize the appearance of matched brackets.
        matchFormat.setFontWeight(QFont::Bold);  // Set the matching brackets to bold.

        // Highlight the matching brackets.
        setFormat(bracketPair.first, 1, matchFormat);   // Highlight the opening bracket
        setFormat(bracketPair.second, 1, matchFormat);  // Highlight the closing bracket
    }
}

void LuaHighlighter::undo()
{
    if (!this->cursor.isNull() && this->cursor.document())
    {
        // The QTextCursor overload moves this->cursor to where the undo was applied.
        // We then push that position back to the QML TextEdit so onCursorRectangleChanged
        // fires and the Flickable scrolls to the undo location.
        // Note: The search must no longer be cleared before. The crash came from the old search, which changed the document inside highlightBlock.
        this->cursor.document()->undo(&this->cursor);
        this->luaEditorTextEdit->setProperty("cursorPosition", this->cursor.position());
    }
}

void LuaHighlighter::redo()
{
    if (!this->cursor.isNull() && this->cursor.document())
    {
        this->cursor.document()->redo(&this->cursor);
        this->luaEditorTextEdit->setProperty("cursorPosition", this->cursor.position());
    }
}

void LuaHighlighter::insertSentText(int sizeToReplace, const QString& text)
{
    this->cursor = this->getCursor();
    if (true == this->cursor.hasSelection())
    {
        this->cursor.setPosition(this->cursor.position());
    }

    // Begin an edit block to group changes for undo/redo
    this->cursor.beginEditBlock();

    QTextCursor cursor = this->cursor;

    cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, sizeToReplace);

    // Replace selected text with the new auto-completed text
    cursor.insertText(text);

    // Update the editor cursor
    this->cursor = cursor;  // Update the cursor after the replacement

    // End the edit block to finalize changes (allows for undo/redo)
    this->cursor.endEditBlock();
}

QPair<int, int> LuaHighlighter::findMatchingBrackets(const QString& text, int cursorPos)
{
    if (cursorPos < 0 || cursorPos >= text.length()) return QPair<int, int>(-1, -1);

    QChar currentChar = text.at(cursorPos);

    if (currentChar == '(')
    {
        int depth = 1;
        for (int i = cursorPos + 1; i < text.length(); ++i)
        {
            QChar ch = text.at(i);
            if (ch == '(') depth++;
            if (ch == ')') depth--;
            if (depth == 0) return QPair<int, int>(cursorPos, i);  // Match found
        }
    }
    else if (currentChar == ')')
    {
        int depth = 1;
        for (int i = cursorPos - 1; i >= 0; --i)
        {
            QChar ch = text.at(i);
            if (ch == ')') depth++;
            if (ch == '(') depth--;
            if (depth == 0) return QPair<int, int>(i, cursorPos);  // Match found
        }
    }
    return QPair<int, int>(-1, -1);  // No match found
}

QTextCursor LuaHighlighter::getCursor()
{
    // Set up QTextCursor from the current text document
    QTextCursor cursor(document());

    if (Q_NULLPTR == this->luaEditorTextEdit || Q_NULLPTR == document())
    {
        return cursor;
    }

    const int maxPosition = qMax(0, document()->characterCount() - 1);

    int cursorPosition = this->luaEditorTextEdit->property("cursorPosition").toInt();
    if (cursorPosition < 0)
    {
        return cursor;
    }
    cursorPosition = qMin(cursorPosition, maxPosition);

    int selectionStart = qBound(0, this->luaEditorTextEdit->property("selectionStart").toInt(), maxPosition);
    int selectionEnd = qBound(0, this->luaEditorTextEdit->property("selectionEnd").toInt(), maxPosition);

    if (selectionStart != selectionEnd)
    {
        // Keep the direction of the selection: the anchor is the end, which is not the cursor
        int anchor = (cursorPosition == selectionStart) ? selectionEnd : selectionStart;
        cursor.setPosition(anchor);
        cursor.setPosition(cursorPosition, QTextCursor::KeepAnchor);
    }
    else
    {
        cursor.setPosition(cursorPosition);
    }

    return cursor;
}

void LuaHighlighter::setCursorPosition(int cursorPosition)
{
    Q_UNUSED(cursorPosition);
    this->cursor = this->getCursor();

    // Only the line with the cursor and the line where the cursor has been before are highlighted again (bracket matching)
    int blockNumber = this->cursor.block().blockNumber();
    if (blockNumber != this->lastCursorBlockNumber)
    {
        this->rehighlightBlockNumber(this->lastCursorBlockNumber);
    }
    this->lastCursorBlockNumber = blockNumber;

    QString text = this->cursor.block().text();

    int cursorPos = this->cursor.position() - this->cursor.block().position();  // Position relative to the block
    if (cursorPos < 0 || cursorPos >= this->cursor.block().length() || cursorPos >= text.length())
    {
        return;
    }

    this->rehighlightBlock(this->cursor.block());  // Reapply syntax highlighting
}

void LuaHighlighter::addTabToSelection()
{
    // Attention: Always take the current cursor and selection of the text edit. The stored cursor is only updated, when the cursor position changes,
    // so a selection made e.g. with Ctrl+A or by dragging back to the same position has not been known, and Tab did nothing to the selected lines.
    this->cursor = this->getCursor();

    const auto blocks = this->selectedBlocks();
    bool isMultiLine = this->cursor.hasSelection() && blocks.first != blocks.second;

    QString indent = QString(4, ' ');

    this->cursor.beginEditBlock();

    if (true == isMultiLine)
    {
        // Indent all selected blocks from their start
        for (QTextBlock block = blocks.first; block.isValid() && block != blocks.second.next(); block = block.next())
        {
            QTextCursor bc(block);
            bc.movePosition(QTextCursor::StartOfBlock);
            bc.insertText(indent);
        }
    }
    else
    {
        // No selection or intra-line: insert tab at current cursor position
        QTextCursor c = this->cursor;
        if (c.hasSelection())
        {
            c.clearSelection();
        }
        c.insertText(indent);
        this->cursor = c;
    }

    this->cursor.endEditBlock();
}

void LuaHighlighter::removeTabFromSelection()
{
    this->cursor = this->getCursor();

    const auto blocks = this->selectedBlocks();

    this->cursor.beginEditBlock();

    // Iterate over each selected block and remove up to 4 spaces or one tab
    for (QTextBlock block = blocks.first; block.isValid() && block != blocks.second.next(); block = block.next())
    {
        QString blockText = block.text();
        int charsToRemove = 0;

        if (false == blockText.isEmpty() && blockText.at(0) == '\t')
        {
            charsToRemove = 1;
        }
        else
        {
            // Count how many leading spaces the block has
            while (charsToRemove < blockText.length() && blockText.at(charsToRemove) == ' ' && charsToRemove < 4)
            {
                charsToRemove++;
            }
        }

        if (charsToRemove > 0)
        {
            // Remove the leading spaces from the block
            QTextCursor blockCursor(block);
            blockCursor.movePosition(QTextCursor::StartOfBlock);
            blockCursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, charsToRemove);
            blockCursor.removeSelectedText();
        }
    }

    this->cursor.endEditBlock();
}

void LuaHighlighter::breakLine()
{
    this->cursor = this->getCursor();

    Q_EMIT insertingNewLineChanged(true); // Emit signal before inserting a new line

    this->cursor.beginEditBlock();

    // Return replaces a selected text, like in every other editor
    if (true == this->cursor.hasSelection())
    {
        this->cursor.removeSelectedText();
    }

    // Get the current block and determine the cursor's position within it
    QTextBlock currentBlock = this->cursor.block();
    int positionInBlock = this->cursor.position() - currentBlock.position();

    // Get the text of the current line up to the cursor's position
    QString currentBlockText = currentBlock.text().left(positionInBlock);

    // Insert a line break at the cursor's current position
    this->cursor.insertText("\n");

    // Determine indentation level by counting leading spaces in the current line up to the cursor position
    int indentationLevel = 0;
    while (indentationLevel < currentBlockText.length() && currentBlockText[indentationLevel] == ' ')
    {
        indentationLevel++;
    }

    // Insert the same number of spaces at the start of the new line
    if (indentationLevel > 0)
    {
        this->cursor.insertText(QString(indentationLevel, ' '));
    }

    this->cursor.endEditBlock();

    Q_EMIT insertingNewLineChanged(false); // Emit signal after the new line is inserted
}

bool LuaHighlighter::setSearchParameters(const QString& searchText, bool wholeWord, bool caseSensitive)
{
    // A search is always within one line
    QString cleanSearchText = searchText;
    int newLinePos = cleanSearchText.indexOf(QChar('\n'));
    if (-1 != newLinePos)
    {
        cleanSearchText = cleanSearchText.left(newLinePos);
    }

    if (this->searchText == cleanSearchText && this->wholeWord == wholeWord && this->caseSensitive == caseSensitive)
    {
        return false;
    }

    // The line with the old current match must be highlighted again
    QTextBlock oldCurrentMatchBlock;
    if (false == this->currentMatchCursor.isNull())
    {
        oldCurrentMatchBlock = this->currentMatchCursor.block();
    }

    this->searchText = cleanSearchText;
    this->wholeWord = wholeWord;
    this->caseSensitive = caseSensitive;
    this->currentMatchCursor = QTextCursor();

    if (true == this->searchText.isEmpty())
    {
        this->searchExpression = QRegularExpression();
        this->rehighlightVisibleStaleBlocks();
        if (true == oldCurrentMatchBlock.isValid())
        {
            this->rehighlightBlock(oldCurrentMatchBlock);
        }
        return true;
    }

    // The search text is always taken literally, so that e.g. "a.b" or "(x)" does not act as regular expression
    QString pattern = QRegularExpression::escape(this->searchText);
    if (true == this->wholeWord)
    {
        // Lua identifiers consist of letters, digits and underscore
        pattern = "(?<![A-Za-z0-9_])" + pattern + "(?![A-Za-z0-9_])";
    }

    QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
    if (false == this->caseSensitive)
    {
        options |= QRegularExpression::CaseInsensitiveOption;
    }

    this->searchExpression = QRegularExpression(pattern, options);

    this->rehighlightVisibleStaleBlocks();
    if (true == oldCurrentMatchBlock.isValid())
    {
        this->rehighlightBlock(oldCurrentMatchBlock);
    }
    return true;
}

QVector<QPair<int, int>> LuaHighlighter::collectMatches(void) const
{
    QVector<QPair<int, int>> matches;

    if (true == this->searchText.isEmpty() || false == this->searchExpression.isValid() || Q_NULLPTR == this->document())
    {
        return matches;
    }

    // Note: toPlainText has the same character positions as the document, each line break counts as one character
    const QString text = this->document()->toPlainText();

    QRegularExpressionMatchIterator it = this->searchExpression.globalMatch(text);
    while (it.hasNext())
    {
        QRegularExpressionMatch match = it.next();
        if (match.capturedLength() > 0)
        {
            matches.append(qMakePair(match.capturedStart(), match.capturedLength()));
        }
    }

    return matches;
}

void LuaHighlighter::emitSearchResult(void)
{
    if (true == this->searchText.isEmpty())
    {
        Q_EMIT resultSearch(0, 0);
        return;
    }

    const auto matches = this->collectMatches();

    int currentMatch = 0;
    if (false == this->currentMatchCursor.isNull() && true == this->currentMatchCursor.hasSelection())
    {
        const int start = this->currentMatchCursor.selectionStart();
        const int length = this->currentMatchCursor.selectionEnd() - start;
        for (int i = 0; i < matches.size(); ++i)
        {
            if (matches[i].first == start && matches[i].second == length)
            {
                currentMatch = i + 1;
                break;
            }
        }
    }

    Q_EMIT resultSearch(currentMatch, matches.size());
}

void LuaHighlighter::selectMatch(int start, int length)
{
    int oldBlockNumber = -1;
    if (false == this->currentMatchCursor.isNull())
    {
        oldBlockNumber = this->currentMatchCursor.block().blockNumber();
    }

    this->currentMatchCursor = QTextCursor(this->document());
    this->currentMatchCursor.setPosition(start);
    this->currentMatchCursor.setPosition(start + length, QTextCursor::KeepAnchor);

    // Selects the match in the text edit. The text edit scrolls to the cursor (onCursorRectangleChanged in LuaEditor.qml).
    // The selection stays visible, even if the focus is in the search field (persistentSelection).
    QMetaObject::invokeMethod(this->luaEditorTextEdit, "select", Q_ARG(int, start), Q_ARG(int, start + length));

    this->rehighlightBlockNumber(oldBlockNumber);
    this->rehighlightBlock(this->currentMatchCursor.block());
}

void LuaHighlighter::clearSearch()
{
    this->searchResultTimer->stop();

    if (true == this->searchText.isEmpty())
    {
        return;
    }

    QTextBlock oldCurrentMatchBlock;
    if (false == this->currentMatchCursor.isNull())
    {
        oldCurrentMatchBlock = this->currentMatchCursor.block();
    }

    this->searchText.clear();
    this->searchExpression = QRegularExpression();
    this->currentMatchCursor = QTextCursor();

    // The old matches disappear and the word highlight is shown again
    this->rehighlightVisibleStaleBlocks();
    if (true == oldCurrentMatchBlock.isValid())
    {
        this->rehighlightBlock(oldCurrentMatchBlock);
    }

    Q_EMIT resultSearch(0, 0);
}

void LuaHighlighter::searchInTextEdit(const QString& searchText, bool wholeWord, bool caseSensitive)
{
    if (true == searchText.isEmpty())
    {
        this->clearSearch();
        return;
    }

    // Highlights the lines with the old and new matches again, if something changed
    this->setSearchParameters(searchText, wholeWord, caseSensitive);

    this->emitSearchResult();
}

void LuaHighlighter::searchNextInTextEdit(const QString& searchText, bool wholeWord, bool caseSensitive, bool backwards)
{
    if (true == searchText.isEmpty())
    {
        this->clearSearch();
        return;
    }

    // Highlights the lines with the old and new matches again, if something changed
    this->setSearchParameters(searchText, wholeWord, caseSensitive);

    const auto matches = this->collectMatches();
    if (true == matches.isEmpty())
    {
        Q_EMIT resultSearch(0, 0);
        return;
    }

    // Start at the current match, or if there is none, at the cursor of the text edit
    int fromStart = -1;
    int fromEnd = -1;
    bool hasCurrentMatch = false == this->currentMatchCursor.isNull() && true == this->currentMatchCursor.hasSelection();
    if (true == hasCurrentMatch)
    {
        fromStart = this->currentMatchCursor.selectionStart();
        fromEnd = this->currentMatchCursor.selectionEnd();
    }
    else
    {
        QTextCursor editCursor = this->getCursor();
        fromStart = editCursor.selectionStart();
        fromEnd = editCursor.selectionStart();
    }

    int index = -1;
    if (false == backwards)
    {
        for (int i = 0; i < matches.size(); ++i)
        {
            // Without a current match, a match at the cursor or around the cursor (e.g. the word taken over by Ctrl+F) is the first one
            if ((true == hasCurrentMatch && matches[i].first >= fromEnd) || (false == hasCurrentMatch && matches[i].first + matches[i].second > fromEnd))
            {
                index = i;
                break;
            }
        }
        if (-1 == index)
        {
            index = 0; // Wrap around
        }
    }
    else
    {
        for (int i = matches.size() - 1; i >= 0; --i)
        {
            if (matches[i].first < fromStart)
            {
                index = i;
                break;
            }
        }
        if (-1 == index)
        {
            index = matches.size() - 1; // Wrap around
        }
    }

    this->selectMatch(matches[index].first, matches[index].second);

    Q_EMIT resultSearch(index + 1, matches.size());
}

void LuaHighlighter::replaceInTextEdit(const QString& searchText, const QString& replaceText, bool wholeWord, bool caseSensitive)
{
    if (true == searchText.isEmpty())
    {
        return;
    }

    // Highlights the lines with the old and new matches again, if something changed
    this->setSearchParameters(searchText, wholeWord, caseSensitive);

    // Only replace, if the current match is still a real match at exactly this place. Else just go to the next match, so that the user sees, what will be replaced.
    if (false == this->currentMatchCursor.isNull() && true == this->currentMatchCursor.hasSelection())
    {
        const int start = this->currentMatchCursor.selectionStart();
        const int length = this->currentMatchCursor.selectionEnd() - start;

        const auto matches = this->collectMatches();
        bool isMatch = false;
        for (const auto& match : matches)
        {
            if (match.first == start && match.second == length)
            {
                isMatch = true;
                break;
            }
        }

        if (true == isMatch)
        {
            QTextCursor replaceCursor(this->document());
            replaceCursor.setPosition(start);
            replaceCursor.setPosition(start + length, QTextCursor::KeepAnchor);
            replaceCursor.beginEditBlock();
            replaceCursor.insertText(replaceText);
            replaceCursor.endEditBlock();

            // Continue behind the replaced text, so that a replace text, which contains the search text, is not found again
            this->currentMatchCursor = QTextCursor();
            QMetaObject::invokeMethod(this->luaEditorTextEdit, "select", Q_ARG(int, start + replaceText.length()), Q_ARG(int, start + replaceText.length()));
        }
    }

    this->searchNextInTextEdit(searchText, wholeWord, caseSensitive, false);
}

int LuaHighlighter::replaceAllInTextEdit(const QString& searchText, const QString& replaceText, bool wholeWord, bool caseSensitive)
{
    if (true == searchText.isEmpty())
    {
        Q_EMIT resultReplaceAll(0);
        return 0;
    }

    // Highlights the lines with the old and new matches again, if something changed
    this->setSearchParameters(searchText, wholeWord, caseSensitive);

    const auto matches = this->collectMatches();
    if (true == matches.isEmpty())
    {
        Q_EMIT resultReplaceAll(0);
        this->emitSearchResult();
        return 0;
    }

    this->currentMatchCursor = QTextCursor();

    // All replacements are one undo step. From the end to the start, so that the positions of the remaining matches stay valid.
    QTextCursor replaceCursor(this->document());
    replaceCursor.beginEditBlock();
    for (int i = matches.size() - 1; i >= 0; --i)
    {
        replaceCursor.setPosition(matches[i].first);
        replaceCursor.setPosition(matches[i].first + matches[i].second, QTextCursor::KeepAnchor);
        replaceCursor.insertText(replaceText);
    }
    replaceCursor.endEditBlock();

    Q_EMIT resultReplaceAll(matches.size());
    this->emitSearchResult();

    return matches.size();
}

void LuaHighlighter::setWordHighlight(const QString& word)
{
    QString cleanWord = word.trimmed();
    if (cleanWord.contains(QChar('\n')) || cleanWord.contains(QChar(QChar::ParagraphSeparator)))
    {
        cleanWord.clear();
    }

    if (this->wordHighlight == cleanWord)
    {
        return;
    }

    this->wordHighlight = cleanWord;
    if (false == this->wordHighlight.isEmpty())
    {
        this->wordHighlightExpression = QRegularExpression("(?<![A-Za-z0-9_])" + QRegularExpression::escape(this->wordHighlight) + "(?![A-Za-z0-9_])");
    }
    else
    {
        this->wordHighlightExpression = QRegularExpression();
    }

    // During a search the word highlight is not shown
    if (true == this->searchText.isEmpty())
    {
        this->rehighlightVisibleStaleBlocks();
    }
}

void LuaHighlighter::applyBackground(int start, int length, const QColor& color)
{
    // Only the background is changed, so that the syntax colors stay visible
    for (int i = start; i < start + length; ++i)
    {
        QTextCharFormat charFormat = this->format(i);
        charFormat.setBackground(color);
        this->setFormat(i, 1, charFormat);
    }
}

void LuaHighlighter::highlightBlock(const QString& text)
{
    // Attention: Never change the document in here. The old search replaced text in here, which crashed on undo.

    // Apply highlighting rules
    for (const HighlightingRule& rule : std::as_const(this->highlightingRules))
    {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text, QRegularExpression::NormalMatch);
        while (matchIterator.hasNext())
        {
            const QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    const int currentBlockNumber = currentBlock().blockNumber();

    // Only apply error formatting if we are on the error line
    if (this->errorLine != -1 && currentBlockNumber == this->errorLine - 1)
    {
        // Format is per block! so the current block starts with 0, the next one too!
        setFormat(0, text.length(), this->errorFormat);
    }

    // The runtime error is shown, as long as its line has not been changed. If the user edits the line, it is most likely being fixed.
    if (false == this->runtimeErrorCursor.isNull() && this->runtimeErrorCursor.block() == currentBlock() && text == this->runtimeErrorBlockText)
    {
        setFormat(0, text.length(), this->runtimeErrorFormat);
    }

    // Bracket highlighting logic
    QStack<int> bracketStack;

    // Loop over the text to check for unbalanced brackets
    for (int i = 0; i < text.length(); ++i)
    {
        QChar ch = text.at(i);

        // Track opening brackets
        if (ch == '(')
        {
            bracketStack.push(i);  // Store the position of opening brackets
        }
        // Match closing brackets
        else if (ch == ')')
        {
            if (!bracketStack.isEmpty())
            {
                bracketStack.pop();  // Match found, pop the corresponding opening bracket
            }
        }
    }

    // If there are unmatched opening brackets, mark them red
    while (!bracketStack.isEmpty())
    {
        int pos = bracketStack.pop();  // Get the position of the unbalanced opening bracket
        setFormat(pos, 1, this->errorFormat);  // Highlight the opening bracket in red
    }

    // Only match for the current block (line)
    if (false == this->cursor.isNull() && this->cursor.block() == currentBlock())
    {
        this->highlightMatchingBrackets();
    }

    bool hasMarks = false;
    if (false == this->searchText.isEmpty() && true == this->searchExpression.isValid())
    {
        // All matches yellow
        QRegularExpressionMatchIterator it = this->searchExpression.globalMatch(text);
        while (it.hasNext())
        {
            QRegularExpressionMatch match = it.next();
            this->applyBackground(match.capturedStart(), match.capturedLength(), QColor(Qt::yellow));
            hasMarks = true;
        }

        // The current match orange
        if (false == this->currentMatchCursor.isNull() && true == this->currentMatchCursor.hasSelection())
        {
            const int blockPosition = currentBlock().position();
            const int start = this->currentMatchCursor.selectionStart() - blockPosition;
            const int end = this->currentMatchCursor.selectionEnd() - blockPosition;
            if (start >= 0 && end <= text.length() && end > start)
            {
                this->applyBackground(start, end - start, QColor("#FFA500"));
                hasMarks = true;
            }
        }
    }
    else if (false == this->wordHighlight.isEmpty() && true == this->wordHighlightExpression.isValid())
    {
        QRegularExpressionMatchIterator it = this->wordHighlightExpression.globalMatch(text);
        while (it.hasNext())
        {
            QRegularExpressionMatch match = it.next();
            this->applyBackground(match.capturedStart(), match.capturedLength(), QColor("#ADD8E6"));
            hasMarks = true;
        }
    }

    // Remember, with which search or word highlight this line has been highlighted (see rehighlightVisibleStaleBlocks)
    MarkStateUserData* userData = static_cast<MarkStateUserData*>(this->currentBlockUserData());
    if (Q_NULLPTR == userData)
    {
        userData = new MarkStateUserData();
        this->setCurrentBlockUserData(userData);
    }
    userData->markState = this->getMarkState();
    userData->hasMarks = hasMarks;
}
