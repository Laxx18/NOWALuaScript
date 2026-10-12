import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import NOWALuaScript

// Project view on the left side: All lua files below the opened folder as tree. A click on a lua file opens it (or switches to its tab).
Rectangle
{
    id: root;
    objectName: "projectView";

    color: "#263D3D";

    // The file of the current tab, which is highlighted in the tree
    property string currentFilePathName: "";

    function expandAll()
    {
        treeView.expandRecursively();
    }

    ColumnLayout
    {
        anchors.fill: parent;
        spacing: 0;

        Rectangle
        {
            Layout.fillWidth: true;
            Layout.preferredHeight: 32;
            color: "#2F4F4F";

            RowLayout
            {
                anchors.fill: parent;
                anchors.leftMargin: 8;
                anchors.rightMargin: 2;
                spacing: 2;

                Label
                {
                    Layout.fillWidth: true;
                    text: NOWAProjectModel.projectName + (NOWAProjectModel.projectFilePathName === "" ? " (not saved)" : "");
                    font.bold: true;
                    elide: Text.ElideRight;
                    color: "white";

                    ToolTip.visible: projectNameArea.containsMouse;
                    ToolTip.text: NOWAProjectModel.projectFilePathName !== "" ? NOWAProjectModel.projectFilePathName : NOWAProjectModel.rootFolder;

                    MouseArea
                    {
                        id: projectNameArea;
                        anchors.fill: parent;
                        hoverEnabled: true;
                        acceptedButtons: Qt.NoButton;
                    }
                }

                ToolButton
                {
                    text: "⟳";
                    font.pixelSize: 16;
                    Layout.preferredWidth: 30;
                    ToolTip.visible: hovered;
                    ToolTip.text: "Refresh";
                    onClicked: NOWAProjectModel.refresh();
                }

                ToolButton
                {
                    text: "✕";
                    Layout.preferredWidth: 30;
                    ToolTip.visible: hovered;
                    ToolTip.text: "Close project";
                    onClicked: NOWAProjectModel.closeProject();
                }
            }
        }

        Label
        {
            Layout.fillWidth: true;
            Layout.margins: 8;
            visible: NOWAProjectModel.luaFileCount === 0;
            text: "No lua files in this folder.";
            color: "#C0C0C0";
            wrapMode: Text.Wrap;
        }

        TreeView
        {
            id: treeView;
            objectName: "projectTreeView";

            Layout.fillWidth: true;
            Layout.fillHeight: true;
            clip: true;

            model: NOWAProjectModel;

            ScrollBar.vertical: ScrollBar
            {
                policy: ScrollBar.AsNeeded;
            }

            delegate: Item
            {
                id: treeDelegate;

                required property TreeView treeView;
                required property bool isTreeNode;
                required property bool expanded;
                required property int hasChildren;
                required property int depth;
                required property int row;
                required property string display;
                required property string filePathName;
                required property bool isFolder;

                readonly property bool isCurrent: !treeDelegate.isFolder && root.currentFilePathName !== "" &&
                                                  treeDelegate.filePathName.toLowerCase() === root.currentFilePathName.toLowerCase();

                implicitWidth: Math.max(treeView.width, label.x + label.implicitWidth + 8);
                implicitHeight: 24;

                Rectangle
                {
                    anchors.fill: parent;
                    color: treeDelegate.isCurrent ? "steelblue" : (hoverHandler.hovered ? "#3A5F5F" : "transparent");
                }

                HoverHandler
                {
                    id: hoverHandler;
                }

                Label
                {
                    id: indicator;

                    x: 6 + treeDelegate.depth * 16;
                    width: 14;
                    anchors.verticalCenter: parent.verticalCenter;
                    text: treeDelegate.isFolder ? (treeDelegate.expanded ? "▾" : "▸") : "";
                    color: "#C0C0C0";
                }

                Label
                {
                    id: label;

                    x: indicator.x + indicator.width + 2;
                    anchors.verticalCenter: parent.verticalCenter;
                    text: treeDelegate.display;
                    color: treeDelegate.isFolder ? "#E0E0A0" : "white";
                    font.bold: treeDelegate.isCurrent;
                }

                TapHandler
                {
                    onTapped:
                    {
                        if (treeDelegate.isFolder)
                        {
                            treeDelegate.treeView.toggleExpanded(treeDelegate.row);
                        }
                        else
                        {
                            NOWALuaEditorModel.requestAddLuaScript(treeDelegate.filePathName);
                        }
                    }
                }

                ToolTip.visible: hoverHandler.hovered && !treeDelegate.isFolder;
                ToolTip.delay: 800;
                ToolTip.text: treeDelegate.filePathName;
            }
        }
    }

    Connections
    {
        target: NOWAProjectModel;

        // The tree has been built again (opened, or lua files added or removed): All folders are shown expanded
        function onProjectContentChanged()
        {
            Qt.callLater(root.expandAll);
        }
    }

    Component.onCompleted: Qt.callLater(root.expandAll);
}
