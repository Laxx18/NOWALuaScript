import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material

Dialog
{
    id: root;
    width: 500;
    modal: true;

    Material.theme: Material.Yellow;
    Material.primary: Material.BlueGrey;
    Material.accent: Material.LightGreen;
    Material.foreground: "#FFFFFF";
    Material.background: "#1E1E1E";

    ColumnLayout
    {
        anchors.fill: parent;
        spacing: 10;

        Label
        {
            text: "NOWALuaScript";
            font.pointSize: 18;
            font.bold: true;
            horizontalAlignment: Text.AlignHCenter;
            Layout.alignment: Qt.AlignHCenter;
        }

        Label
        {
            text: "Author: Lukas Kalinowski";
            font.pointSize: 14;
            horizontalAlignment: Text.AlignHCenter;
        }

        RowLayout
        {
            spacing: 4;

            Label
            {
                text: "Website:";
                font.pointSize: 14;
            }

            Text
            {
                text: "https://lukas-kalinowski.com";
                color: "blue";
                font.underline: true;
                font.pointSize: 14;

                MouseArea
                {
                    anchors.fill: parent;
                    cursorShape: Qt.PointingHandCursor;

                    onClicked:
                    {
                        Qt.openUrlExternally("https://lukas-kalinowski.com");
                    }
                }
            }
        }

        Label
        {
            text: "E-Mail: lukas.kalinowski1505@gmail.com";
            font.pointSize: 14;
        }

        Label
        {
            text: "License: GNU Lesser General Public License v2.1";
            font.pointSize: 14;
        }

        // Close button
        Button
        {
            text: "Close";
            Layout.alignment: Qt.AlignHCenter;

            onClicked:
            {
                root.close();
            }
        }
    }
}
