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

    // No Qt.WindowStaysOnTopHint — that was overlapping every other application on the OS.
    // Qt.Dialog ties the window to its parent and raises with it.
    // Set transientParent = <your ApplicationWindow id> from outside so the OS groups them.
    flags: Qt.Window;

    property bool caseSensitive: false;
    property bool wholeWord: false;
    property int matchCount: 0;

    onVisibleChanged:
    {
        if (visible)
        {
            // FIX Issue 3: Always start with an empty search box.
            // No pre-fill, no pending text, no stale word from a previous session.
            searchField.text = "";
            root.matchCount = 0;

            // FIX Issue 2 (Return key): requestActivate() asks the OS to give this
            // Window actual keyboard focus.  Without it the Window is visible but
            // key events still go to the main window behind it, so Keys.onReturnPressed
            // in searchField never fires.
            // raise() brings it to the front first, then requestActivate() grabs focus.
            root.raise();
            root.requestActivate();

            // forceActiveFocus() routes QML keyboard events to searchField once the
            // Window has OS focus.
            searchField.forceActiveFocus();
        }
        else
        {
            // Window closing — clear the search highlight in the editor
            NOWALuaEditorModel.clearSearch();
        }
    }

    // Draggable title bar area
    MouseArea
    {
        id: dragArea;
        anchors.fill: parent;

        property var clickOffset: Qt.point(0, 0);

        onClicked: (mouse)=>
        {
            clickOffset = Qt.point(mouse.x, mouse.y);
        }

        onPositionChanged:
        {
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

            // FIX Issue 2: Return key triggers search.
            // Works because onVisibleChanged now calls requestActivate() + forceActiveFocus()
            // so this TextField actually receives OS keyboard events.
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
                        // onVisibleChanged(false) calls clearSearch() already,
                        // but also clearing the model's selected text here ensures
                        // no stale value is carried over.
                        NOWALuaEditorModel.setSelectedSearchText("");
                        searchField.text = "";
                        replaceField.text = "";
                    }
                }
            }
        }
    }

    Connections
    {
        target: NOWALuaEditorModel;

        // FIX Issue 3: Only update searchField when the window is already open
        // (the user selected text while mid-search — a useful live update).
        // When the window is closed this does nothing, so opening the window
        // fresh always shows an empty box regardless of what was selected.
        function onSignal_setSelectedSearchText(searchText)
        {
            if (root.visible && searchText !== "")
            {
                searchField.text = searchText;
            }
            // Always acknowledge so the model does not hold a stale copy
            NOWALuaEditorModel.setSelectedSearchText("");
        }
    }

    Connections
    {
        target: LuaScriptQmlAdapter;

        // FIX Issue 1: matchCount is now emitted exactly once after the full
        // rehighlight in searchInTextEdit(), not once per line.
        // So this handler fires once with the correct final total instead of
        // flickering through hundreds of partial counts.
        function onSignal_resultSearchMatchCount(matchCount)
        {
            root.matchCount = matchCount;
        }
    }
}
