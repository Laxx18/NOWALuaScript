#include "qml/luaeditorqml.h"

#include "model/luaeditormodelitem.h"

#include <QTextDocument>
#include <QFontMetrics>
#include <QQuickWindow>
#include <QDebug>

LuaEditorQml::LuaEditorQml(QQuickItem* parent)
    : QQuickItem(parent),
    luaEditorModelItem(Q_NULLPTR),
    lineNumbersEdit(Q_NULLPTR),
    luaEditorTextEdit(Q_NULLPTR),
    quickTextDocument(Q_NULLPTR),
    highlighter(Q_NULLPTR),
    scrollY(0.0f),
    isAfterColon(false),
    isAfterDot(false),
    lastColonIndex(-1),
    lastDotIndex(-1),
    oldCursorPosition(-1),
    cursorPosition(0),
    isInMatchedFunctionProcessing(false)
{
    connect(this, &QQuickItem::parentChanged, this, &LuaEditorQml::onParentChanged);

    // Make sure the item is focusable and accepts key input
    // setFlag(QQuickItem::ItemHasContents, true);
    // setAcceptedMouseButtons(Qt::AllButtons);
    // setFlag(QQuickItem::ItemAcceptsInputMethod, true);
    // setFocus(true);  // Ensure it can receive key events
    // setFocusPolicy(Qt::StrongFocus);
}

LuaEditorModelItem* LuaEditorQml::model() const
{
    return this->luaEditorModelItem;
}

void LuaEditorQml::setModel(LuaEditorModelItem* luaEditorModelItem)
{
    if (this->luaEditorModelItem == luaEditorModelItem)
    {
       return;
    }

    this->luaEditorModelItem = luaEditorModelItem;

    connect(this->luaEditorModelItem, &LuaEditorModelItem::signal_commentLines, this, [this] {
        this->highlighter->commentSelection();
        this->resetTextAfterColon();
        this->resetTextAfterDot();
    });

    connect(this->luaEditorModelItem, &LuaEditorModelItem::signal_unCommentLines, this, [this] {
        this->highlighter->uncommentSelection();
        this->resetTextAfterColon();
        this->resetTextAfterDot();
    });

    connect(this->luaEditorModelItem, &LuaEditorModelItem::signal_addTabToSelection, this, [this] {
        this->highlighter->addTabToSelection();
        this->resetTextAfterColon();
        this->resetTextAfterDot();
    });

    connect(this->luaEditorModelItem, &LuaEditorModelItem::signal_removeTabFromSelection, this, [this] {
        this->highlighter->removeTabFromSelection();
        this->resetTextAfterColon();
        this->resetTextAfterDot();
    });

    connect(this->luaEditorModelItem, &LuaEditorModelItem::signal_breakLine, this, [this] {
        this->highlighter->breakLine();
        this->resetTextAfterColon();
        this->resetTextAfterDot();
    });

    connect(this->luaEditorModelItem, &LuaEditorModelItem::signal_searchInTextEdit, this, [this](const QString& searchText, bool wholeWord, bool caseSensitive) {
        this->luaEditorTextEdit->forceActiveFocus();
        this->highlighter->searchInTextEdit(searchText, wholeWord, caseSensitive);
    });

    connect(this->luaEditorModelItem, &LuaEditorModelItem::signal_replaceInTextEdit, this, [this](const QString& searchText, const QString& replaceText) {
        this->luaEditorTextEdit->forceActiveFocus();
        this->highlighter->replaceInTextEdit(searchText, replaceText);
        this->resetTextAfterColon();
        this->resetTextAfterDot();
    });

    connect(this->luaEditorModelItem, &LuaEditorModelItem::signal_clearSearch, this, [this] {
        this->luaEditorTextEdit->forceActiveFocus();
        this->highlighter->clearSearch();
    });

    connect(this->luaEditorModelItem, &LuaEditorModelItem::signal_undo, this, [this] {
        this->luaEditorTextEdit->forceActiveFocus();
        this->highlighter->undo();
        this->resetTextAfterColon();
        this->resetTextAfterDot();
    });

    connect(this->luaEditorModelItem, &LuaEditorModelItem::signal_redo, this, [this] {
        this->luaEditorTextEdit->forceActiveFocus();
        this->highlighter->redo();
        this->resetTextAfterColon();
        this->resetTextAfterDot();
    });

    connect(this->luaEditorModelItem, &LuaEditorModelItem::signal_sendTextToEditor, this, [this](const QString& text) {
        // this->showIntelliSenseContextMenuAtCursor(false, text);
        this->luaEditorTextEdit->forceActiveFocus();
        if (true == this->isAfterColon)
        {
            this->highlighter->insertSentText(this->typedAfterColon.size(), text);
            // Actualize the typed after colon text, to still show the intelisense for the method
            this->typedAfterColon = text;
        }
        else if (true == this->isAfterDot)
        {
            this->highlighter->insertSentText(this->typedAfterDot.size(), text);
            // Actualize the typed after colon text, to still show the intelisense for the method
            this->typedAfterDot = text;
            Q_EMIT requestCloseIntellisense();
        }

        this->oldCursorPosition = this->cursorPosition - 1;
    });

    connect(this->luaEditorModelItem, &LuaEditorModelItem::signal_sendVariableTextToEditor, this, [this](const QString& text) {
        this->luaEditorTextEdit->forceActiveFocus();

        this->highlighter->insertSentText(this->currentLineText.size(), text);
        this->currentLineText = text;
        Q_EMIT requestCloseIntellisense();

        this->oldCursorPosition = this->cursorPosition - 1;
    });

    connect(this, &LuaEditorQml::requestIntellisenseProcessing, this->luaEditorModelItem, &LuaEditorModelItem::startIntellisenseProcessing);
    connect(this, &LuaEditorQml::requestCloseIntellisense, this->luaEditorModelItem, &LuaEditorModelItem::closeIntellisense);

    connect(this, &LuaEditorQml::requestMatchedForVariablesProcessing, this->luaEditorModelItem, &LuaEditorModelItem::startMatchedVariablesProcessing);


    connect(this, &LuaEditorQml::requestMatchedFunctionContextMenu, this->luaEditorModelItem, &LuaEditorModelItem::startMatchedFunctionProcessing);
    connect(this, &LuaEditorQml::requestCloseMatchedFunctionContextMenu, this->luaEditorModelItem, &LuaEditorModelItem::closeMatchedFunction);


    connect(this->highlighter, &LuaHighlighter::insertingNewLineChanged, this, [this](bool inserting) {
        Q_EMIT signal_insertingNewLine(inserting);
    });

    Q_EMIT modelChanged();
}

