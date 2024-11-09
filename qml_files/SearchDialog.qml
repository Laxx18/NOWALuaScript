import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs

import NOWALuaScript

// Search Window
Window
{
    id: searchWindow;
    width: 300;
    height: 300;
    visible: false;
    title: "Search and Replace";
    flags: Qt.Window | Qt.WindowStaysOnTopHint; // Stay on top

    property bool caseSensitive: false;
    property bool wholeWord: false;

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
            searchWindow.x = mouse.x - clickOffset.x + searchWindow.x;
            searchWindow.y = mouse.y - clickOffset.y + searchWindow.y;
        }
    }

    ColumnLayout
    {
        anchors.fill: parent;
        anchors.margins: 10;

        Label
        {
            text: "Search:";
        }

        TextField
        {
            id: searchField;
            placeholderText: "Enter text to search";
            Layout.fillWidth: true;

            Keys.onReturnPressed:
            {
                NOWALuaEditorModel.searchInTextEdit(searchField.text, searchWindow.wholeWord, searchWindow.caseSensitive);
            }
        }

        Label
        {
            text: "Replace with:";
        }

        TextField
        {
            id: replaceField;
            placeholderText: "Enter replacement text";
            Layout.fillWidth: true;

            Keys.onReturnPressed:
            {
                NOWALuaEditorModel.replaceInTextEdit(searchField.text, replaceField.text);
            }
        }

        RowLayout
        {
            CheckBox
            {
                id: wholeWordCheckbox;
                text: "Match whole word";
                checked: false;
                onCheckedChanged: searchWindow.wholeWord = wholeWordCheckbox.checked;
            }

            CheckBox
            {
                id: caseSensitiveCheckbox;
                text: "Case sensitive";
                checked: false;
                onCheckedChanged: searchWindow.caseSensitive = caseSensitiveCheckbox.checked;
            }
        }

        RowLayout
        {
            Button
            {
                text: "Search";
                enabled: searchField.text !== "";
                onClicked: NOWALuaEditorModel.searchInTextEdit(searchField.text, searchWindow.wholeWord, searchWindow.caseSensitive);
            }

            Button
            {
                text: "Replace";
                enabled: searchField.text !== "" && replaceField.text !== "";
                onClicked: NOWALuaEditorModel.replaceInTextEdit(searchField.text, replaceField.text);
            }

            Button
            {
                text: "Close";
                onClicked:
                {
                    searchWindow.visible = false;
                    NOWALuaEditorModel.clearSearch();
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
}
