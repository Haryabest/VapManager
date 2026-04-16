import QtQuick 2.12
import QtQuick.Controls 2.12

Rectangle {
    id: root
    width: parent ? parent.width : 160
    height: 70
    radius: 8
    color: "#F1F2F4"

    property string title: ""
    property int currentCount: 0
    property int totalCount: 0
    property color accentColor: "#00C8FF"

    Column {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 4

        Text {
            text: root.title
            font.pixelSize: 11
            font.bold: true
            font.family: "Inter"
            color: "#64748B"
        }

        Row {
            spacing: 6
            anchors.horizontalCenter: parent.horizontalCenter

            Rectangle {
                width: 28
                height: 28
                radius: 14
                color: root.accentColor
                anchors.verticalCenter: parent.verticalCenter

                Text {
                    anchors.centerIn: parent
                    text: root.currentCount
                    font.pixelSize: 14
                    font.bold: true
                    font.family: "Inter"
                    color: "#0F00DB"
                }
            }

            Text {
                text: "из " + root.totalCount
                font.pixelSize: 12
                font.family: "Inter"
                color: "#64748B"
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }
}