void LuaEditorQml::onParentChanged(QQuickItem* newParent)
{
    if (Q_NULLPTR == newParent)
    {
        return;
    }

    if (Q_NULLPTR != this->highlighter)
    {
        return;
    }

    this->luaEditorTextEdit = this->findChild<QQuickItem*>("luaEditor");

    if (Q_NULLPTR == this->luaEditorTextEdit)
    {
        qWarning() << "Could not find luaEditorTextEdit!";
    }

    this->lineNumbersEdit = this->findChild<QQuickItem*>("lineNumbersEdit");

    if (Q_NULLPTR == this->lineNumbersEdit)
    {
        qWarning() << "Could not find lineNumbersEdit!";
    }

    // Access the QQuickTextDocument from the TextEdit
    this->quickTextDocument = this->luaEditorTextEdit->property("textDocument").value<QQuickTextDocument*>();

    // Create and set up the LuaHighlighter
    this->highlighter = new LuaHighlighter(this->luaEditorTextEdit, quickTextDocument->textDocument());
    // Make sure to set the document for the highlighter
    this->highlighter->setDocument(quickTextDocument->textDocument());

    this->highlighter->setCursorPosition(0);
}

void LuaEditorQml::highlightWordUnderCursor(const QString& word)
{
    if (Q_NULLPTR != this->highlighter)
    {
        this->highlighter->searchInTextEdit(word, true, true);
    }
}

void LuaEditorQml::clearHighlightWordUnderCursor()
{
    if (Q_NULLPTR != this->highlighter)
    {
        this->highlighter->clearSearch();
    }
}

void LuaEditorQml::resetTextAfterColon()
{
    this->isAfterColon = false;
    this->typedAfterColon.clear();
}

void LuaEditorQml::resetTextAfterDot()
{
    this->isAfterDot = false;
    this->typedAfterDot.clear();
}

