import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs

import NOWALuaScript

// Search Window
Window
{
    id: root;
    width: 300;
    height: 400;
    visible: false;
    title: "Search and Replace";
    flags: Qt.Window | Qt.WindowStaysOnTopHint; // Stay on top

    property bool caseSensitive: false;
    property bool wholeWord: false;
    property int matchCount: 0;

    onVisibleChanged:
    {
        if (visible)
        {
            searchField.forceActiveFocus();
        }
    }

    // Draggable area
    MouseArea
    {
        id: dragArea;
        anchors.fill: parent;
        // drag.target: parent;

        property var clickOffset: Qt.point(0, 0); // Offset from the top-left corner of the window

        onClicked: (mouse)=>
        {
            clickOffset = Qt.point(mouse.x, mouse.y); // Store the mouse position relative to the window
        }

        onPositionChanged:
        {
            // Move the window by the amount the mouse has moved since it was pressed
            root.x = mouse.x - clickOffset.x + root.x;
            root.y = mouse.y - clickOffset.y + root.y;
        }
    }

    ColumnLayout
    {
        anchors.fill: parent;
        anchors.margins: 8;

        Label
        {
            text: "Search:";
        }

        TextField
        {
            id: searchField;
            Layout.fillWidth: true;

            Keys.onReturnPressed:
            {
                NOWALuaEditorModel.searchInTextEdit(searchField.text, root.wholeWord, root.caseSensitive);
            }
        }

        Label
        {
            text: "Replace with:";
        }

        TextField
        {
            id: replaceField;
            Layout.fillWidth: true;

            Keys.onReturnPressed:
            {
                NOWALuaEditorModel.replaceInTextEdit(searchField.text, replaceField.text);
            }
        }

        Label
        {
            text: "Match count: " + root.matchCount;
        }

        RowLayout
        {
            CheckBox
            {
                id: wholeWordCheckbox;
                text: "Match whole word";
                checked: false;
                onCheckedChanged: root.wholeWord = wholeWordCheckbox.checked;
            }

            CheckBox
            {
                id: caseSensitiveCheckbox;
                text: "Case sensitive";
                checked: false;
                onCheckedChanged: root.caseSensitive = caseSensitiveCheckbox.checked;
            }
        }

        ColumnLayout
        {
            RowLayout
            {


                Button
                {
                    Layout.preferredWidth: 140;
                    text: "Search";
                    enabled: searchField.text !== "";
                    onClicked: NOWALuaEditorModel.searchInTextEdit(searchField.text, root.wholeWord, root.caseSensitive);
                }

                Button
                {
                    Layout.preferredWidth: 140;
                    text: "Search/Continue";
                    enabled: searchField.text !== "";
                    onClicked: NOWALuaEditorModel.searchContinueInTextEdit(searchField.text, root.wholeWord, root.caseSensitive);
                }
            }

            RowLayout
            {
                Button
                {
                    Layout.preferredWidth: 140;
                    text: "Replace";
                    enabled: searchField.text !== "" && replaceField.text !== "";
                    onClicked: NOWALuaEditorModel.replaceInTextEdit(searchField.text, replaceField.text);
                }

                Button
                {
                    Layout.preferredWidth: 140;
                    text: "Close";
                    onClicked:
                    {
                        root.visible = false;
                        NOWALuaEditorModel.clearSearch();
                    }
                }
            }
        }
    }

    Connections
    {
        target: NOWALuaEditorModel;

        function onSignal_setSelectedSearchText(searchText)
        {
            if (searchText !== "")
            {
                searchField.text = searchText;
                NOWALuaEditorModel.setSelectedSearchText("");
            }
        }
    }


    Connections
    {
        target: LuaScriptQmlAdapter;

        function onSignal_resultSearchMatchCount(matchCount)
        {
            root.matchCount = matchCount;
        }
    }
}
