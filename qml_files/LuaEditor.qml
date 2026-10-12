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

    property bool isInsertingNewLine: false;

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
        LuaScriptQmlAdapter.checkSyntax(root.model.filePathName, luaEditor.text);
    }

    // Gives the keyboard focus back to the text, e.g. when the search bar is closed
    function focusEditor()
    {
        luaEditor.forceActiveFocus();
    }

    // Jumps to the start of the given line (1 based) and gives the focus to the editor. Used by Ctrl+G.
    function goToLine(lineNumber)
    {
        var text = luaEditor.text;
        var position = 0;
        for (var i = 1; i < lineNumber; ++i)
        {
            var newLinePosition = text.indexOf("\n", position);
            if (newLinePosition < 0)
            {
                break;
            }
            position = newLinePosition + 1;
        }

        luaEditor.deselect();
        luaEditor.cursorPosition = position;
        luaEditor.forceActiveFocus();
    }

    function getLineCount()
    {
        // Attention: luaEditor.lineCount counts the wrapped lines too
        return luaEditor.text.split("\n").length;
    }

    // Note: A named function, so that Qt.callLater calls it only once per event loop, also if several keys have been pressed
    function syncCurrentText()
    {
        root.currentText = luaEditor.text;
    }

    function rebuildLineNumbers()
    {
        var count = luaEditor.text.split("\n").length;

        if (count === root.lastRenderedLineCount)
        {
            return;
        }
        root.lastRenderedLineCount = count;

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
            contentHeight: luaEditor.contentHeight + 12;
            clip: true;

            boundsBehavior: Flickable.StopAtBounds;
            pressDelay: 50;

            ScrollBar.vertical: ScrollBar
            {
                width: 20;
                x: flickable.width - 12;
                policy: ScrollBar.AlwaysOn;
            }

            function ensureVisible(r)
            {
                if (root.isInsertingNewLine)
                {
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

            Row
            {
                width: flickable.width;

                TextEdit
                {
                    id: lineNumbersEdit;
                    objectName: "lineNumbersEdit";

                    readOnly: true;
                    width: 50;
                    padding: 8;
                    wrapMode: TextEdit.NoWrap;
                    color: "#888888";
                    font.family: luaEditor.font.family;
                    font.pixelSize: luaEditor.font.pixelSize;
                    textFormat: TextEdit.PlainText;

                    Component.onCompleted: lineNumberTimer.restart();
                }

                TextEdit
                {
                    id: luaEditor;
                    objectName: "luaEditor";

                    Component.onCompleted: lineNumberTimer.restart();

                    width: parent.width - 12;
                    height: parent.height;
                    padding: 8;

                    font.pixelSize: 14;

                    textFormat: TextEdit.PlainText;

                    wrapMode: TextEdit.Wrap;
                    focus: true;
                    activeFocusOnPress: true;
                    color: textColor;
                    selectByMouse: true;
                    persistentSelection: true;
                    selectedTextColor: "black";
                    selectionColor: "lightgreen";

                    property bool controlHeld: false;

                    // Scroll the parent Flickable by targeting its contentY directly.
                    // rotationScale is pixels-per-degree of wheel rotation.
                    // One standard notch = 15 degrees.  So for N lines at H px each:
                    //   rotationScale = -(N * H) / 15
                    // Negative because scrolling up (positive rotation) must DECREASE contentY.
                    // Tune the "5" to scroll more or fewer lines per notch.
                    WheelHandler
                    {
                        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad;

                        onWheel: (event) =>
                        {
                            var pixels = (event.angleDelta.y / 120.0) * fontMetrics.height * 5;
                            var newY   = flickable.contentY - pixels;
                            newY = Math.max(0, Math.min(newY, flickable.contentHeight - flickable.height));

                            flickable.contentY = newY;
                            event.accepted = true;
                        }
                    }

                    // Own blinking caret. Attention: The built in caret of the TextEdit sometimes stopped blinking and stayed invisible, after the text had been
                    // changed from C++ (intellisense, tab, comment) or after selections. The TextEdit also controls the visibility of a cursor delegate itself,
                    // hence the delegate is empty and the caret below is a normal child, which only depends on the focus. It is solid, while the cursor moves.
                    cursorDelegate: Item
                    {
                    }

                    Rectangle
                    {
                        id: caret;

                        property bool blinkOn: true;

                        objectName: "caret";
                        x: luaEditor.cursorRectangle.x;
                        y: luaEditor.cursorRectangle.y;
                        width: 2;
                        height: luaEditor.cursorRectangle.height;
                        color: luaEditor.color;
                        visible: luaEditor.activeFocus && Qt.application.state === Qt.ApplicationActive && caret.blinkOn;

                        Timer
                        {
                            id: caretBlinkTimer;
                            interval: 530;
                            repeat: true;
                            running: luaEditor.activeFocus;
                            onTriggered: caret.blinkOn = !caret.blinkOn;
                        }

                        Connections
                        {
                            target: luaEditor;

                            function onCursorPositionChanged()
                            {
                                caret.blinkOn = true;
                                caretBlinkTimer.restart();
                            }

                            function onActiveFocusChanged()
                            {
                                caret.blinkOn = true;
                                caretBlinkTimer.restart();
                            }
                        }
                    }


                    // Attention: Tab and Shift+Tab must reach this editor and not a shortcut of the window. The window shortcuts for Tab have been removed,
                    // they also fired, when the focus was somewhere else, and depended on the current index of the model.
                    Keys.onShortcutOverride: (event) =>
                    {
                        if (event.key === Qt.Key_Tab || event.key === Qt.Key_Backtab)
                        {
                            event.accepted = true;
                        }
                    }

                    Keys.onPressed: (event) =>
                    {
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
                        if (event.key === Qt.Key_Backtab || (event.key === Qt.Key_Tab && (event.modifiers & Qt.ShiftModifier)))
                        {
                            root.unindentSelection();
                            event.accepted = true;
                        }
                        else if (event.key === Qt.Key_Tab)
                        {
                            root.indentSelection();
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
                            event.accepted = true;
                            NOWALuaEditorModel.undo();
                        }
                        else if (event.key === Qt.Key_Y && (event.modifiers & Qt.ControlModifier))
                        {
                            event.accepted = true;
                            NOWALuaEditorModel.redo();
                        }
                        else if (event.text.length === 1)
                        {
                            // Only real characters, modifier keys like Shift have no text and would cause a type error
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
                        lineNumberTimer.restart();

                        if (root.model)
                        {
                            root.model.content = luaEditor.text;

                            Qt.callLater(root.syncCurrentText);
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

                    onSelectedTextChanged:
                    {
                        if (luaEditor.controlHeld)
                        {
                            if (luaEditor.selectedText.length > 0)
                            {
                                root.highlightWordUnderCursor(luaEditor.selectedText);
                            }
                            else
                            {
                                root.clearHighlightWordUnderCursor();
                            }
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
                                    // An error without line is only shown in the details area
                                    if (line > 0)
                                    {
                                        root.highlightRuntimeError(line, start, end);
                                    }
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

                        // The file has been changed outside of the editor. Keeps the cursor and the scroll position as good as possible.
                        function onSignal_luaScriptReloaded(filePathName, content)
                        {
                            if (filePathName === root.model.filePathName && luaEditor.text !== content)
                            {
                                let oldCursorPosition = luaEditor.cursorPosition;
                                let oldContentY = flickable.contentY;

                                luaEditor.text = content;

                                luaEditor.cursorPosition = Math.min(oldCursorPosition, luaEditor.length);
                                flickable.contentY = Math.max(0, Math.min(oldContentY, flickable.contentHeight - flickable.height));
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