void LuaEditorQml::handleKeywordPressed(QChar keyword)
{
    // Skip any virtual char like shift, strg, alt etc.
    if (keyword == '\0')
    {
        return;
    }

    this->currentText = this->quickTextDocument->textDocument()->toPlainText();

    // Cursor jump, clear everything
    if (this->cursorPosition != this->oldCursorPosition + 1 && this->cursorPosition != this->oldCursorPosition)
    {
        this->resetTextAfterColon();
        this->resetTextAfterDot();
        this->currentLineText.clear();
        this->isInMatchedFunctionProcessing = false;
        this->luaEditorModelItem->resetMatchedClass();
    }

    this->processWithinFunction(keyword);

    // Detect if a colon has been deleted
    bool isCharDeleted = false;
    if (keyword == '\010' || keyword == '\177')  // Handle backspace and delete keys
    {
        isCharDeleted = true;
        if (this->lastColonIndex >= 0 && this->lastColonIndex < this->currentText.size())
        {
            if (true == this->isAfterColon)
            {
                // If deleting a char and the opening bracket has been deleted, we are no longer in matched function processing
                if (this->typedAfterColon.contains("("))
                {
                    this->isInMatchedFunctionProcessing = false;
                    Q_EMIT requestCloseMatchedFunctionContextMenu();
                }
                this->typedAfterColon.removeLast();
                this->typedAfterDot.removeLast();

            }
        }
        else if (this->lastDotIndex >= 0 && this->lastDotIndex < this->currentText.size())
        {

            if (true == this->isAfterDot)
            {
                // If deleting a char and the opening bracket has been deleted, we are no longer in matched function processing
                if (this->typedAfterColon.contains("("))
                {
                    this->isInMatchedFunctionProcessing = false;
                    Q_EMIT requestCloseMatchedFunctionContextMenu();
                }
                this->typedAfterDot.removeLast();
                this->typedAfterColon.removeLast();
            }
        }
        else
        {
            // Removed an opening bracket
            if (this->cursorPosition > 0)
            {
                // this->cursorPosition--;
                // this->oldCursorPosition--;
                // this->cursorPositionChanged(this->cursorPosition);
                this->typedAfterDot.removeLast();
                this->typedAfterColon.removeLast();

                QString textAt = this->currentText[this->cursorPosition];
                if (textAt == '(')
                {
                    this->isInMatchedFunctionProcessing = false;
                    Q_EMIT requestCloseMatchedFunctionContextMenu();
                }
            }
        }
    }

    // Check if we're typing after a colon and sequentially
    if (true == this->isAfterColon)
    {
        if (false == this->isInMatchedFunctionProcessing && (this->cursorPosition != this->oldCursorPosition + 1 || keyword == ' ' || keyword == '\n' || keyword == ':' || keyword == '.'))
        {
            this->resetTextAfterColon();
            this->requestCloseMatchedFunctionContextMenu();
            this->luaEditorModelItem->resetMatchedClass();
        }
        else
        {
             // Do not append backspace or delete keys
            if (keyword != '\010' && keyword != '\177')
            {
                this->typedAfterColon.append(keyword);

                // Start autocompletion with at least 3 chars
                if (this->typedAfterColon.size() >= 3 && (keyword != '(' && keyword != ')') && false == this->typedAfterDot.contains(')'))
                {
                    if (keyword == '.')
                    {
                        this->isInMatchedFunctionProcessing = false;
                        Q_EMIT requestCloseMatchedFunctionContextMenu();
                        this->showIntelliSenseContextMenuAtCursor(true, this->typedAfterColon);
                    }
                    else if (keyword == ':')
                    {
                        this->isInMatchedFunctionProcessing = false;
                        Q_EMIT requestCloseMatchedFunctionContextMenu();
                        this->showIntelliSenseContextMenuAtCursor(false, this->typedAfterColon);
                    }
                    else
                    {
                        this->showIntelliSenseContextMenuAtCursor(false, this->typedAfterColon);
                    }
                }
            }
        }
    }
    else if (true == this->isAfterDot)
    {
        if (false == this->isInMatchedFunctionProcessing && (this->cursorPosition != this->oldCursorPosition + 1 || keyword == ' ' || keyword == '\n' || keyword == ':' || keyword == '.'))
        {
            this->resetTextAfterDot();
            this->requestCloseIntellisense();
            this->luaEditorModelItem->resetMatchedClass();
        }
        else
        {
            // Do not append backspace or delete keys
            if (keyword != '\010' && keyword != '\177')
            {
                this->typedAfterDot.append(keyword);

                // Start autocompletion with at least 3 chars
                if (this->typedAfterDot.size() >= 3 && (keyword != '(' && keyword != ')') && false == this->typedAfterDot.contains(')'))
                {
                    this->showIntelliSenseContextMenuAtCursor(true, this->typedAfterDot);
                }
            }
        }
    }

    // and not removing char
    if (keyword == ')' && keyword != '\010' && keyword != '\177')
    {
        this->processBracket(keyword);
    }
    if (this->typedAfterColon.contains("(") && !this->typedAfterColon.endsWith('.') && !this->typedAfterColon.endsWith(':') && keyword != ')')
    {
        this->isInMatchedFunctionProcessing = true;
        this->showMachtedFunctionContextMenuAtCursor(this->typedAfterColon);
    }
    if (this->typedAfterColon.contains(")"))
    {
        this->isInMatchedFunctionProcessing = false;
        Q_EMIT requestCloseMatchedFunctionContextMenu();
    }

    if (true == this->isAfterColon)
    {
        bool insideFunctionParameters = this->isInsideFunctionParameters(keyword);
        if (false == insideFunctionParameters)
        {
            this->isInMatchedFunctionProcessing = false;
            Q_EMIT requestCloseMatchedFunctionContextMenu();
        }
        else
        {
            this->isInMatchedFunctionProcessing = true;
            Q_EMIT requestCloseIntellisense();

            this->showMachtedFunctionContextMenuAtCursor(this->typedAfterColon);
        }
    }

    // Detect new colon and update lastColonIndex
    if (keyword == ':')
    {
        this->isAfterColon = true;
        this->currentLineText.clear();
        this->typedAfterColon.clear();
        this->lastColonIndex = this->cursorPosition;
        this->isInMatchedFunctionProcessing = false;
        Q_EMIT requestCloseMatchedFunctionContextMenu();
        Q_EMIT requestCloseIntellisense();
        this->showIntelliSenseContextMenu(false);
    }
    // Detect new dot and update lastDotIndex
    else if (keyword == '.')
    {
        this->isAfterDot = true;
        this->isAfterColon = false;
        this->isInMatchedFunctionProcessing = false;

        this->currentLineText.clear();
        this->typedAfterDot.clear();
        this->lastDotIndex = this->cursorPosition;

        Q_EMIT requestCloseMatchedFunctionContextMenu();
        Q_EMIT requestCloseIntellisense();
        this->showIntelliSenseContextMenu(true);
    }
    else if (keyword == '(' && true == this->isAfterColon)
    {
        this->isInMatchedFunctionProcessing = true;
        Q_EMIT requestCloseIntellisense();

        this->showMachtedFunctionContextMenuAtCursor(this->typedAfterColon);
    }
    else if (keyword == ')' && true == this->isAfterColon)
    {
        this->isInMatchedFunctionProcessing = false;
    }

    // and not removing char
    if (keyword == '(' && keyword != '\010' && keyword != '\177')
    {
        this->processBracket(keyword);
    }

    this->processVariableBeingTyped(this->currentText, this->cursorPosition, keyword);

    this->processFunctionBeingTyped(this->currentText, this->cursorPosition, keyword);

    if (keyword != '\010' && keyword != '\177')
    {
        // Update oldCursorPosition to the current cursor position
        this->oldCursorPosition = this->cursorPosition;
    }
}

