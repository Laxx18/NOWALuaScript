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
    isInMatchedFunctionProcessing(false),
    charDeleted(false)
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

        QString currentLineText = this->getCurrentLineUpToCursor();

        this->luaEditorTextEdit->forceActiveFocus();
        if (true == this->isAfterColon)
        {
            int sizeToReplace = this->getCharsToReplace(currentLineText, text);
            // qDebug() << "--> forClass sizeToReplace: " << sizeToReplace;
            this->highlighter->insertSentText(sizeToReplace, text);

            // Actualize the typed after colon text, to still show the intelisense for the method
            this->typedAfterColon = text;
        }
        else if (true == this->isAfterDot)
        {
            int sizeToReplace = this->getCharsToReplace(currentLineText, text);
            // qDebug() << "--> forConstant sizeToReplace: " << sizeToReplace;
            this->highlighter->insertSentText(sizeToReplace, text);

            // Actualize the typed after colon text, to still show the intelisense for the method
            this->typedAfterDot = text;
            Q_EMIT requestCloseIntellisense();
        }

        this->oldCursorPosition = this->cursorPosition - 1;
    });

    connect(this->luaEditorModelItem, &LuaEditorModelItem::signal_sendVariableTextToEditor, this, [this](const QString& text) {

        QString currentLineText = this->getCurrentLineUpToCursor();

        this->luaEditorTextEdit->forceActiveFocus();
        int sizeToReplace = this->getCharsToReplace(currentLineText, text);
        qDebug() << "--> signal_sendVariableTextToEditor sizeToReplace: " << sizeToReplace;
        this->highlighter->insertSentText(sizeToReplace, text);
        this->currentLineTextVariable = text;
        Q_EMIT requestCloseIntellisense();

        this->oldCursorPosition = this->cursorPosition - 1;
    });

    connect(this, &LuaEditorQml::requestIntellisenseProcessing, this->luaEditorModelItem, &LuaEditorModelItem::startIntellisenseProcessing);
    connect(this, &LuaEditorQml::requestCloseIntellisense, this->luaEditorModelItem, &LuaEditorModelItem::closeIntellisense);

    connect(this, &LuaEditorQml::requestCloseMatchedFunctionContextMenu, this->luaEditorModelItem, &LuaEditorModelItem::closeMatchedFunction);


    connect(this->highlighter, &LuaHighlighter::insertingNewLineChanged, this, [this](bool inserting) {
        Q_EMIT signal_insertingNewLine(inserting);
    });

    Q_EMIT modelChanged();
}

int LuaEditorQml::getCharsToReplace(const QString& originalText, const QString& suggestedText)
{
    // Remove leading spaces from the original text to focus only on the meaningful part
    QString trimmedOriginal = originalText.trimmed();

    // Initialize the size of the match
    int matchLength = 0;

    // Compare the suffix of originalText with the prefix of suggestedText
    for (int i = 0; i < qMin(trimmedOriginal.length(), suggestedText.length()); ++i)
    {
        if (trimmedOriginal.mid(trimmedOriginal.length() - i - 1) == suggestedText.left(i + 1))
        {
            // If the suffix of originalText matches the prefix of suggestedText, increase the match length
            matchLength = i + 1;
        }
    }

    // Return the number of characters to replace
    return matchLength;
}

void LuaEditorQml::setCurrentText(const QString& currentText)
{
    if (this->currentText == currentText)
    {
        return;
    }

    this->currentText = currentText;

    this->processVariableBeingTyped();

    if (false == this->processFunctionBeingTyped())
    {
        this->processFunctionParametersBeingTyped();
    }

    Q_EMIT currentTextChanged();
}

