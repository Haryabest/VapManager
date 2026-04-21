import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

import "../../components"

Rectangle {
    id: root

    property int selectedMonth: new Date().getMonth() + 1
    property int selectedYear: new Date().getFullYear()
    property var severityByDate: ({})

    signal dayClicked(var date)
    signal settingsClicked()
    signal monthChanged(int month, int year)

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

        // Header
        RowLayout {
            width: parent.width
            height: 36
            spacing: 12

            Text {
                text: Qt.locale().standaloneMonthName(selectedMonth - 1) + " " + selectedYear
                font.pixelSize: 20
                font.bold: true
                font.family: "Inter"
                color: "#1A1A1A"
                Layout.alignment: Qt.AlignVCenter
            }

            Item { Layout.fillWidth: true; height: 1 }

            Button {
                width: 32
                height: 32
                Layout.alignment: Qt.AlignVCenter
                background: Rectangle {
                    radius: 6
                    color: parent.hovered ? "#E5E7EB" : "#F3F4F6"
                }
                contentItem: Text {
                    text: "<"
                    font.pixelSize: 16
                    font.bold: true
                    font.family: "Inter"
                    color: "#1A1A1A"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    selectedMonth--
                    if (selectedMonth < 1) {
                        selectedMonth = 12
                        selectedYear--
                    }
                    root.monthChanged(selectedMonth, selectedYear)
                }
            }

            Button {
                width: 32
                height: 32
                Layout.alignment: Qt.AlignVCenter
                background: Rectangle {
                    radius: 6
                    color: parent.hovered ? "#E5E7EB" : "#F3F4F6"
                }
                contentItem: Text {
                    text: ">"
                    font.pixelSize: 16
                    font.bold: true
                    font.family: "Inter"
                    color: "#1A1A1A"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    selectedMonth++
                    if (selectedMonth > 12) {
                        selectedMonth = 1
                        selectedYear++
                    }
                    root.monthChanged(selectedMonth, selectedYear)
                }
            }

            Button {
                width: 100
                height: 32
                Layout.alignment: Qt.AlignVCenter
                background: Rectangle {
                    radius: 6
                    color: parent.hovered ? "#E5E7EB" : "#F3F4F6"
                }
                contentItem: Text {
                    text: "Настройки"
                    font.pixelSize: 12
                    font.family: "Inter"
                    color: "#1A1A1A"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: root.settingsClicked()
            }
        }

        // Legend using Row with inline elements
        Row {
            spacing: 14

            Row { spacing: 4
                Rectangle { width: 10; height: 10; radius: 5; color: "#FF0000"; anchors.verticalCenter: parent.verticalCenter }
                Text { text: "Просрочен"; font.pixelSize: 11; font.family: "Inter"; color: "#6B7280"; anchors.verticalCenter: parent.verticalCenter }
            }
            Row { spacing: 4
                Rectangle { width: 10; height: 10; radius: 5; color: "#FF8800"; anchors.verticalCenter: parent.verticalCenter }
                Text { text: "Ближайшие"; font.pixelSize: 11; font.family: "Inter"; color: "#6B7280"; anchors.verticalCenter: parent.verticalCenter }
            }
            Row { spacing: 4
                Rectangle { width: 10; height: 10; radius: 5; color: "#18CF00"; anchors.verticalCenter: parent.verticalCenter }
                Text { text: "Запланировано"; font.pixelSize: 11; font.family: "Inter"; color: "#6B7280"; anchors.verticalCenter: parent.verticalCenter }
            }
            Row { spacing: 4
                Rectangle { width: 10; height: 10; radius: 5; color: "#00E5FF"; anchors.verticalCenter: parent.verticalCenter }
                Text { text: "Обслужено"; font.pixelSize: 11; font.family: "Inter"; color: "#6B7280"; anchors.verticalCenter: parent.verticalCenter }
            }
        }

        Rectangle {
            width: parent.width
            height: 1
            color: "#E5E7EB"
        }

        CalendarGrid {
            id: calendarGrid
            width: parent.width
            height: parent.height - 100
            selectedMonth: root.selectedMonth
            selectedYear: root.selectedYear
            severityByDate: root.severityByDate
            darkMode: false
            onDayClicked: root.dayClicked(date)
        }
    }
}