void LuaEditorQml::processWithinFunction(QChar keyword)
{
    // Case: e.g. setDirection(Vector3.UNIT_X, blub) to recognized where the cursor is and intelisense
    if (keyword == ',' || keyword == ' ')
    {
        // this->isAfterColon = false;
        // this->isAfterDot = false;
        // this->typedAfterColon.clear();
        // this->typedAfterDot.clear();

        // Get the substring up to the current cursor position
        QString textUpToCursor = this->currentText.left(this->cursorPosition);

        // Find the last line break in the substring
        int lineStart = textUpToCursor.lastIndexOf('\n');

        int lastOpenBracketInLineIndex = this->currentText.lastIndexOf('(', this->cursorPosition);
        int lastClosedBracketInLineIndex = this->currentText.lastIndexOf(')', this->cursorPosition);
        bool isInSameLine = lastOpenBracketInLineIndex >= lineStart;

        // Detects just an opening or closing bracket, whether its a valid function to show infos
        if (true == isInSameLine && lastClosedBracketInLineIndex < lastOpenBracketInLineIndex)
        {
            this->isInMatchedFunctionProcessing = true;
            // Find the last colon in the line up to the current cursor position
            // Get the substring up to the current cursor position
            QString textUpToCursor = this->currentText.left(this->cursorPosition);

            // Find the last line break in the substring
            int lineStart = textUpToCursor.lastIndexOf('\n');
            int lastColonInLineIndex = this->currentText.lastIndexOf(':', this->cursorPosition);
            bool isInSameLine2 = lastColonInLineIndex >= lineStart;

            // Detects just an opening or closing bracket, whether its a valid function to show infos
            if (true == isInSameLine2)
            {
                // Extract text from the last colon to the bracket position
                int startIndex = lastColonInLineIndex + 1;
                int endIndex = this->cursorPosition;

                QString textFromColonToBracket = this->currentText.mid(startIndex, endIndex - startIndex).trimmed();
                textFromColonToBracket += keyword;
                this->showMachtedFunctionContextMenuAtCursor(textFromColonToBracket);
            }
        }
    }
}

