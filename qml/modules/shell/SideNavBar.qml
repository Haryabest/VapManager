import QtQuick 2.12
import QtQuick.Controls 2.12

import "../../components"

Rectangle {
    id: root

    property string activePage: "calendar"
    property int agvCount: 0
    property string userRole: "viewer"

    signal navigationClicked(string page)
    signal addAgvClicked()

    width: 260
    height: parent.height
    color: "#F8FAFC"

    Column {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        // Navigation card
        Rectangle {
            width: parent.width
            height: childrenRect.height + 16
            color: "#FFFFFF"
            radius: 10
            border.color: "#E5E7EB"
            border.width: 1

            Column {
                width: parent.width
                anchors.margins: 8
                spacing: 2

                SideNavButton {
                    text: "Пользователи"
                    iconSource: "qrc:/new/mainWindowIcons/noback/user.png"
                    selected: root.activePage === "users"
                    onNavClicked: root.navigationClicked("users")
                }

                Row {
                    width: parent.width
                    spacing: 6

                    SideNavButton {
                        width: parent.width - 32
                        text: "AGV"
                        iconSource: "qrc:/new/mainWindowIcons/noback/agvIcon.png"
                        selected: root.activePage === "agvList"
                        onNavClicked: root.navigationClicked("agvList")
                    }

                    Rectangle {
                        width: 24
                        height: 24
                        radius: 12
                        color: "#0F00DB"
                        anchors.verticalCenter: parent.verticalCenter

                        Text {
                            anchors.centerIn: parent
                            text: root.agvCount
                            font.pixelSize: 12
                            font.bold: true
                            font.family: "Inter"
                            color: "#FFFFFF"
                        }
                    }
                }

                SideNavButton {
                    text: "Модель AGV"
                    iconSource: "qrc:/new/mainWindowIcons/noback/edit.png"
                    selected: root.activePage === "models"
                    onNavClicked: root.navigationClicked("models")
                }

                SideNavButton {
                    text: "Отчеты"
                    iconSource: "qrc:/new/mainWindowIcons/noback/YearListPrint.png"
                    selected: root.activePage === "reports"
                    onNavClicked: root.navigationClicked("reports")
                }

                SideNavButton {
                    text: "Настройки"
                    iconSource: "qrc:/new/mainWindowIcons/noback/agvSetting.png"
                    selected: root.activePage === "settings"
                    onNavClicked: root.navigationClicked("settings")
                }

                Item { width: 1; height: 8 }

                Button {
                    width: parent.width - 48
                    height: 36
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: root.userRole !== "viewer"
                    background: Rectangle {
                        radius: 8
                        color: parent.hovered ? "#1A4ACD" : "#0F00DB"
                    }
                    contentItem: Text {
                        text: "+ Добавить AGV"
                        font.pixelSize: 13
                        font.bold: true
                        font.family: "Inter"
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: root.addAgvClicked()
                }
            }
        }

        // Status card
        Rectangle {
            width: parent.width
            height: childrenRect.height + 16
            color: "#FFFFFF"
            radius: 10
            border.color: "#E5E7EB"
            border.width: 1

            Column {
                width: parent.width
                anchors.margins: 10
                spacing: 8

                Text {
                    text: "Статус"
                    font.pixelSize: 12
                    font.bold: true
                    font.family: "Inter"
                    color: "#1A1A1A"
                }

                Button {
                    width: 80
                    height: 24
                    anchors.horizontalCenter: parent.horizontalCenter
                    background: Rectangle { color: "transparent" }
                    contentItem: Row {
                        spacing: 4
                        anchors.horizontalCenter: parent.horizontalCenter

                        Image {
                            source: "qrc:/new/mainWindowIcons/noback/logs.png"
                            width: 12
                            height: 12
                            fillMode: Image.PreserveAspectFit
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            text: "Logs"
                            font.pixelSize: 11
                            font.family: "Inter"
                            color: "#1A1A1A"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                    onClicked: root.navigationClicked("logs")
                }

                Text {
                    text: "v1.0.0"
                    font.pixelSize: 9
                    font.family: "Inter"
                    color: "#9CA3AF"
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }

        Item { width: 1; height: 1 }
    }
}