import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import NOWALuaScript

Rectangle
{
    id: root

    width: parent.width;
    height: 110;
    color: "white";

    property bool hasRuntimeError: false;
    property string filePathName: "";

    property string syntaxErrorText: ""
    property string runtimeErrorText: ""

    Flickable
    {
        id: flickable;

        width: parent.width;
        height: parent.height;
        contentWidth: detailsText.width;
        contentHeight: detailsText.height;
        clip: true;

        boundsBehavior: Flickable.StopAtBounds;

        function ensureVisible(r)
        {
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

        ScrollBar.vertical: ScrollBar
        {
            id: verticalScrollBar;
            anchors.top: parent.top;
            anchors.bottom: parent.bottom;
            hoverEnabled: true;
            active: pressed || hovered;
            policy: flickable.contentHeight > flickable.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff;
        }

        TextEdit
        {
            id: detailsText;

            width: flickable.width - 16;
            padding: 8;

            wrapMode: TextEdit.Wrap;
            focus: true;
            readOnly: true;
            selectByMouse: true;
            persistentSelection: true;

            color: "black";

            selectedTextColor: "black";
            selectionColor: "lightgreen";

            text: root.syntaxErrorText + (root.syntaxErrorText !== "" && root.runtimeErrorText !== "" ? "\n" : "") + root.runtimeErrorText;

            onCursorRectangleChanged:
            {
                flickable.ensureVisible(cursorRectangle);
            }

            Connections
            {
                target: LuaScriptQmlAdapter

                function onSignal_syntaxCheckResult(filePathName, valid, line, start, end, message)
                {
                    if (filePathName === root.filePathName)
                    {
                        if (valid)
                        {
                            // Clear syntax errors if valid
                            root.syntaxErrorText = "";
                        }
                        else
                        {
                            // Update syntax error text if invalid
                            let newMessage = "Error at line: " + line + " Details: " + message + "\n";
                            if (!root.syntaxErrorText.includes(newMessage))
                            {
                                root.syntaxErrorText += newMessage;
                            }
                        }
                    }
                }

                function onSignal_runtimeErrorResult(filePathName, valid, line, start, end, message)
                {
                    if (filePathName === root.filePathName)
                    {
                        if (valid)
                        {
                            // Clear runtime errors if valid
                            root.runtimeErrorText = "";
                        }
                        else
                        {
                            // Update runtime error text if invalid
                            let newMessage = "Runtime error at line: " + line + " Details: " + message + "\n";
                            if (!root.runtimeErrorText.includes(newMessage))
                            {
                                root.runtimeErrorText += newMessage;
                            }
                        }
                    }
                }
            }
        }
    }
}
