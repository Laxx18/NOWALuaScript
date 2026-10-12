import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import NOWALuaScript

// Search in the whole project (Ctrl+Shift+F) below the editor. Shows all matches grouped by file, a click on a match opens the lua script and selects it.
// - Return in the search field starts the search, Escape closes the view.
// - Without an opened project (folder or project file) the open lua scripts are searched.
Rectangle
{
    id: root;
    objectName: "projectSearchView";

    color: "#263D3D";

    // A match has been clicked
    signal resultActivated(string filePathName, int line, int column, int length);

    signal closeRequested();

    property string infoText: "";
    property int currentRow: -1;
    property int matchCount: 0;

    // The rows of the result list: File headers and matches
    property var rows: [];

    function openSearch(proposal)
    {
        root.visible = true;

        if (proposal !== "")
        {
            searchField.text = proposal;
        }

        searchField.forceActiveFocus();
        searchField.selectAll();
    }

    function startSearch()
    {
        if (searchField.text === "")
        {
            return;
        }

        root.infoText = "Searching...";
        NOWAProjectModel.searchInProject(searchField.text, wholeWordCheckBox.checked, caseSensitiveCheckBox.checked);
    }

    function activateRow(index)
    {
        if (index < 0 || index >= root.rows.length)
        {
            return;
        }

        let row = root.rows[index];
        if (row.isHeader)
        {
            return;
        }

        root.currentRow = index;
        root.resultActivated(row.filePathName, row.line, row.column, row.length);
    }

    function escapeHtml(text)
    {
        return text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/\t/g, "    ").replace(/ /g, "&nbsp;");
    }

    ColumnLayout
    {
        anchors.fill: parent;
        spacing: 0;

        Rectangle
        {
            Layout.fillWidth: true;
            // Attention: Not a fixed height. The Material TextField of newer Qt versions is higher (outlined style), it stuck out of a 44 pixel bar.
            Layout.preferredHeight: headerRow.implicitHeight + 8;
            color: "#2F4F4F";

            RowLayout
            {
                id: headerRow;

                anchors.fill: parent;
                anchors.leftMargin: 8;
                anchors.rightMargin: 2;
                spacing: 6;

                Label
                {
                    text: NOWAProjectModel.isOpen ? "Search in project:" : "Search in open files:";
                    color: "white";
                    font.bold: true;
                }

                TextField
                {
                    id: searchField;
                    objectName: "projectSearchField";

                    // Note: No placeholderText, the outlined Material style shows it as floating label above the field
                    Layout.preferredWidth: 300;
                    Layout.alignment: Qt.AlignVCenter;
                    selectByMouse: true;
                    color: "white";

                    Keys.onReturnPressed: (event) =>
                    {
                        root.startSearch();
                        event.accepted = true;
                    }

                    Keys.onEnterPressed: (event) =>
                    {
                        root.startSearch();
                        event.accepted = true;
                    }

                    Keys.onEscapePressed: (event) =>
                    {
                        root.closeRequested();
                        event.accepted = true;
                    }
                }

                CheckBox
                {
                    id: wholeWordCheckBox;
                    objectName: "projectSearchWholeWord";
                    text: "Whole word";
                    focusPolicy: Qt.NoFocus;
                }

                CheckBox
                {
                    id: caseSensitiveCheckBox;
                    objectName: "projectSearchCaseSensitive";
                    text: "Match case";
                    focusPolicy: Qt.NoFocus;
                }

                Button
                {
                    text: "Search";
                    focusPolicy: Qt.NoFocus;
                    enabled: searchField.text !== "" && !NOWAProjectModel.isSearching;
                    onClicked: root.startSearch();
                }

                Label
                {
                    Layout.fillWidth: true;
                    text: root.infoText;
                    color: "#C0C0C0";
                    elide: Text.ElideRight;
                }

                ToolButton
                {
                    text: "✕";
                    Layout.preferredWidth: 30;
                    focusPolicy: Qt.NoFocus;
                    ToolTip.visible: hovered;
                    ToolTip.text: "Close (Escape)";
                    onClicked: root.closeRequested();
                }
            }
        }

        // Attention: Formerly a ListModel with sections. Now a plain list of rows (file headers and matches), which works the same in all Qt versions.
        ListView
        {
            id: resultListView;
            objectName: "projectSearchResultList";

            Layout.fillWidth: true;
            Layout.fillHeight: true;
            clip: true;
            boundsBehavior: Flickable.StopAtBounds;

            model: root.rows;

            ScrollBar.vertical: ScrollBar
            {
                policy: ScrollBar.AsNeeded;
            }

            delegate: Item
            {
                id: rowDelegate;

                required property int index;
                required property var modelData;

                width: ListView.view.width;
                height: rowDelegate.modelData.isHeader ? 26 : 22;

                Rectangle
                {
                    anchors.fill: parent;
                    color: rowDelegate.modelData.isHeader ? "#2F4F4F" : (rowDelegate.index === root.currentRow ? "steelblue" : (rowHover.hovered ? "#3A5F5F" : "transparent"));
                }

                HoverHandler
                {
                    id: rowHover;
                }

                // File header: relative path and number of matches
                Label
                {
                    visible: rowDelegate.modelData.isHeader;
                    anchors.verticalCenter: parent.verticalCenter;
                    x: 8;
                    width: parent.width - 16;
                    elide: Text.ElideMiddle;
                    text: rowDelegate.modelData.isHeader ? rowDelegate.modelData.relativePath + "  (" + rowDelegate.modelData.count + ")" : "";
                    color: "#E0E0A0";
                    font.bold: true;
                }

                // Match: line number and the line with the highlighted match
                Label
                {
                    id: lineLabel;

                    visible: !rowDelegate.modelData.isHeader;
                    x: 24;
                    width: 50;
                    anchors.verticalCenter: parent.verticalCenter;
                    horizontalAlignment: Text.AlignRight;
                    text: rowDelegate.modelData.isHeader ? "" : rowDelegate.modelData.line + ":";
                    color: "#C0C0C0";
                    font.family: "Consolas";
                }

                Label
                {
                    visible: !rowDelegate.modelData.isHeader;
                    anchors.left: lineLabel.right;
                    anchors.leftMargin: 8;
                    anchors.right: parent.right;
                    anchors.verticalCenter: parent.verticalCenter;
                    elide: Text.ElideRight;
                    textFormat: Text.StyledText;
                    color: "white";
                    font.family: "Consolas";
                    text: rowDelegate.modelData.isHeader ? "" : root.escapeHtml(rowDelegate.modelData.preText) + "<b><font color=\"#FFD700\">" +
                                                             root.escapeHtml(rowDelegate.modelData.matchText) + "</font></b>" + root.escapeHtml(rowDelegate.modelData.postText);
                }

                TapHandler
                {
                    onTapped: root.activateRow(rowDelegate.index);
                }

                ToolTip.visible: rowHover.hovered && !rowDelegate.modelData.isHeader;
                ToolTip.delay: 800;
                ToolTip.text: rowDelegate.modelData.isHeader ? "" : rowDelegate.modelData.filePathName;
            }
        }
    }

    Connections
    {
        target: NOWAProjectModel;

        function onSearchFinished(searchText, results, searchedFileCount, truncated)
        {
            // Rows for the list: A header row for each file, then its matches
            let rows = [];
            let fileCount = 0;
            let headerRow = null;
            for (let i = 0; i < results.length; ++i)
            {
                let result = results[i];
                if (headerRow === null || headerRow.filePathName !== result.filePathName)
                {
                    headerRow = { isHeader: true, filePathName: result.filePathName, relativePath: result.relativePath, count: 0 };
                    rows.push(headerRow);
                    fileCount++;
                }
                headerRow.count++;

                rows.push({ isHeader: false, filePathName: result.filePathName, relativePath: result.relativePath, line: result.line, column: result.column,
                            length: result.length, preText: result.preText, matchText: result.matchText, postText: result.postText });
            }

            root.currentRow = -1;
            root.rows = rows;
            root.matchCount = results.length;
            resultListView.positionViewAtBeginning();

            if (results.length === 0)
            {
                root.infoText = "\"" + searchText + "\" not found (" + searchedFileCount + " files searched)";
            }
            else
            {
                root.infoText = results.length + " matches in " + fileCount + " files (" + searchedFileCount + " files searched)" +
                        (truncated ? ", only the first " + results.length + " are shown" : "");
            }
        }
    }
}