QString LuaEditorQml::getCurrentText(void) const
{
    return this->currentText;
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

    // Skip any virtual char like shift, strg, alt etc.
    this->currentText = this->quickTextDocument->textDocument()->toPlainText();

    // Cursor jump, clear everything
    if (this->cursorPosition != this->oldCursorPosition + 1 && this->cursorPosition != this->oldCursorPosition)
    {
        this->resetTextAfterColon();
        this->resetTextAfterDot();
        this->currentLineTextVariable.clear();
        this->isInMatchedFunctionProcessing = false;
        this->luaEditorModelItem->resetMatchedClass();
    }

    // Detect if a colon has been deleted
    this->charDeleted = false;
    if (keyword == '\010' || keyword == '\177')  // Handle backspace and delete keys
    {
        this->charDeleted = true;
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

    if (')' == keyword)
    {
        int tempCursorPosition = this->cursorPosition;
        QString tempCurrentText = this->currentText;

        QString textUpToCursor = this->currentText.left(this->cursorPosition);
        int lastNewlinePos = textUpToCursor.lastIndexOf('\n');

        int lineCursorPos = this->cursorPosition - (lastNewlinePos + 1);

        QString currentLineText = this->getCurrentLineUpToCursor();
        currentLineText += ")";



        int bracketBalance = 0;
        int unmatchedOpenBracketPos = -1;

        // Check if there is a last open bracket
        for (int i = lineCursorPos; i >= 0; --i)
        {
            QChar ch = currentLineText[i];
            if (ch == ')')
            {
                ++bracketBalance;
            }
            else if (ch == '(')
            {
                --bracketBalance;
                if (bracketBalance < 0)
                {
                    unmatchedOpenBracketPos = i + 1; // Start after the unmatched '('
                    break;
                }
            }
        }
        // If there's an unmatched '(' before the cursor, use it to split the tokens

        if (unmatchedOpenBracketPos == -1)
        {
            this->luaEditorModelItem->closeIntellisense();
            this->luaEditorModelItem->closeMatchedFunction();
        }
    }

    // Check if we're typing after a colon and sequentially
    if (true == this->isAfterColon)
    {
        if (false == this->isInMatchedFunctionProcessing && (this->cursorPosition != this->oldCursorPosition + 1 || keyword == ' ' || keyword == '\n' || keyword == ':' || keyword == '.'))
        {
            this->resetTextAfterColon();
            Q_EMIT requestCloseMatchedFunctionContextMenu();
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
                        this->showIntelliSenseContextMenuAtCursor(true, false, this->currentText, this->cursorPosition, this->typedAfterColon);
                    }
                    else if (keyword == ':')
                    {
                        this->isInMatchedFunctionProcessing = false;
                        Q_EMIT requestCloseMatchedFunctionContextMenu();
                        this->showIntelliSenseContextMenuAtCursor(false, false, this->currentText, this->cursorPosition, this->typedAfterColon);
                    }
                    else
                    {
                        this->showIntelliSenseContextMenuAtCursor(false, false, this->currentText, this->cursorPosition, this->typedAfterColon);
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
                    this->showIntelliSenseContextMenuAtCursor(true, false, this->currentText, this->cursorPosition, this->typedAfterDot);
                }
            }
        }
    }

    // Detect new colon and update lastColonIndex
    if (keyword == ':')
    {
        this->isAfterColon = true;
        this->currentLineTextVariable.clear();
        this->typedAfterColon.clear();
        this->lastColonIndex = this->cursorPosition;
        this->isInMatchedFunctionProcessing = false;
    }
    // Detect new dot and update lastDotIndex
    else if (keyword == '.')
    {
        this->isAfterDot = true;
        this->isAfterColon = false;
        this->isInMatchedFunctionProcessing = false;

        this->currentLineTextVariable.clear();
        this->typedAfterDot.clear();
        this->lastDotIndex = this->cursorPosition;
    }

    if (keyword != '\010' && keyword != '\177')
    {
        // Update oldCursorPosition to the current cursor position
        this->oldCursorPosition = this->cursorPosition;
    }
}

QString LuaEditorQml::getCurrentLineUpToCursor()
{
    // Get the text up to (but not including) the cursor position
    QString textUpToCursor = this->currentText.left(this->cursorPosition);

    // Find the last newline before the cursor
    int lastNewlinePos = textUpToCursor.lastIndexOf('\n');

    // Extract the current line up to one character before the cursor
    QString currentLineUpToCursor;
    if (false == this->charDeleted)
    {
        currentLineUpToCursor = textUpToCursor.mid(lastNewlinePos + 1);
    }
    else
    {
        currentLineUpToCursor = textUpToCursor.mid(lastNewlinePos + 1, this->cursorPosition - lastNewlinePos - 2);
    }
    // qDebug() << "##########currentLineUpToCursor: " << currentLineUpToCursor;
    return currentLineUpToCursor;
}


