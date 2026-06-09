import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import NOWALuaScript

LuaEditorQml
{
    id: root;

    // Is done in C++
    // width: parent.width - 12;
    // height: 800;

    focus: true;

    property string textColor: "black";

    property int leftPadding: 4;
    property int topPadding: 4;

    // Property to track if a new line is being inserted
    property bool isInsertingNewLine: false;

    // FIX Bug 9: Track the last line count we rendered so we can skip rebuilds when
    // the count has not actually changed (e.g. the user typed a character mid-line).
    property int lastRenderedLineCount: 0;

    Rectangle
    {
        anchors.fill: parent;
        color: "white";
        border.color: "darkgrey";
        border.width: 2;
    }

    function checkSyntax()
    {
        LuaScriptQmlAdapter.checkSyntax(root.model.filePathName, luaEditor.originalText);
    }

    // FIX Bug 9: Central line-number rebuild, called only by lineNumberTimer.
    // Uses Array.join() which is O(n) total instead of the old "+=" loop that was O(n²).
    // Only fires when the logical line count actually changed.
    function rebuildLineNumbers()
    {
        // Count logical lines (paragraphs) by splitting on newline.
        // We keep this one split because it is now gated behind an 80 ms timer
        // instead of running on every single keystroke.
        var count = luaEditor.text.split("\n").length;

        if (count === root.lastRenderedLineCount)
        {
            return; // Nothing changed, skip the DOM update entirely.
        }
        root.lastRenderedLineCount = count;

        // Build the number string with a join — single allocation instead of n re-allocations.
        var nums = [];
        for (var i = 1; i <= count; i++)
        {
            nums.push(i);
        }
        lineNumbersEdit.text = nums.join("\n");
    }

    Connections
    {
        target: root;

        function onSignal_insertingNewLine(inserting)
        {
            root.isInsertingNewLine = inserting;
        }

        function onSignal_resultSearchContinuePosition(cursorRectangle)
        {
            flickable.ensureVisible(cursorRectangle);
        }
    }

    ColumnLayout
    {
        width: parent.width;
        height: parent.height;

        Flickable
        {
            id: flickable;

            Layout.fillWidth: true;
            Layout.fillHeight: true;

            contentWidth: luaEditor.contentWidth;
            contentHeight: luaEditor.contentHeight + 12; // Ensure extra space for last line
            clip: true;

            boundsBehavior: Flickable.StopAtBounds;

            // FIX Bug 7 (cursor disappear): A pressDelay > 0 lets child Items (the TextEdit)
            // claim the press event first before the Flickable decides whether to start a pan.
            // Without this, a mouse click that lands slightly off the Flickable's threshold
            // is consumed by the Flickable, the TextEdit never gets activeFocus, and the
            // blinking cursor disappears.
            pressDelay: 50;

            ScrollBar.vertical: ScrollBar
            {
                width: 20;
                x: flickable.width - 12;
                policy: ScrollBar.AlwaysOn;
            }

            // Synchronize scroll position
            function ensureVisible(r)
            {
                if (root.isInsertingNewLine)
                {
                    // Do nothing if a new line is being inserted, because it corrupts the
                    // "r" with wrong values and so there is always an ugly jump
                    return;
                }

                if (contentX >= r.x)
                {
                    contentX = r.x;
                }
                else if (contentX + width <= r.x + r.width)
                {
                    contentX = r.x + r.width - width;
                }
                if (contentY >= r.y)
                {
                    contentY = r.y;
                }
                else if (contentY + height <= r.y + r.height)
                {
                    contentY = r.y + r.height - height;
                }
            }

            onContentYChanged:
            {
                root.updateContentY(contentY);
            }

            WheelHandler
            {
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad;

                // CanTakeOverFromAnything gives this handler priority over the Flickable's
                // own built-in wheel handler, so you don't get double-scrolling.
                grabPermissions: PointerHandler.CanTakeOverFromAnything;

                onWheel: function(event)
                {
                    // angleDelta.y is +120 per notch up, -120 per notch down (standard mouse).
                    // Dividing by 120 gives notch count. Multiply by lineHeight * scrollLines
                    // to get pixels. 'fontMetrics' is already defined at the bottom of this file.
                    var scrollLines = 50;  // lines per notch — raise to scroll faster
                    var pixels = (event.angleDelta.y / 60.0) * fontMetrics.height * scrollLines;
                    var newY = flickable.contentY - pixels;

                    flickable.contentY = Math.max(0,
                        Math.min(newY, Math.max(0, flickable.contentHeight - flickable.height)));

                    event.accepted = true;
                }
            }

            Row
            {
                width: flickable.width;

                // Line Number View
                TextEdit
                {
                    id: lineNumbersEdit;
                    objectName: "lineNumbersEdit";

                    readOnly: true;
                    width: 50;
                    padding: 8;
                    wrapMode: TextEdit.NoWrap;
                    color: "#888888";  // Slightly dimmer than editor text
                    font.family: luaEditor.font.family;
                    font.pixelSize: luaEditor.font.pixelSize;

                    // FIX Bug 7: Explicit PlainText so Qt never attempts HTML parsing on numbers.
                    textFormat: TextEdit.PlainText;

                    // FIX Bug 9: Removed the old:
                    //   Component.onCompleted: updateLineNumbers();
                    //   onTextChanged: updateLineNumbers();
                    //
                    // The onTextChanged here caused an infinite loop (setting lineNumbersEdit.text
                    // triggered onTextChanged which called updateLineNumbers which set the text
                    // again). Qt silently merged this into one pass but it was still wrong.
                    // Line number updates now come exclusively from lineNumberTimer below.
                    Component.onCompleted: lineNumberTimer.restart();
                }

                // Lua Code Editor
                TextEdit
                {
                    id: luaEditor;
                    objectName: "luaEditor";

                    Component.onCompleted: lineNumberTimer.restart();

                    width: parent.width - 12;
                    height: parent.height;
                    padding: 8;

                    font.pixelSize: 14;

                    // FIX Bug 7: Was TextEdit.AutoText. AutoText causes Qt to scan the entire
                    // document for HTML markers every time text changes. When the QSyntaxHighlighter
                    // writes rich-text character formats (bold, colours) to the QTextDocument,
                    // AutoText mode can flip the TextEdit into HTML rendering mid-session.
                    // This corrupts the undo stack ordering and makes the blinking cursor
                    // disappear because the internal layout is rebuilt without restoring the
                    // cursor state. PlainText is correct — the QSyntaxHighlighter attaches its
                    // formatting at the QTextDocument level independently of this property.
                    textFormat: TextEdit.PlainText;

                    wrapMode: TextEdit.Wrap;
                    focus: true;
                    activeFocusOnPress: true; // Enable focus on press for text selection
                    color: textColor;
                    selectByMouse: true;
                    persistentSelection: true;
                    selectedTextColor: "black";
                    selectionColor: "lightgreen";

                    property bool controlHeld: false;

                    Keys.onPressed: (event) =>
                    {
                        // If context menu is shown, relay those key to the context menu for navigation
                        if (NOWAApiModel.isIntellisenseShown)
                        {
                            if (event.key === Qt.Key_Tab || event.key === Qt.Key_Up ||
                                event.key === Qt.Key_Down || event.key === Qt.Key_Return)
                            {
                                LuaScriptQmlAdapter.relayKeyPress(event.key);
                                event.accepted = true;
                                return;
                            }
                        }
                        if (event.key === Qt.Key_Tab)
                        {
                            NOWALuaEditorModel.addTabToSelection();
                            event.accepted = true;
                        }
                        else if (event.key === Qt.Key_Return)
                        {
                            event.accepted = true;
                            NOWALuaEditorModel.breakLine();
                            NOWAApiModel.signal_closeIntellisense();
                            NOWAApiModel.signal_closeMatchedFunctionMenu();
                        }
                        else if (event.key === Qt.Key_Control)
                        {
                            luaEditor.controlHeld = true;
                        }
                        else if (event.key === Qt.Key_Z && (event.modifiers & Qt.ControlModifier))
                        {
                            event.accepted = true; // Prevent text edit undo
                            NOWALuaEditorModel.undo();
                        }
                        else if (event.key === Qt.Key_Y && (event.modifiers & Qt.ControlModifier))
                        {
                            event.accepted = true; // Prevent text edit redo
                            NOWALuaEditorModel.redo();
                        }
                        else
                        {
                            root.handleKeywordPressed(event.text);
                        }
                    }

                    Keys.onReleased: (event) =>
                    {
                        if (event.key === Qt.Key_Control)
                        {
                            luaEditor.controlHeld = false;
                        }
                    }

                    onTextChanged:
                    {
                        checkSyntaxTimer.restart();

                        // FIX Bug 9: Replaced the direct call to updateLineNumbers() with a
                        // debounced timer restart. The timer fires at most once per 80 ms even
                        // if the user types continuously. Inside the timer, rebuildLineNumbers()
                        // additionally skips the DOM update when the line count has not changed
                        // (e.g. typing within a line). This eliminates the per-keystroke O(n²)
                        // string rebuild that was slowing down 400-line files.
                        lineNumberTimer.restart();

                        if (root.model)
                        {
                            root.model.content = luaEditor.text;

                            // Update currentText only after cursorPosition is updated
                            Qt.callLater(() => {
                                root.currentText = luaEditor.text;
                            });
                        }
                    }

                    onCursorPositionChanged:
                    {
                        root.cursorPositionChanged(cursorPosition);
                    }

                    onCursorRectangleChanged:
                    {
                        flickable.ensureVisible(cursorRectangle);
                    }

                    // Track changes in selected text
                    onSelectedTextChanged:
                    {
                        if (luaEditor.controlHeld)
                        {
                            if (luaEditor.selectedText.length > 0)
                            {
                                // A word was selected, trigger the search in LuaHighlighter
                                root.highlightWordUnderCursor(luaEditor.selectedText);
                            }
                            else
                            {
                                root.clearHighlightWordUnderCursor();
                            }
                        }
                        if (luaEditor.selectedText.length > 0)
                        {
                            // This signal now goes to SearchWindow which decides whether to apply
                            // it immediately (if open) or park it as pendingSearchText (if closed).
                            // See SearchWindow.qml Connections.onSignal_setSelectedSearchText.
                            NOWALuaEditorModel.setSelectedSearchText(luaEditor.selectedText);
                        }
                    }

                    Connections
                    {
                        target: LuaScriptQmlAdapter;

                        function onSignal_syntaxCheckResult(filePathName, valid, line, start, end, message)
                        {
                            if (filePathName === root.model.filePathName)
                            {
                                if (!valid)
                                {
                                    root.highlightError(line, start, end);
                                }
                                else
                                {
                                    root.clearError();
                                }
                            }
                        }

                        function onSignal_runtimeErrorResult(filePathName, valid, line, start, end, message)
                        {
                            if (filePathName === root.model.filePathName)
                            {
                                if (!valid)
                                {
                                    root.highlightRuntimeError(line, start, end);
                                }
                                else
                                {
                                    root.clearRuntimeError();
                                }
                            }
                        }
                    }

                    Connections
                    {
                        target: NOWALuaEditorModel;

                        function onSignal_luaScriptAdded(filePathName, content)
                        {
                            if (filePathName === root.model.filePathName)
                            {
                                luaEditor.text = content;
                                detailsArea.filePathName = root.model.filePathName;
                            }
                        }
                    }
                }
            }
        }


        Rectangle
        {
            Layout.fillWidth: true;
            Layout.preferredHeight: 10;
            color: "darkslategrey";
        }

        DetailsArea
        {
            id: detailsArea;
            Layout.fillWidth: true;
            Layout.preferredHeight: 110;
        }
    }

    Timer
    {
        id: checkSyntaxTimer;
        interval: 1000;
        repeat: false;
        onTriggered:
        {
            LuaScriptQmlAdapter.checkSyntax(root.model.filePathName, luaEditor.text);
        }
    }

    // FIX Bug 9: Debounced line-number timer.
    // - interval 80 ms: fast enough to feel instantaneous, slow enough to skip
    //   intermediate keystrokes when typing quickly.
    // - rebuildLineNumbers() additionally guards against rebuilding when the line
    //   count itself has not changed, so holding down a key mid-line does zero work.
    Timer
    {
        id: lineNumberTimer;
        interval: 80;
        repeat: false;
        onTriggered: root.rebuildLineNumbers();
    }

    FontMetrics
    {
        id: fontMetrics;
        font: luaEditor.font;
    }
}