void LuaEditorQml::processBracket(QChar keyword)
{
    if (false == this->isInMatchedFunctionProcessing)
    {
        // Find the last colon in the line up to the current cursor position
        // Get the substring up to the current cursor position
        QString textUpToCursor = this->currentText.left(this->cursorPosition);

        // Find the last line break in the substring
        int lineStart = textUpToCursor.lastIndexOf('\n');
        int lastColonInLineIndex = this->currentText.lastIndexOf(':', this->cursorPosition);
        bool isInSameLine = lastColonInLineIndex >= lineStart;

        // Detects just an opening or closing bracket, whether its a valid function to show infos
        if (true == isInSameLine)
        {
            // Extract text from the last colon to the bracket position
            int startIndex = lastColonInLineIndex + 1;
            int endIndex = this->cursorPosition + 1;

            QString textFromColonToBracket = this->currentText.mid(startIndex, endIndex - startIndex).trimmed();
            textFromColonToBracket += keyword;

            // Optionally, skip processing based on extracted text or take further actions here
            if (false == textFromColonToBracket.isEmpty() && keyword != ')')
            {
                this->isInMatchedFunctionProcessing = true;
                Q_EMIT requestCloseIntellisense();

                this->isAfterColon = true;
                this->isAfterDot = false;
                this->typedAfterColon = textFromColonToBracket;

                this->showMachtedFunctionContextMenuAtCursor(this->typedAfterColon);
            }
        }
    }
    else if (keyword == ')')
    {
        this->isInMatchedFunctionProcessing = false;
        Q_EMIT requestCloseMatchedFunctionContextMenu();
        Q_EMIT requestCloseIntellisense();
    }
}

