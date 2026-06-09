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
        LuaScriptQmlAdapter.checkSyntax(root.model.filePathName, luaEditor.originalText);
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

                    onActiveFocusChanged:
                    {
                        if (activeFocus)
                        {
                            luaEditor.cursorVisible = false;
                            luaEditor.cursorVisible = true;
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
                            event.accepted = true;
                            NOWALuaEditorModel.undo();
                        }
                        else if (event.key === Qt.Key_Y && (event.modifiers & Qt.ControlModifier))
                        {
                            event.accepted = true;
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
                        lineNumberTimer.restart();

                        if (root.model)
                        {
                            root.model.content = luaEditor.text;

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
                        if (luaEditor.selectedText.length > 0)
                        {
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