bool LuaEditorQml::processVariableBeingTyped(void)
{
    QString textUpToCursor = this->currentText.left(this->cursorPosition);
    int lastNewlinePos = textUpToCursor.lastIndexOf('\n');

    this->currentLineTextVariable.clear();
    this->currentLineTextVariable = this->getCurrentLineUpToCursor();

    int lastSpacePos = this->currentLineTextVariable.lastIndexOf(' ');
    QString lastWord = (lastSpacePos != -1) ? this->currentLineTextVariable.mid(lastSpacePos + 1) : this->currentLineTextVariable;
    // **Remove unwanted leading characters**
    lastWord.remove(QRegularExpression(R"(^[~+\-/*(#]+)"));

    this->currentLineTextVariable = lastWord;

    bool variableBeingDetected = false;

    // Case 1: Ignore if inside a string
    if (this->currentLineTextVariable.contains("\"") || this->currentLineTextVariable.contains("\'"))
    {
        return false;
    }

    // Case 2: Ignore if it's a comment
    if (this->currentLineTextVariable.startsWith("--"))
    {
        return false;
    }

    // Case 3: Ignore if after `:` but no `(` follows
    int colonPos = this->currentLineTextVariable.lastIndexOf(':');
    if (colonPos != -1 && this->currentLineTextVariable.indexOf('(', colonPos) == -1)
    {
        return false; // After a colon and no opening bracket
    }

    // Case 4: Ignore if after a `.` character
    if (this->currentLineTextVariable.lastIndexOf('.') > lastSpacePos)
    {
        return false;
    }

    // Case 5: Ignore if after a `(` character
    if (this->currentLineTextVariable.lastIndexOf('(') > lastSpacePos)
    {
        return false;
    }

    // Case 6: Ensure the word starts with a valid variable character
    if (!lastWord.isEmpty())
    {
        QChar firstChar = lastWord.at(0);
        if (!firstChar.isLetter() && firstChar != '_')
        {
            return false; // Invalid start for a variable
        }

        // Ensure the variable name doesn't start with a digit
        if (firstChar.isDigit())
        {
            return false;
        }

        variableBeingDetected = true;
    }

    // Case 7: Ensure the word matches the variable pattern
    QRegularExpression variablePattern(R"(^[a-zA-Z_][a-zA-Z0-9_]*$)");
    if (!variablePattern.match(lastWord).hasMatch())
    {
        return false;
    }

    // IntelliSense trigger
    if (variableBeingDetected && lastWord.size() > 2)
    {
        QChar startChar = lastWord.at(0);
        bool forSingleton = startChar.isUpper();
        this->showIntelliSenseContextMenuAtCursor(false, false, this->currentText, this->cursorPosition, lastWord);
        return true;
    }
    else
    {
        if (this->typedAfterColon.isEmpty() && this->typedAfterDot.isEmpty())
        {
            this->requestCloseIntellisense();
        }
        return false;
    }
}

bool LuaEditorQml::processFunctionBeingTyped(void)
{
    this->isAfterColon = false;
    this->isAfterDot = false;

    // if (this->isInMatchedFunctionProcessing)
    // {
    //     return;
    // }

    int tempCursorPosition = this->cursorPosition;
    QString tempCurrentText = this->currentText;

    QString textUpToCursor = this->currentText.left(this->cursorPosition);
    int lastNewlinePos = textUpToCursor.lastIndexOf('\n');
    QString currentLineText = textUpToCursor.mid(lastNewlinePos + 1);

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
        return false; // No `:` or `.` or it's at the end
    }

    QString afterSymbol = currentLineText.mid(symbolPos);

    // Ensure we have valid characters after the symbol
    QRegularExpression functionNamePattern(R"(^[:.a-zA-Z_][a-zA-Z0-9_:.\s]*$)");
    if (!functionNamePattern.match(afterSymbol).hasMatch())
    {
        return false;
    }

    // Check if there is an opening parenthesis after the symbol
    if (currentLineText.indexOf('(', symbolPos) != -1)
    {
        return false; // Already has an opening parenthesis, not typing a function
    }

    // Function or constant detected
    this->typedAfterColon = afterSymbol;
    this->isAfterColon = !isForConstant;
    this->isAfterDot = isForConstant;
    this->showIntelliSenseContextMenuAtCursor(isForConstant, false, tempCurrentText, tempCursorPosition, this->typedAfterColon);
    return true;
}