bool LuaEditorQml::isInsideFunctionParameters(QChar keyword)
{
    bool insideString = false;
    QChar stringDelimiter; // Track if we're inside ' or "
    bool insideComment = false;

    int parenthesisCount = 0;
    int lastOpenParenthesisPos = -1; // Track the last unmatched '(' position

    bool deleteChar = false;
    if (keyword == '\010' || keyword == '\177')  // Handle backspace and delete keys
    {
        deleteChar = true;
    }

    QString textUpToCursor = currentText.left(cursorPosition);
    int lastNewlinePos = textUpToCursor.lastIndexOf('\n');

    QString tempCurrentLineText = textUpToCursor.mid(lastNewlinePos + 1);
    if (!deleteChar)
    {
        tempCurrentLineText += keyword;
    }

    for (int i = 0; i < tempCurrentLineText.size(); ++i)
    {
        QChar ch = tempCurrentLineText.at(i);

        // Case 1: Handle string literals
        if ((ch == '"' || ch == '\'') && !insideComment)
        {
            if (!insideString)
            {
                insideString = true;
                stringDelimiter = ch;
            }
            else if (ch == stringDelimiter)
            {
                insideString = false;
            }
            continue;
        }

        // Case 2: Handle comments
        if (ch == '-' && i + 1 < tempCurrentLineText.size() && tempCurrentLineText.at(i + 1) == '-')
        {
            insideComment = true;
        }
        if (insideComment)
        {
            continue;
        }

        // Case 3: Count parentheses only if not inside string or comment
        if (!insideString && !insideComment)
        {
            if (ch == '(')
            {
                parenthesisCount++;
                lastOpenParenthesisPos = i; // Update last unmatched '(' position
            }
            else if (ch == ')')
            {
                parenthesisCount--;
                if (parenthesisCount == 0)
                {
                    lastOpenParenthesisPos = -1; // Reset if parentheses are balanced
                }
            }
        }
    }

    // Case 4: Check for `:` or `.` after last unmatched `(`
    int lastColonPos = tempCurrentLineText.lastIndexOf(':');
    int lastDotPos = tempCurrentLineText.lastIndexOf('.');

    // Determine the nearest `:` or `.`
    int nearestDelimiterPos = qMax(lastColonPos, lastDotPos);

    if (nearestDelimiterPos > lastOpenParenthesisPos)
    {
        // If `:` or `.` is found after the last unmatched `(`, we're NOT in parameters
        return false;
    }

    // If there are unmatched opening brackets and no invalid delimiters after them, we're inside function parameters
    return parenthesisCount > 0;
}

void LuaEditorQml::processVariableBeingTyped(const QString& currentText, int cursorPosition, QChar keyword)
{
    bool deleteChar = false;
    if (keyword == '\010' || keyword == '\177')  // Handle backspace and delete keys
    {
        deleteChar = true;
    }
    else if (!keyword.isLetter() && keyword != '_' && !keyword.isDigit())
    {
        return; // Ignore invalid characters unless it's backspace/delete
    }

    if (this->isInMatchedFunctionProcessing)
    {
        return;
    }

    QString textUpToCursor = currentText.left(cursorPosition);
    int lastNewlinePos = textUpToCursor.lastIndexOf('\n');

    this->currentLineText.clear();
    this->currentLineText = textUpToCursor.mid(lastNewlinePos + 1);

    if (!deleteChar)
    {
        // Add the latest character typed if it's not a deletion
        this->currentLineText += keyword;
    }

    // Trim spaces and isolate the last word being typed
    this->currentLineText = this->currentLineText.trimmed();
    int lastSpacePos = this->currentLineText.lastIndexOf(' ');
    QString lastWord = (lastSpacePos != -1) ? this->currentLineText.mid(lastSpacePos + 1) : this->currentLineText;
    // **Remove unwanted leading characters**
    lastWord.remove(QRegularExpression(R"(^[~+\-/*(#]+)"));

    this->currentLineText = lastWord;

    bool variableBeingDetected = false;

    // Case 1: Ignore if inside a string
    if (this->currentLineText.contains("\"") || this->currentLineText.contains("\'"))
    {
        return;
    }

    // Case 2: Ignore if it's a comment
    if (this->currentLineText.startsWith("--"))
    {
        return;
    }

    // Case 3: Ignore if after `:` but no `(` follows
    int colonPos = this->currentLineText.lastIndexOf(':');
    if (colonPos != -1 && this->currentLineText.indexOf('(', colonPos) == -1)
    {
        return; // After a colon and no opening bracket
    }

    // Case 4: Ignore if after a `.` character
    if (this->currentLineText.lastIndexOf('.') > lastSpacePos)
    {
        return;
    }

    // Case 5: Ignore if after a `(` character
    if (this->currentLineText.lastIndexOf('(') > lastSpacePos)
    {
        return;
    }

    // Case 6: Ensure the word starts with a valid variable character
    if (!lastWord.isEmpty())
    {
        QChar firstChar = lastWord.at(0);
        if (!firstChar.isLetter() && firstChar != '_')
        {
            return; // Invalid start for a variable
        }

        // Ensure the variable name doesn't start with a digit
        if (firstChar.isDigit())
        {
            return;
        }

        variableBeingDetected = true;
    }

    // Case 7: Ensure the word matches the variable pattern
    QRegularExpression variablePattern(R"(^[a-zA-Z_][a-zA-Z0-9_]*$)");
    if (!variablePattern.match(lastWord).hasMatch())
    {
        return;
    }

    // IntelliSense trigger
    if (variableBeingDetected && lastWord.size() > 2)
    {
        QChar startChar = lastWord.at(0);
        bool forSingleton = startChar.isUpper();
        this->showIntelliSenseMenuForVariablesAtCursor(forSingleton, lastWord);
    }
    else
    {
        if (this->typedAfterColon.isEmpty() && this->typedAfterDot.isEmpty())
        {
            this->requestCloseIntellisense();
        }
    }
}

