import QtQuick 2.12
import QtQuick.Controls 2.12

import "../../components"

Rectangle {
    id: root

    property int activeCount: 0
    property int maintenanceCount: 0
    property int errorCount: 0
    property int disabledCount: 0
    property int totalCount: 0

    width: 260
    height: 220
    color: "#FFFFFF"
    radius: 10
    border.color: "#E5E7EB"
    border.width: 1

    Column {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        Text {
            text: "Статус системы"
            font.pixelSize: 14
            font.bold: true
            font.family: "Inter"
            color: "#1A1A1A"
        }

        Grid {
            width: parent.width
            columns: 2
            columnSpacing: 8
            rowSpacing: 8

            StatusCard {
                title: "Активные"
                currentCount: root.activeCount
                totalCount: root.totalCount
                accentColor: "#18CF00"
                width: (parent.width - 8) / 2
                height: 68
            }

            StatusCard {
                title: "Обслуживание"
                currentCount: root.maintenanceCount
                totalCount: root.totalCount
                accentColor: "#FF8800"
                width: (parent.width - 8) / 2
                height: 68
            }

            StatusCard {
                title: "Ошибка"
                currentCount: root.errorCount
                totalCount: root.totalCount
                accentColor: "#FF0000"
                width: (parent.width - 8) / 2
                height: 68
            }

            StatusCard {
                title: "Отключены"
                currentCount: root.disabledCount
                totalCount: root.totalCount
                accentColor: "#6B7280"
                width: (parent.width - 8) / 2
                height: 68
            }
        }

        Button {
            width: 100
            height: 28
            anchors.horizontalCenter: parent.horizontalCenter
            background: Rectangle { color: "transparent" }
            contentItem: Row {
                spacing: 6
                anchors.horizontalCenter: parent.horizontalCenter

                Image {
                    source: "qrc:/new/mainWindowIcons/noback/logs.png"
                    width: 14
                    height: 14
                    fillMode: Image.PreserveAspectFit
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    text: "Logs"
                    font.pixelSize: 12
                    font.bold: true
                    font.family: "Inter"
                    color: "#1A1A1A"
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            onClicked: logsClicked()
        }

        Text {
            text: "Версия: 1.0.0"
            font.pixelSize: 10
            font.family: "Inter"
            color: "#9CA3AF"
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }

    signal logsClicked()
}