bool LuaEditorQml::processFunctionParametersBeingTyped(void)
{
    this->isInMatchedFunctionProcessing = false;

    int tempCursorPosition = this->cursorPosition;
    QString tempCurrentText = this->currentText;

    QString textUpToCursor = this->currentText.left(this->cursorPosition);
    int lastNewlinePos = textUpToCursor.lastIndexOf('\n');

    int lineCursorPos = this->cursorPosition - (lastNewlinePos + 1);

    QString currentLineText = this->getCurrentLineUpToCursor();

    // Find the last colon `:` and opening bracket `(`
    int lastColonPos = currentLineText.lastIndexOf(':');
    int lastOpenBracketPos = currentLineText.lastIndexOf('(');

    if (lastColonPos == -1 || lastOpenBracketPos == -1 || lastColonPos > lastOpenBracketPos)
    {
        return false; // No colon, no opening bracket, or colon appears after the last open bracket
    }

    // Detect the position of the first unbalanced opening parenthesis before the cursor
    int bracketBalance = 0;
    int unmatchedOpenBracketPos = -1;

    if (lineCursorPos >= currentLineText.size())
    {
        lineCursorPos = currentLineText.size();
    }

    // Check if there is a last open bracket
    for (int i = lineCursorPos - 1; i >= 0; --i)
    {
        QChar ch = currentLineText[i];
        if (ch == ')')
        {
            ++bracketBalance;
        }
        else if (ch == '(')
        {
            --bracketBalance;
            if (bracketBalance < 0)
            {
                unmatchedOpenBracketPos = i + 1; // Start after the unmatched '('
                break;
            }
        }
    }
    // If there's an unmatched '(' before the cursor, use it to split the tokens

    if (unmatchedOpenBracketPos == -1)
    {
        return false; // Already has a closing bracket
    }

    // Extract the text between the last colon and the opening bracket
    QString textFromColonToBracket = currentLineText.mid(lastColonPos + 1);

    // Check if the text from colon to bracket is valid
    if (false == textFromColonToBracket.isEmpty())
    {
        this->isInMatchedFunctionProcessing = true;
        this->showIntelliSenseContextMenuAtCursor(false, true, tempCurrentText, tempCursorPosition, textFromColonToBracket);
        return true;
    }
    else
    {
        Q_EMIT requestCloseMatchedFunctionContextMenu();
        return false;
    }
}

void LuaEditorQml::showIntelliSenseContextMenuAtCursor(bool forConstant, bool forFunctionParameters, const QString& text, int cursorPosition, const QString& textAfterColon)
{
    const auto& cursorGlobalPos = this->cursorAtPosition(text, cursorPosition);
    Q_EMIT requestIntellisenseProcessing(forConstant, forFunctionParameters, text, textAfterColon, cursorPosition, cursorGlobalPos.x(), cursorGlobalPos.y(), false);
}

void LuaEditorQml::updateContentY(qreal contentY)
{
    this->scrollY = contentY;
}

QPointF LuaEditorQml::cursorAtPosition(const QString& currentText, int cursorPos)
{
    QString textUpToCursor = currentText.left(cursorPos); // Text up to the cursor
    int lastNewlinePos = textUpToCursor.lastIndexOf('\n');
    int lineCursorPos = this->cursorPosition - (lastNewlinePos + 1);

    QString wholeCurrentLine = textUpToCursor.mid(lastNewlinePos + 1);
    QString currentLineUpToCursor = wholeCurrentLine.mid(lineCursorPos);
    QString currentLineText = this->getCurrentLineUpToCursor();

    // Adjust the cursor position to be relative to the current line
    int charPosInLine = cursorPos - (lastNewlinePos + 1);

    // Calculate the current line index
    int lineIndex = textUpToCursor.count('\n');

    QFont font = this->quickTextDocument->textDocument()->defaultFont();
    QFontMetrics fontMetrics(font);

    // Calculate x position
    qreal x = fontMetrics.horizontalAdvance(currentLineText.mid(0, charPosInLine)) + fontMetrics.ascent();

    // Calculate y position
    qreal y = lineIndex * fontMetrics.height() + fontMetrics.ascent();

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
        this->charDeleted = false;
        this->highlighter->setCursorPosition(cursorPosition);
        this->cursorPosition = cursorPosition;
    }
}
