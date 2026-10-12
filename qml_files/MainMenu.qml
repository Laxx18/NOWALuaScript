import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Window

import NOWALuaScript

MenuBar
{
    id: mainMenu;
    focus: true;

    Component.onCompleted:
    {
        mainMenu.forceActiveFocus();
    }

    Rectangle
    {
        anchors.fill: parent;
        color: "darkslategrey";
    }

    FileDialog
    {
       id: fileOpenDialog;
       currentFolder: Qt.resolvedUrl("../../media/projects");

       nameFilters: ["Lua files (*.lua)"];

       onAccepted:
       {
           var selectedFilePath = fileOpenDialog.selectedFile.toString();

           if (Qt.platform.os === "windows")
           {
               selectedFilePath = selectedFilePath.replace("file:///", "");
           }
           else
           {
               selectedFilePath = selectedFilePath.replace("file://", "/");
           }

           NOWALuaEditorModel.addRecentFile(selectedFilePath);
           NOWALuaEditorModel.requestAddLuaScript(selectedFilePath);
       }
    }

    FileDialog
    {
       id: apiFileOpenDialog;
       currentFolder: Qt.resolvedUrl("../bin/Release");

       nameFilters: ["Lua Api File (*.lua)"];

       onAccepted:
       {
           var selectedFilePath = apiFileOpenDialog.selectedFile.toString();

           if (Qt.platform.os === "windows")
           {
               selectedFilePath = selectedFilePath.replace("file:///", "");
           }
           else
           {
               selectedFilePath = selectedFilePath.replace("file://", "/");
           }
           // false -> parseSilent: Give feedback if it could be parsed
           LuaScriptQmlAdapter.requestSetLuaApi(selectedFilePath, false);
       }
    }

    // Project: A folder with lua files, shown as tree on the left side
    FolderDialog
    {
        id: projectFolderDialog;
        title: "Open Folder";
        currentFolder: Qt.resolvedUrl("../../media/projects");

        onAccepted: NOWAProjectModel.openFolder(projectFolderDialog.selectedFolder.toString());
    }

    FileDialog
    {
        id: projectOpenDialog;
        title: "Open Project";
        currentFolder: Qt.resolvedUrl("../../media/projects");
        nameFilters: ["NOWA Lua project (*.nowaluaproj)"];

        onAccepted: NOWAProjectModel.openProject(projectOpenDialog.selectedFile.toString());
    }

    FileDialog
    {
        id: projectSaveDialog;
        title: "Save Project As";
        fileMode: FileDialog.SaveFile;
        defaultSuffix: "nowaluaproj";
        nameFilters: ["NOWA Lua project (*.nowaluaproj)"];

        onAccepted: NOWAProjectModel.saveProject(projectSaveDialog.selectedFile.toString());
    }

    function saveProjectAs()
    {
        let proposal = NOWAProjectModel.getDefaultProjectFilePathName();
        if (proposal !== "")
        {
            let proposalUrl = (Qt.platform.os === "windows" ? "file:///" : "file://") + proposal;
            projectSaveDialog.currentFolder = proposalUrl.substring(0, proposalUrl.lastIndexOf("/"));
            projectSaveDialog.selectedFile = proposalUrl;
        }
        projectSaveDialog.open();
    }

    function saveProject()
    {
        if (NOWAProjectModel.projectFilePathName === "")
        {
            mainMenu.saveProjectAs();
        }
        else
        {
            NOWAProjectModel.saveProject("");
        }
    }

    MessageDialog
    {
        id: projectMessageDialog;
        visible: false;
        text: "Project";
        informativeText: "--";
        buttons: MessageDialog.Ok;
    }

    Connections
    {
        target: NOWAProjectModel;

        function onProjectError(message)
        {
            projectMessageDialog.informativeText = message;
            projectMessageDialog.visible = true;
        }
    }

    // Message dialog for unsaved changes prompt
    MessageDialog
    {
        id: messageDialog;
        visible: false;
        // Prompt user to save changes
        text: "Lua api parse result:";
        informativeText: "--";
        buttons: MessageDialog.Ok;

        // icon: StandardIcon.Warning;
        onAccepted:
        {
            messageDialog.visible = false;
        }
    }

    Connections
    {
        target: LuaScriptQmlAdapter;

        function onSignal_luaApiPreparationResult(parseSilent, success, message)
        {
            if (!parseSilent)
            {
                messageDialog.informativeText = message;
                messageDialog.visible = true;
            }
        }
    }

    Menu
    {
        title: qsTr("   File   ");

        Action
        {
            text: qsTr("Open...");
            shortcut: "Ctrl+O";
            icon.name: "document-open";  // Common icon for "Open"
            onTriggered: fileOpenDialog.open();
        }

        Action
        {
            text: qsTr("Save");
            shortcut: "Ctrl+S";
            // Note: Always enabled, so that Ctrl+S always saves the current tab. Formerly it was bound to hasChanges, which was not always correct for the current tab.
            icon.name: "document-save";  // Common icon for "Save"
            onTriggered: NOWALuaEditorModel.requestSaveLuaScript();
        }

        MenuSeparator {}

        Action
        {
            text: qsTr("Open Folder...");
            onTriggered: projectFolderDialog.open();
        }

        Action
        {
            text: qsTr("Open Project...");
            onTriggered: projectOpenDialog.open();
        }

        Action
        {
            text: qsTr("Save Project");
            enabled: NOWAProjectModel.isOpen;
            onTriggered: mainMenu.saveProject();
        }

        Action
        {
            text: qsTr("Save Project As...");
            enabled: NOWAProjectModel.isOpen;
            onTriggered: mainMenu.saveProjectAs();
        }

        Action
        {
            text: qsTr("Close Project");
            enabled: NOWAProjectModel.isOpen;
            onTriggered: NOWAProjectModel.closeProject();
        }

        Menu
        {
            title: qsTr("Recent Projects");
            enabled: NOWAProjectModel.recentProjects.length > 0;

            Repeater
            {
                model: NOWAProjectModel.recentProjects;

                delegate: MenuItem
                {
                    text: modelData.split('/').pop().replace(".nowaluaproj", "") + "   (" + modelData + ")";
                    onTriggered: NOWAProjectModel.openProject(modelData);
                }
            }
        }

        MenuSeparator {}

        // Repeater to display recent files dynamically
        Repeater
        {
            model: NOWALuaEditorModel.recentFiles;

            delegate: MenuItem
            {
                text: modelData.split('/').pop()  // Extract filename from path
                onTriggered: NOWALuaEditorModel.requestAddLuaScript(modelData)
            }
        }

        // Only show this separator if there are files in the model
        MenuSeparator { visible: NOWALuaEditorModel.recentFiles.length > 0 }

        MenuItem
        {
            text: "Load All";

            visible: NOWALuaEditorModel.recentFiles.length > 0;

            onTriggered:
            {
                // Loop through all recent files and load them
                NOWALuaEditorModel.recentFiles.forEach(function(file)
                {
                    NOWALuaEditorModel.requestAddLuaScript(file);
                });
            }
        }

        Action
        {
            text: qsTr("Exit");
            shortcut: "Ctrl+Q";
            icon.name: "application-exit";  // Common icon for "Exit"
            onTriggered: Qt.quit();
        }
    }

    Menu
    {
        title: qsTr("   Edit   ");

        Action
        {
            text: qsTr("Undo");
            shortcut: "Ctrl+Z";
            icon.name: "edit-undo";

            onTriggered:
            {
                NOWALuaEditorModel.undo();
            }
        }

        Action
        {
            text: qsTr("Redo");
            shortcut: "Ctrl+Y";
            icon.name: "edit-redo";

            onTriggered:
            {
                NOWALuaEditorModel.redo();
            }
        }

        Action
        {
            text: qsTr("Copy");
            shortcut: StandardKey.Copy;
            icon.name: "edit-copy";
            onTriggered: console.log("Copy action triggered");
        }

        Action
        {
            text: qsTr("Paste");
            shortcut: StandardKey.Paste;
            icon.name: "edit-paste";
            onTriggered: console.log("Paste action triggered");
        }

        Action
        {
            text: qsTr("Cut");
            shortcut: StandardKey.Cut;
            icon.name: "edit-cut";
            onTriggered: console.log("Cut action triggered");
        }

        Action
        {
            text: qsTr("Select All");
            shortcut: StandardKey.SelectAll;
            icon.name: "edit-select-all";  // If available, for "Select All"
            onTriggered: console.log("Select All action triggered");
        }

        Action
        {
            text: qsTr("Shift Lines");
            icon.name: "Tab";
            onTriggered: NOWALuaEditorModel.addTabToSelection();
        }

        Action
        {
            text: qsTr("Unshift Lines");
            icon.name: "Tab";  // Might need to check availability
            onTriggered: NOWALuaEditorModel.removeTabFromSelection();
        }

        Action
        {
            text: qsTr("Comment");
            icon.name: "text-comment";  // Might need to check availability
            onTriggered: NOWALuaEditorModel.commentLines();
        }

        Action
        {
            text: qsTr("Uncomment");
            icon.name: "text-comment";  // Might need to check availability
            onTriggered: NOWALuaEditorModel.unCommentLines();
        }
    }

    Menu
    {
        title: qsTr("   Lua   ");

        Action
        {
            text: qsTr("Open Lua Api File");
            // icon.name: "document-save";  // Common icon for "Save"
            onTriggered: apiFileOpenDialog.open();
        }

        Action
        {
            text: qsTr("Open Lua Script Folder");
            shortcut: "Ctrl+L";
            icon.name: "folder-open";  // Common icon for opening folders
            onTriggered: NOWALuaEditorModel.openProjectFolder();
        }
    }

    AboutDialog
    {
        id: aboutDialog;

        x: (parent.width - width) / 2;
        y: (parent.parent.height - height) / 2;
    }

    Menu
    {
        title: qsTr("   Help   ");

        Action
        {
            text: qsTr("About");
            icon.name: "help-about";  // Common icon for "About"

            onTriggered:
            {
                aboutDialog.open();
            }
        }
    }
}
