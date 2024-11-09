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

    Component.onCompleted:
    {
        console.debug("----> LuaEditorQml LOADED");
    }

    // Hugh performance impact and cursor is visible just after a while on a large file
    // onVisibleChanged:
    // {
    //     if (visible)
    //     {
    //         luaEditor.forceActiveFocus();
    //     }
    // }

    property string textColor: "black";

    property int leftPadding: 4;
    property int topPadding: 4;

    // Property to track if a new line is being inserted
    property bool isInsertingNewLine: false

    Rectangle
    {
        anchors.fill: parent
        // color: "darkslategrey";
        color: "white";
        border.color: "darkgrey";
        border.width: 2;
    }

    function checkSyntax()
    {
        LuaScriptQmlAdapter.checkSyntax(root.model.filePathName, luaEditor.originalText);
    }

    Connections
    {
        target: root
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

            contentWidth: luaEditor.contentWidth;  // Content width is equal to the width of the TextEdit
            contentHeight: luaEditor.contentHeight + 12; // Ensure extra space for last line
            clip: true;  // Clip content that exceeds the viewable area
            // Dangerous!
            // interactive: false; // Prevent flickable from capturing input events

            boundsBehavior: Flickable.StopAtBounds;

            ScrollBar.vertical: ScrollBar
            {
                width: 20;
                x: flickable.width - 12;
                policy: ScrollBar.AlwaysOn;
            }

            // MouseArea
            // {
            //     anchors.fill: parent;
            //     propagateComposedEvents: true;

            //     onClicked:
            //     {
            //         // Ensure luaEditor receives focus on click
            //         luaEditor.forceActiveFocus();
            //     }
            // }

            // Synchronize scroll position
            function ensureVisible(r)
            {
                if (root.isInsertingNewLine)
                {
                    // Do nothing if a new line is being inserted, because it corrupts the "r" with wrong values and so there is always an ugly jump
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

                // Line Number View
                TextEdit
                {
                    id: lineNumbersEdit;
                    objectName: "lineNumbersEdit";

                    readOnly: true;
                    width: 50;
                    padding: 8;
                    wrapMode: TextEdit.NoWrap;
                    color: textColor;
                    font.family: luaEditor.font.family;
                    font.pixelSize: luaEditor.font.pixelSize;

                    // // Keep line numbers updated
                    Component.onCompleted: updateLineNumbers();
                    onTextChanged: updateLineNumbers();

                    function updateLineNumbers()
                    {
                        var lineCount = luaEditor.text.split("\n").length;
                        var lineText = "";
                        for (var i = 1; i <= lineCount; i++)
                        {
                            lineText += i + "\n";
                        }
                        lineNumbersEdit.text = lineText;
                    }

                    onCursorRectangleChanged:
                    {
                        flickable.ensureVisible(cursorRectangle);
                    }
                }

                // Lua Code Editor
                TextEdit
                {
                    id: luaEditor;
                    objectName: "luaEditor";

                    Component.onCompleted: lineNumbersEdit.updateLineNumbers();

                    width: parent.width - 12;
                    height: parent.height;
                    padding: 8;

                    font.pixelSize: 14;

                    textFormat: TextEdit.AutoText;
                    wrapMode: TextEdit.Wrap;
                    focus: true;
                    activeFocusOnPress: true; // Enable focus on press for text selection
                    color: textColor;
                    selectByMouse: true;
                    persistentSelection: true;
                    selectedTextColor: "black";
                    selectionColor: "lightgreen";

                    Keys.onPressed: (event) =>
                    {
                        // If context menu is shown, relay those key to the context menu for navigation
                        if (NOWAApiModel.isIntellisenseShown)
                        {
                            if (event.key === Qt.Key_Tab || event.key === Qt.Key_Up || event.key === Qt.Key_Down)
                            {
                                LuaScriptQmlAdapter.relayKeyPress(event.key);
                                event.accepted = true; // Prevents default behavior if necessary
                                return;
                            }
                        }
                        if (event.key === Qt.Key_Tab)
                        {
                            NOWALuaEditorModel.addTabToSelection();
                            // Prevent the default Tab behavior
                            event.accepted = true;
                        }
                        else if (event.key === Qt.Key_Return)
                        {
                            // Prevent the default Return behavior
                            event.accepted = true;
                            NOWALuaEditorModel.breakLine();
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

                    onTextChanged:
                    {
                        checkSyntaxTimer.restart();
                        lineNumbersEdit.updateLineNumbers();  // Sync line numbers on text change

                        // // Sets new changes to model
                        if (root.model)
                        {
                            root.model.content = luaEditor.text;
                        }
                    }

                    onCursorRectangleChanged:
                    {
                        flickable.ensureVisible(cursorRectangle);
                    }

                    onCursorPositionChanged:
                    {
                        // Notify C++ whenever the cursor changes
                        root.cursorPositionChanged(cursorPosition);
                    }

                    // Track changes in selected text
                        onSelectedTextChanged:
                        {
                            if (luaEditor.selectedText.length > 0)
                            {
                                // A word was selected, trigger the search in LuaHighlighter
                                root.highlightWordUnderCursor(luaEditor.selectedText);
                                NOWALuaEditorModel.setSelectedSearchText(luaEditor.selectedText);
                            }
                            else
                            {
                                root.clearHighlightWordUnderCursor();
                            }
                        }

                    // Add MouseArea to track the mouse position
                    // MouseArea
                    // {
                    //     id: mouseTracker;
                    //     anchors.fill: parent;
                    //     hoverEnabled: true;
                    //     acceptedButtons: Qt.NoButton  // This allows text interaction to pass through

                    //     property real mouseX: 0;
                    //     property real mouseY: 0;

                    //     onPositionChanged: (mouse) =>
                    //     {
                    //         mouseX = mouse.x;
                    //         mouseY = mouse.y;
                    //     }
                    // }

                    Connections
                    {
                        target: LuaScriptQmlAdapter;

                        function onSignal_syntaxCheckResult(filePathName, valid, line, start, end, message)
                        {
                            if (filePathName === root.model.filePathName)
                            {
                                if (!valid)
                                {
                                    root.highlightError(line, start, end); // Highlight the error line
                                }
                                else
                                {
                                    root.clearError(); // Clear the error highlight
                                }
                            }
                        }

                        function onSignal_runtimeErrorResult(filePathName, valid, line, start, end, message)
                        {
                            if (filePathName === root.model.filePathName)
                            {
                                if (!valid)
                                {
                                    root.highlightRuntimeError(line, start, end); // Highlight the error line
                                }
                                else
                                {
                                    root.clearRuntimeError(); // Clear the error highlight
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
            Layout.fillWidth: true;
            Layout.preferredHeight: 110;

            filePathName: root.model.filePathName;
        }
    }

    Timer
    {
        id: checkSyntaxTimer;
        interval: 2000;
        repeat: false;
        onTriggered:
        {
            LuaScriptQmlAdapter.checkSyntax(root.model.filePathName, luaEditor.text);
        }
    }

    FontMetrics
    {
        id: fontMetrics;
        font: luaEditor.font;
    }
}