void LuaEditorQml::processFunctionBeingTyped(const QString& currentText, int cursorPosition, QChar keyword)
{
    bool deleteChar = false;

    this->isAfterColon = false;
    this->isAfterDot = false;

    if (keyword == '\010' || keyword == '\177') // Handle backspace and delete keys
    {
        deleteChar = true;
    }
    else if (!keyword.isLetter() && keyword != '_' && !keyword.isDigit() && keyword != ':' && keyword != '.')
    {
        return; // Ignore invalid characters unless it's backspace/delete
    }

    if (this->isInMatchedFunctionProcessing)
    {
        return;
    }

    QString textUpToCursor = currentText.left(cursorPosition);
    int lastNewlinePos = textUpToCursor.lastIndexOf('\n');

    QString currentLineText = textUpToCursor.mid(lastNewlinePos + 1);

    if (!deleteChar)
    {
        // Add the latest character typed if it's not a deletion
        currentLineText += keyword;
    }

    // Trim spaces
    currentLineText = currentLineText.trimmed();

    // Check for the last occurrence of `:` or `.`
    int colonPos = currentLineText.lastIndexOf(':');
    int dotPos = currentLineText.lastIndexOf('.');

    // Determine the closer symbol and its type
    int symbolPos = -1;
    bool isForConstant = false;

    if (colonPos > dotPos)
    {
        symbolPos = colonPos;
        isForConstant = false; // It's a class function
    }
    else if (dotPos > colonPos)
    {
        symbolPos = dotPos;
        isForConstant = true; // It's a constant
    }

    if (symbolPos == -1/* || symbolPos == currentLineText.size() - 1*/)
    {
        return; // No `:` or `.` or it's at the end
    }
    QString afterSymbol = currentLineText.mid(symbolPos + 1).trimmed();

    // Ensure we have valid characters after the symbol
    QRegularExpression functionNamePattern(R"(^[a-zA-Z_][a-zA-Z0-9_]*$)");
    if (!functionNamePattern.match(afterSymbol).hasMatch())
    {
        return;
    }

    // Check if there is an opening parenthesis after the symbol
    if (currentLineText.indexOf('(', symbolPos) != -1)
    {
        return; // Already has an opening parenthesis, not typing a function
    }

    // Function or constant detected
    this->typedAfterColon = afterSymbol;
    this->isAfterColon = !isForConstant;
    this->isAfterDot = isForConstant;
    this->showIntelliSenseContextMenuAtCursor(isForConstant, this->typedAfterColon);
}

void LuaEditorQml::showIntelliSenseContextMenu(bool forConstant)
{
    if (true == this->isInMatchedFunctionProcessing)
    {
        Q_EMIT requestCloseIntellisense();
        return;
    }

    QTextCursor cursor = this->highlighter->getCursor();
    int cursorPos = cursor.position();  // Get the cursor position

    // Create modified current text that includes the ":", because at this time, the current text would be without the ":" and that would corrupt the class match.
    QString modifiedText = this->currentText;
    if (false == forConstant)
    {
        modifiedText.insert(cursorPos, ":"); // Insert ":" at the cursor position
    }
    else
    {
        modifiedText.insert(cursorPos, "."); // Insert "." at the cursor position
    }

    const auto& cursorGlobalPos = this->cursorAtPosition(modifiedText, cursorPos);
    Q_EMIT requestIntellisenseProcessing(forConstant, this->currentText, "", cursorPos, cursorGlobalPos.x(), cursorGlobalPos.y(), false);
}

