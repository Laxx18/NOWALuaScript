import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import NOWALuaScript

// Search and replace bar, shown at the top right of the editor (Ctrl+F: search, Ctrl+H: search and replace).
// - Takes over the selected text or the word under the cursor.
// - Marks all matches while typing (yellow), the current match is orange and selected in the editor.
// - Return: next match, Shift+Return: previous match, F3 / Shift+F3 also work in the editor. Escape closes the bar.
// - Replace replaces only the current match and goes to the next one. Replace All is one undo step (Ctrl+Z).
Rectangle
{
    id: root;
    objectName: "searchBar";

    width: 520;
    height: contentColumn.implicitHeight + 16;
    visible: false;

    color: "#2F4F4F";
    border.color: "#7FA7A7";
    border.width: 1;
    radius: 4;

    // The current LuaEditor (LuaEditorQml), set from Main.qml
    property var editor: null;

    property bool replaceVisible: false;
    property bool caseSensitive: true;
    property bool wholeWord: false;
    property int currentMatch: 0;
    property int matchCount: 0;
    property string infoText: "";

    function openSearch(withReplace)
    {
        if (root.editor)
        {
            let proposal = root.editor.getSearchTextProposal();
            if (proposal !== "")
            {
                searchField.text = proposal;
            }
        }

        root.replaceVisible = withReplace;
        root.infoText = "";
        root.visible = true;

        searchField.forceActiveFocus();
        searchField.selectAll();

        root.refreshSearch();
    }

    function closeSearch()
    {
        if (!root.visible)
        {
            return;
        }

        root.visible = false;
        NOWALuaEditorModel.clearSearch();
        root.currentMatch = 0;
        root.matchCount = 0;
        root.infoText = "";

        if (root.editor)
        {
            root.editor.focusEditor();
        }
    }

    // Marks all matches, without moving the cursor. Also used, when the tab changes.
    function refreshSearch()
    {
        if (!root.visible)
        {
            return;
        }

        if (searchField.text === "")
        {
            NOWALuaEditorModel.clearSearch();
            root.currentMatch = 0;
            root.matchCount = 0;
            return;
        }

        NOWALuaEditorModel.searchInTextEdit(searchField.text, root.wholeWord, root.caseSensitive);
    }

    function findNext(backwards)
    {
        if (!root.visible)
        {
            root.openSearch(root.replaceVisible);
        }

        if (searchField.text === "")
        {
            return;
        }

        root.infoText = "";
        NOWALuaEditorModel.searchNextInTextEdit(searchField.text, root.wholeWord, root.caseSensitive, backwards);
    }

    function replaceCurrent()
    {
        if (searchField.text === "")
        {
            return;
        }

        root.infoText = "";
        NOWALuaEditorModel.replaceInTextEdit(searchField.text, replaceField.text, root.wholeWord, root.caseSensitive);
    }

    function replaceAll()
    {
        if (searchField.text === "")
        {
            return;
        }

        NOWALuaEditorModel.replaceAllInTextEdit(searchField.text, replaceField.text, root.wholeWord, root.caseSensitive);
    }

    // Swallow clicks, so that they do not reach the editor below
    MouseArea
    {
        anchors.fill: parent;
        acceptedButtons: Qt.AllButtons;
    }

    Timer
    {
        id: liveSearchTimer;
        interval: 150;
        repeat: false;
        onTriggered: root.refreshSearch();
    }

    ColumnLayout
    {
        id: contentColumn;

        anchors.left: parent.left;
        anchors.right: parent.right;
        anchors.top: parent.top;
        anchors.margins: 8;
        spacing: 4;

        RowLayout
        {
            Layout.fillWidth: true;
            spacing: 4;

            ToolButton
            {
                text: root.replaceVisible ? "▾" : "▸";
                font.pixelSize: 16;
                Layout.preferredWidth: 32;
                ToolTip.visible: hovered;
                ToolTip.text: "Toggle replace (Ctrl+H)";
                onClicked: root.replaceVisible = !root.replaceVisible;
            }

            TextField
            {
                id: searchField;

                Layout.fillWidth: true;
                placeholderText: "Search";
                selectByMouse: true;

                onTextEdited: liveSearchTimer.restart();

                Keys.onReturnPressed: (event) =>
                {
                    liveSearchTimer.stop();
                    root.findNext((event.modifiers & Qt.ShiftModifier) !== 0);
                    event.accepted = true;
                }

                Keys.onEnterPressed: (event) =>
                {
                    liveSearchTimer.stop();
                    root.findNext((event.modifiers & Qt.ShiftModifier) !== 0);
                    event.accepted = true;
                }

                Keys.onEscapePressed: (event) =>
                {
                    root.closeSearch();
                    event.accepted = true;
                }
            }

            Label
            {
                Layout.preferredWidth: 90;
                horizontalAlignment: Text.AlignHCenter;
                color: (searchField.text !== "" && root.matchCount === 0) ? "#FF8080" : "white";
                text:
                {
                    if (searchField.text === "")
                    {
                        return "";
                    }
                    if (root.matchCount === 0)
                    {
                        return "No results";
                    }
                    if (root.currentMatch > 0)
                    {
                        return root.currentMatch + " of " + root.matchCount;
                    }
                    return root.matchCount + (root.matchCount === 1 ? " result" : " results");
                }
            }

            ToolButton
            {
                text: "Aa";
                checkable: true;
                checked: root.caseSensitive;
                font.bold: checked;
                Layout.preferredWidth: 36;
                ToolTip.visible: hovered;
                ToolTip.text: "Match case";
                onToggled:
                {
                    root.caseSensitive = checked;
                    root.refreshSearch();
                }
            }

            ToolButton
            {
                text: "W";
                checkable: true;
                checked: root.wholeWord;
                font.bold: checked;
                Layout.preferredWidth: 36;
                ToolTip.visible: hovered;
                ToolTip.text: "Match whole word";
                onToggled:
                {
                    root.wholeWord = checked;
                    root.refreshSearch();
                }
            }

            ToolButton
            {
                text: "↑";
                Layout.preferredWidth: 32;
                enabled: searchField.text !== "";
                ToolTip.visible: hovered;
                ToolTip.text: "Previous match (Shift+Return, Shift+F3)";
                onClicked: root.findNext(true);
            }

            ToolButton
            {
                text: "↓";
                Layout.preferredWidth: 32;
                enabled: searchField.text !== "";
                ToolTip.visible: hovered;
                ToolTip.text: "Next match (Return, F3)";
                onClicked: root.findNext(false);
            }

            ToolButton
            {
                text: "✕";
                Layout.preferredWidth: 32;
                ToolTip.visible: hovered;
                ToolTip.text: "Close (Escape)";
                onClicked: root.closeSearch();
            }
        }

        RowLayout
        {
            Layout.fillWidth: true;
            spacing: 4;
            visible: root.replaceVisible;

            Item
            {
                Layout.preferredWidth: 32;
            }

            TextField
            {
                id: replaceField;

                Layout.fillWidth: true;
                placeholderText: "Replace";
                selectByMouse: true;

                Keys.onReturnPressed: (event) =>
                {
                    root.replaceCurrent();
                    event.accepted = true;
                }

                Keys.onEnterPressed: (event) =>
                {
                    root.replaceCurrent();
                    event.accepted = true;
                }

                Keys.onEscapePressed: (event) =>
                {
                    root.closeSearch();
                    event.accepted = true;
                }
            }

            Button
            {
                text: "Replace";
                enabled: searchField.text !== "";
                ToolTip.visible: hovered;
                ToolTip.text: "Replaces the selected match and goes to the next one (Return)";
                onClicked: root.replaceCurrent();
            }

            Button
            {
                text: "Replace All";
                enabled: searchField.text !== "" && root.matchCount > 0;
                ToolTip.visible: hovered;
                ToolTip.text: "Replaces all matches. Can be undone with Ctrl+Z";
                onClicked: root.replaceAll();
            }
        }

        Label
        {
            Layout.fillWidth: true;
            visible: root.infoText !== "";
            text: root.infoText;
            color: "#C0FFC0";
        }
    }

    Connections
    {
        target: LuaScriptQmlAdapter;

        function onSignal_resultSearch(currentMatch, matchCount)
        {
            root.currentMatch = currentMatch;
            root.matchCount = matchCount;
        }

        function onSignal_resultReplaceAll(replacedCount)
        {
            root.infoText = replacedCount === 0 ? "Nothing replaced." : (replacedCount + " replaced.");
        }
    }
}
