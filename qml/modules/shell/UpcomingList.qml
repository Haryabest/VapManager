import QtQuick 2.12
import QtQuick.Controls 2.12

Rectangle {
    id: root

    property int itemCount: listModel.count
    signal itemClicked(string agvId)

    ListModel { id: listModel }

    width: parent.width
    height: parent.height
    color: "#FFFFFF"
    radius: 10
    border.color: "#E5E7EB"
    border.width: 1

    Column {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        Text {
            text: "Предстоящее обслуживание"
            font.pixelSize: 16
            font.bold: true
            font.family: "Inter"
            color: "#1A1A1A"
        }

        ListView {
            id: listView
            width: parent.width
            height: parent.height - 40
            clip: true
            spacing: 6
            model: listModel

            delegate: Rectangle {
                width: ListView.view.width
                height: 48
                radius: 8
                color: "#F8FAFC"
                border.color: "#E5E7EB"
                border.width: 1

                Row {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    Rectangle {
                        width: 10
                        height: 10
                        radius: 5
                        color: getSeverityColor(severity)
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        text: agvName
                        font.pixelSize: 13
                        font.bold: true
                        font.family: "Inter"
                        color: "#1A1A1A"
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        text: taskCount + " задач"
                        font.pixelSize: 12
                        font.family: "Inter"
                        color: "#6B7280"
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Item { width: 10; height: 1 }

                    Text {
                        text: dueDate
                        font.pixelSize: 12
                        font.family: "Inter"
                        color: "#9CA3AF"
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.itemClicked(agvId)
                }

                function getSeverityColor(sev) {
                    if (sev === "overdue") return "#FF0000"
                    if (sev === "soon") return "#FF8800"
                    if (sev === "planned") return "#18CF00"
                    if (sev === "completed") return "#00E5FF"
                    return "#9CA3AF"
                }
            }

            Rectangle {
                visible: listModel.count === 0
                anchors.centerIn: parent
                color: "transparent"

                Text {
                    text: "Нет предстоящего обслуживания"
                    font.pixelSize: 13
                    font.family: "Inter"
                    color: "#9CA3AF"
                }
            }
        }
    }

    function addItem(data) {
        listModel.append(data)
    }

    function clearItems() {
        listModel.clear()
    }
}