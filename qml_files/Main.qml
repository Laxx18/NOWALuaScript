import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Window

import NOWALuaScript

ApplicationWindow
{
    id: root;

    visible: true;
    minimumWidth: 800;
    minimumHeight: 800;
    title: "NOWALuaScript";

    flags: Qt.Window | Qt.WindowFullscreenButtonHint
    visibility: "Maximized"

    Material.theme: Material.Yellow;
    Material.primary: Material.BlueGrey;
    Material.accent: Material.LightGreen;
    Material.foreground: "#FFFFFF";
    Material.background: "#1E1E1E";

    color: "darkslategrey";

    // Flag to control forced close
    property bool forceClose: false

    // The tab, which shall be closed after the unsaved changes question
    property int tabIndexToClose: -1

    // Lua scripts, which have been changed outside of the editor while they have unsaved changes. Asked one after the other.
    property var externallyChangedFiles: []

    // The LuaEditor of the current tab. Attention: The editors are the children of the stack layout in the order of the tabs.
    function currentEditor()
    {
        if (tabBar.currentIndex < 0 || tabBar.currentIndex >= luaEditorContainer.children.length)
        {
            return null;
        }
        return luaEditorContainer.children[tabBar.currentIndex];
    }

    function askNextExternallyChangedFile()
    {
        if (externalChangeDialog.visible || root.externallyChangedFiles.length === 0)
        {
            return;
        }

        let filePathName = root.externallyChangedFiles[0];
        externalChangeDialog.filePathName = filePathName;
        externalChangeDialog.informativeText = "The file\n" + filePathName + "\nhas been changed outside of the editor.\n\nReload it and discard your changes in the editor?";
        externalChangeDialog.visible = true;
    }

    function finishExternallyChangedFile()
    {
        let files = root.externallyChangedFiles.slice();
        files.shift();
        root.externallyChangedFiles = files;
        Qt.callLater(root.askNextExternallyChangedFile);
    }

    onClosing: function(closeEvent)
    {
        if (root.forceClose)
        {
            // Allow closing if forced
            closeEvent.accepted = true;
        }
        else if (NOWALuaEditorModel.hasChanges)
        {
            // Prevent closing initially
            closeEvent.accepted = false;
            exitAppDialog.visible = true;
        }
    }

    menuBar: MainMenu
    {
        onProjectSearchRequested: root.openProjectSearch();
    }

    header: ToolBar
    {
        Rectangle
        {
            anchors.fill: parent;
            color: "darkslategrey";
        }

        TabBar
        {
            id: tabBar;

            width: parent.width  // Set TabBar width to match the toolbar width

            onCurrentIndexChanged:
            {
               NOWALuaEditorModel.currentIndex = currentIndex;
               // Get the LuaEditor for the current index
               let luaEditor = root.currentEditor();
               if (luaEditor)
               {
                   luaEditor.checkSyntax();
               }
               // The search marks the matches in the new tab
               searchDialog.refreshSearch();
            }

            Repeater
            {
                model: NOWALuaEditorModel;

                delegate: TabButton
                {
                    text: model.title;
                    font.bold: true;
                    font.pixelSize: 16;

                    onClicked: tabBar.currentIndex = index;

                    Button
                    {
                        text: "x";
                        font.pixelSize: 14;

                        anchors.right: parent.right;
                        anchors.rightMargin: 8;
                        anchors.bottomMargin: 2;

                        onClicked:
                        {
                            closeTab(index);
                        }
                    }
                }
            }
        }
    }

    // footer: Rectangle
    // {
    //     height: 20;
    //     width: parent.width;
    //     color: "red";
    // }

    // The file of the current tab, which is highlighted in the project view
    property string currentFilePathName: "";

    function updateCurrentFilePathName()
    {
        let item = NOWALuaEditorModel.getEditorModelItem(NOWALuaEditorModel.currentIndex);
        root.currentFilePathName = item ? item.filePathName : "";
    }

    Connections
    {
        target: NOWALuaEditorModel;

        function onCurrentIndexChanged()
        {
            root.updateCurrentFilePathName();
        }
    }

    SplitView
    {
        id: mainSplitView;

        anchors.fill: parent;
        orientation: Qt.Horizontal;

        // Left: The project with all lua files, if a folder or project has been opened
        ProjectView
        {
            id: projectView;

            visible: NOWAProjectModel.isOpen;
            currentFilePathName: root.currentFilePathName;

            SplitView.preferredWidth: 280;
            SplitView.minimumWidth: 150;
        }

        // Right: The editors and below the results of the project search (Ctrl+Shift+F), if it is open.
        // Attention: Formerly a vertical SplitView. With newer Qt versions the search view only got its minimum height there, so the result list
        // was not visible. Now its height is set directly and can be changed with the handle above it.
        ColumnLayout
        {
            id: editorColumn;

            SplitView.fillWidth: true;
            spacing: 0;

            StackLayout
            {
                // NOTE: Do not change this names, as they are used for object matching in C++
                id: luaEditorContainer;
                objectName: "luaEditorContainer";

                Layout.fillWidth: true;
                Layout.fillHeight: true;
                currentIndex: tabBar.currentIndex;
            }

            // Handle to change the height of the search view
            Rectangle
            {
                Layout.fillWidth: true;
                Layout.preferredHeight: 6;
                visible: projectSearchView.visible;
                color: searchResizeArea.containsMouse || searchResizeArea.pressed ? "#7FA7A7" : "#3A5F5F";

                MouseArea
                {
                    id: searchResizeArea;

                    property real pressY: 0;
                    property real pressHeight: 0;

                    anchors.fill: parent;
                    hoverEnabled: true;
                    cursorShape: Qt.SizeVerCursor;

                    onPressed: (mouse) =>
                    {
                        searchResizeArea.pressY = mapToItem(editorColumn, mouse.x, mouse.y).y;
                        searchResizeArea.pressHeight = projectSearchView.searchHeight;
                    }

                    onPositionChanged: (mouse) =>
                    {
                        if (pressed)
                        {
                            let y = mapToItem(editorColumn, mouse.x, mouse.y).y;
                            projectSearchView.searchHeight = Math.max(120, Math.min(editorColumn.height - 150, searchResizeArea.pressHeight + searchResizeArea.pressY - y));
                        }
                    }
                }
            }

            ProjectSearchView
            {
                id: projectSearchView;

                property real searchHeight: 300;

                visible: false;

                Layout.fillWidth: true;
                Layout.preferredHeight: projectSearchView.searchHeight;

                onResultActivated: (filePathName, line, column, length) =>
                {
                    root.openSearchResult(filePathName, line, column, length);
                }

                onCloseRequested:
                {
                    projectSearchView.visible = false;
                    let editor = root.currentEditor();
                    if (editor)
                    {
                        editor.focusEditor();
                    }
                }
            }
        }
    }

    // Ctrl+Shift+F: Takes over the selected text or the word under the cursor
    function openProjectSearch()
    {
        let proposal = "";
        let editor = root.currentEditor();
        if (editor)
        {
            proposal = editor.getSearchTextProposal();
        }
        projectSearchView.openSearch(proposal);
    }

    // Opens the lua script of a search result (or switches to its tab) and selects the match. The editor of a newly opened lua script
    // is created a moment later, so it is waited for.
    function openSearchResult(filePathName, line, column, length)
    {
        searchResultTimer.filePathName = filePathName;
        searchResultTimer.line = line;
        searchResultTimer.column = column;
        searchResultTimer.length = length;
        searchResultTimer.tries = 0;

        NOWALuaEditorModel.requestAddLuaScript(filePathName);

        if (!root.selectSearchResult())
        {
            searchResultTimer.start();
        }
    }

    function selectSearchResult()
    {
        let editor = root.currentEditor();
        if (editor && editor.model && editor.model.filePathName.toLowerCase() === searchResultTimer.filePathName.toLowerCase())
        {
            editor.selectInLine(searchResultTimer.line, searchResultTimer.column, searchResultTimer.length);
            return true;
        }
        return false;
    }

    Timer
    {
        id: searchResultTimer;

        property string filePathName: "";
        property int line: 1;
        property int column: 0;
        property int length: 0;
        property int tries: 0;

        interval: 30;
        repeat: true;

        onTriggered:
        {
            searchResultTimer.tries++;
            if (root.selectSearchResult() || searchResultTimer.tries > 50)
            {
                searchResultTimer.stop();
            }
        }
    }

    // Function to close the tab
    function closeTab(index)
    {
        // Attention: The tab to close is not always the current one. Formerly the changes of the current tab were checked,
        // and after the question the current tab was closed instead of the clicked one.
        if (NOWALuaEditorModel.getHasChangesAt(index))
        {
            root.tabIndexToClose = index;
            messageDialog.visible = true;
        }
        else
        {
            // Close the tab without confirmation
            removeTab(index);
        }
    }

    // Remove tab and corresponding LuaEditor
    function removeTab(index)
    {
        NOWALuaEditorModel.requestRemoveLuaScript(index);
    }

    // Message dialog for unsaved changes prompt
    MessageDialog
    {
        id: messageDialog;
        visible: false;
        // Prompt user to save changes
        text: "Unsaved Changes";
        informativeText: "You have unsaved changes. Do you really want to close this tab?";
        buttons: MessageDialog.Yes | MessageDialog.No;

        // icon: StandardIcon.Warning;
        onAccepted:
        {
            // Close the tab if accepted
            if (root.tabIndexToClose >= 0)
            {
                removeTab(root.tabIndexToClose);
            }
            root.tabIndexToClose = -1;
        }

        onRejected:
        {
            root.tabIndexToClose = -1;
        }
    }

    MessageDialog
    {
        id: externalChangeDialog;
        visible: false;

        property string filePathName: "";

        text: "File changed outside of the editor";
        informativeText: "";
        buttons: MessageDialog.Yes | MessageDialog.No;

        onAccepted:
        {
            NOWALuaEditorModel.reloadLuaScript(externalChangeDialog.filePathName);
            root.finishExternallyChangedFile();
        }

        onRejected:
        {
            // Keeps the changes of the editor. Saving overwrites the file.
            root.finishExternallyChangedFile();
        }
    }

    Connections
    {
        target: NOWALuaEditorModel;

        function onSignal_luaScriptChangedExternally(filePathName)
        {
            if (root.externallyChangedFiles.indexOf(filePathName) === -1)
            {
                let files = root.externallyChangedFiles.slice();
                files.push(filePathName);
                root.externallyChangedFiles = files;
            }
            root.askNextExternallyChangedFile();
        }
    }

    MessageDialog
    {
            id: exitAppDialog;
            visible: false;
            text: "Unsaved Changes";
            informativeText: "You have unsaved changes. Do you really want to close the editor?";
            buttons: MessageDialog.Yes | MessageDialog.No;

            onAccepted:
            {
                root.forceClose = true; // Set the flag to bypass onClosing
                root.close(); // Trigger close again
            }
        }

    // Ctrl+G: Enter a line number and press Return to jump there. Escape closes.
    Popup
    {
        id: goToLineDialog;
        objectName: "goToLineDialog";

        property int lineCount: 1;

        x: (parent.width - width) / 2;
        y: 60;
        width: 300;
        modal: false;
        focus: true;
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside;

        function openDialog()
        {
            let editor = root.currentEditor();
            if (!editor)
            {
                return;
            }

            goToLineDialog.lineCount = editor.getLineCount();
            lineNumberField.text = "";
            goToLineDialog.open();
            lineNumberField.forceActiveFocus();
        }

        function goToLine()
        {
            let editor = root.currentEditor();
            let lineNumber = parseInt(lineNumberField.text);
            if (editor && !isNaN(lineNumber))
            {
                lineNumber = Math.max(1, Math.min(lineNumber, goToLineDialog.lineCount));
                goToLineDialog.close();
                editor.goToLine(lineNumber);
            }
        }

        onClosed:
        {
            let editor = root.currentEditor();
            if (editor)
            {
                editor.focusEditor();
            }
        }

        background: Rectangle
        {
            color: "#2F4F4F";
            border.color: "#7FA7A7";
            border.width: 1;
            radius: 4;
        }

        ColumnLayout
        {
            anchors.fill: parent;
            spacing: 4;

            Label
            {
                text: "Go to line (1 - " + goToLineDialog.lineCount + "):";
                color: "white";
            }

            TextField
            {
                id: lineNumberField;

                Layout.fillWidth: true;
                placeholderText: "Line number";
                selectByMouse: true;
                validator: IntValidator { bottom: 1; }

                Keys.onReturnPressed: (event) =>
                {
                    goToLineDialog.goToLine();
                    event.accepted = true;
                }

                Keys.onEnterPressed: (event) =>
                {
                    goToLineDialog.goToLine();
                    event.accepted = true;
                }
            }
        }
    }

    SearchDialog
    {
        id: searchDialog;

        // Top right above the editor, like in other editors
        anchors.top: parent.top;
        anchors.right: parent.right;
        anchors.topMargin: 4;
        anchors.rightMargin: 40;
        z: 3;

        editor: tabBar.currentIndex >= 0 && tabBar.currentIndex < luaEditorContainer.children.length ? luaEditorContainer.children[tabBar.currentIndex] : null;
    }

    IntelliSenseContextMenu
    {
        id: intelliSenseContextMenu;
        z: 2;
        visible: false;
    }

    // Overlay Rectangle to detect clicks outside of the menu
    Rectangle
    {
        id: overlay;
        anchors.fill: parent;
        color: "transparent";
        z: 1;
        visible: intelliSenseContextMenu.visible;

        MouseArea
        {
            anchors.fill: parent;
            onClicked:
            {
                intelliSenseContextMenu.visible = false;
                NOWAApiModel.isIntellisenseShown = false;
            }
        }
    }

    MatchedFunctionContextMenu
    {
        id: matchedFunctionContextMenu;
        visible: false;
    }

    // Search, takes over the selected text or the word under the cursor
    Shortcut
    {
        sequence: "Ctrl+F";
        onActivated: searchDialog.openSearch(false);
    }

    // Search and replace
    Shortcut
    {
        sequence: "Ctrl+H";
        onActivated: searchDialog.openSearch(true);
    }

    // Go to line
    Shortcut
    {
        sequence: "Ctrl+G";
        onActivated: goToLineDialog.openDialog();
    }

    Shortcut
    {
        sequence: "F3";
        onActivated: searchDialog.findNext(false);
    }

    Shortcut
    {
        sequence: "Shift+F3";
        onActivated: searchDialog.findNext(true);
    }

    // Note: Tab and Shift+Tab are handled by the editor itself (LuaEditor.qml). As window shortcuts they also fired in other fields and could hit another tab.

    Shortcut
    {
        sequence: "Ctrl+Shift+7";
        onActivated:
        {
            NOWALuaEditorModel.commentLines();
        }
    }

    Shortcut
    {
        sequence: "Ctrl+Shift+8";
        onActivated:
        {
            NOWALuaEditorModel.unCommentLines();
        }
    }

    Connections
    {
        target: LuaScriptQmlAdapter;

        function onSignal_changeTab(newTabIndex)
        {
            tabBar.currentIndex = newTabIndex; // Change the current tab index
        }
    }

    Connections
    {
        target: NOWAApiModel;

        function onSignal_showIntelliSenseMenu(resultType, x, y)
        {
            intelliSenseContextMenu.open(resultType, x, y);
            matchedFunctionContextMenu.close();
        }

        function onSignal_closeIntellisense()
        {
            intelliSenseContextMenu.close();
        }
    }

    Connections
    {
        target: NOWAApiModel;

        function onSignal_showMatchedFunctionMenu(x, y)
        {
            matchedFunctionContextMenu.open(x, y);
        }

        function onSignal_closeMatchedFunctionMenu()
        {
            matchedFunctionContextMenu.close();
        }
    }
}