void LuaEditorQml::showIntelliSenseContextMenuAtCursor(bool forConstant, const QString& textAfterColon)
{
    if (true == this->isInMatchedFunctionProcessing)
    {
        Q_EMIT requestCloseIntellisense();
        return;
    }

    const auto& cursorGlobalPos = this->cursorAtPosition(this->currentText, this->cursorPosition);
    Q_EMIT requestIntellisenseProcessing(forConstant, this->currentText, textAfterColon, this->cursorPosition, cursorGlobalPos.x(), cursorGlobalPos.y(), false);
}

void LuaEditorQml::showIntelliSenseMenuForVariablesAtCursor(bool forSingleton, const QString& text)
{
    const auto& cursorGlobalPos = this->cursorAtPosition(this->currentText, this->cursorPosition);
    Q_EMIT requestMatchedForVariablesProcessing(forSingleton, text, this->cursorPosition, cursorGlobalPos.x(), cursorGlobalPos.y());
}

void LuaEditorQml::showMachtedFunctionContextMenuAtCursor(const QString& textAfterColon)
{
    if (false == this->isInMatchedFunctionProcessing)
    {
        Q_EMIT requestCloseMatchedFunctionContextMenu();
        return;
    }

    Q_EMIT requestCloseIntellisense();

    const auto& cursorGlobalPos = this->cursorAtPosition(this->currentText, this->cursorPosition);

    QString textUpToCursor = currentText.left(this->cursorPosition); // Text up to the cursor
    int lastNewlinePos = textUpToCursor.lastIndexOf('\n');

    QString tempCurrentLineText = textUpToCursor.mid(lastNewlinePos + 1);

    qDebug() << "---> tempCurrentLineText: " << tempCurrentLineText << " textAfterColon: " << textAfterColon;
    Q_EMIT requestMatchedFunctionContextMenu(this->currentText, textAfterColon, this->cursorPosition, cursorGlobalPos.x(), cursorGlobalPos.y());
}

void LuaEditorQml::updateContentY(qreal contentY)
{
    this->scrollY = contentY;
}

QPointF LuaEditorQml::cursorAtPosition(const QString& currentText, int cursorPos)
{
    // Get the updated text and cursor position
    QTextCursor textCursor = this->highlighter->getCursor();
    textCursor.setPosition(cursorPos);

    // Use QTextCursor to get line and character position
    int line = textCursor.blockNumber();
    int charPosInLine = textCursor.positionInBlock();

    QFont font = this->quickTextDocument->textDocument()->defaultFont();
    QFontMetrics fontMetrics(font);

    // Calculate x position
    qreal x = fontMetrics.horizontalAdvance(textCursor.block().text().mid(0, charPosInLine)) + fontMetrics.ascent();

    // Calculate y position
    qreal y = line * fontMetrics.height() + fontMetrics.ascent();

    QPointF textEditPos = this->luaEditorTextEdit->mapToGlobal(QPointF(0, 0));
    QQuickWindow* window = this->luaEditorTextEdit->window();
    QPoint windowPos = window->mapToGlobal(QPoint(0, 0));

    qreal adjustedY = y - textEditPos.y() - this->scrollY + fontMetrics.height() + 23;
    QPointF cursorGlobalPos = textEditPos + QPointF(x - windowPos.x(), adjustedY - windowPos.y());

    return cursorGlobalPos;
}

void LuaEditorQml::highlightError(int line, int start, int end)
{
    if (Q_NULLPTR != this->highlighter)
    {
        this->highlighter->setErrorLine(line, start, end);
    }
}

void LuaEditorQml::clearError()
{
    if (Q_NULLPTR != this->highlighter)
    {
        this->highlighter->clearErrors();
    }
}

void LuaEditorQml::highlightRuntimeError(int line, int start, int end)
{
    if (Q_NULLPTR != this->highlighter)
    {
        this->highlighter->setRuntimeErrorLine(line, start, end);
    }
}

void LuaEditorQml::clearRuntimeError()
{
    if (Q_NULLPTR != this->highlighter)
    {
        this->highlighter->clearRuntimeErrors();
    }
}

void LuaEditorQml::cursorPositionChanged(int cursorPosition)
{
    if (Q_NULLPTR != this->highlighter)
    {
        this->highlighter->setCursorPosition(cursorPosition);
        this->cursorPosition = cursorPosition;
    }
}
