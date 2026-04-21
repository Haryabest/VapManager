import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

Rectangle {
    id: root

    property string username: ""
    property string userRole: "viewer"
    property int notificationCount: 0
    property string searchText: ""

    signal notificationsClicked()
    signal searchChanged(string text)
    signal accountClicked()
    signal logoutClicked()

    width: parent.width
    height: 76
    color: "#FFFFFF"

    Rectangle {
        width: parent.width
        height: 2
        anchors.bottom: parent.bottom
        color: "#E5E7EB"
    }

    Row {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 0

        // Logo area
        Rectangle {
            width: 240
            height: parent.height
            color: "#F1F2F4"

            Image {
                source: "qrc:/new/mainWindowIcons/noback/VAPManagerLogo.png"
                fillMode: Image.PreserveAspectFit
                width: 200
                height: 50
                anchors.centerIn: parent
            }
        }

        Item { width: 16; height: parent.height }

        // Title
        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4

            Text {
                text: "Календарь ТО"
                font.pixelSize: 18
                font.bold: true
                font.family: "Inter"
                color: "#1A1A1A"
            }

            Text {
                text: "AGV Manager"
                font.pixelSize: 12
                font.family: "Inter"
                color: "#6B7280"
            }
        }

        Item { width: 20; height: 1 }

        // Search
        Rectangle {
            width: 180
            height: 36
            radius: 8
            color: "#F3F4F6"
            border.color: "#E5E7EB"
            border.width: 1
            anchors.verticalCenter: parent.verticalCenter

            Row {
                anchors.fill: parent
                anchors.leftMargin: 10
                spacing: 8

                Image {
                    source: "qrc:/new/mainWindowIcons/noback/lupa.png"
                    width: 16
                    height: 16
                    fillMode: Image.PreserveAspectFit
                    anchors.verticalCenter: parent.verticalCenter
                    opacity: 0.6
                }

                TextField {
                    width: parent.width - 30
                    height: parent.height
                    placeholderText: "Поиск..."
                    placeholderTextColor: "#9CA3AF"
                    font.pixelSize: 12
                    font.family: "Inter"
                    color: "#1A1A1A"
                    background: Rectangle { color: "transparent"; border.width: 0 }
                    onTextChanged: root.searchChanged(text)
                }
            }
        }

        Item { Layout.fillWidth: true; width: 10 }

        // Notifications
        Button {
            width: 40
            height: 40
            anchors.verticalCenter: parent.verticalCenter
            background: Rectangle { color: "transparent"; radius: 8 }
            contentItem: Image {
                source: "qrc:/new/mainWindowIcons/noback/bell.png"
                width: 24
                height: 24
                fillMode: Image.PreserveAspectFit
                anchors.centerIn: parent
            }
            onClicked: root.notificationsClicked()

            Rectangle {
                visible: root.notificationCount > 0
                width: 16
                height: 16
                radius: 8
                color: "#FF3B30"
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.topMargin: 4
                anchors.rightMargin: 4

                Text {
                    anchors.centerIn: parent
                    text: root.notificationCount > 9 ? "9+" : root.notificationCount
                    font.pixelSize: 9
                    font.bold: true
                    font.family: "Inter"
                    color: "white"
                }
            }
        }

        Item { width: 8 }

        // User button
        Rectangle {
            width: 140
            height: 40
            radius: 8
            color: "#F3F4F6"
            border.color: "#E5E7EB"
            border.width: 1
            anchors.verticalCenter: parent.verticalCenter

            Row {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 8

                // Avatar
                Rectangle {
                    width: 32
                    height: 32
                    radius: 16
                    color: "#0F00DB"
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        anchors.centerIn: parent
                        text: getInitials(root.username)
                        font.pixelSize: 11
                        font.bold: true
                        font.family: "Inter"
                        color: "#FFFFFF"
                    }
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 1

                    Text {
                        text: root.username || "Пользователь"
                        font.pixelSize: 11
                        font.bold: true
                        font.family: "Inter"
                        color: "#1A1A1A"
                    }

                    Text {
                        text: getRoleText(root.userRole)
                        font.pixelSize: 9
                        font.family: "Inter"
                        color: "#6B7280"
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: userMenu.popup()
            }

            Menu {
                id: userMenu
                y: parent.height + 4

                background: Rectangle {
                    implicitWidth: 200
                    color: "#FFFFFF"
                    radius: 8
                    border.color: "#E5E7EB"
                    border.width: 1
                }

                MenuItem {
                    text: "Аккаунт: " + (root.username || "неизвестно")
                    onTriggered: root.accountClicked()
                }
                MenuSeparator {}
                MenuItem {
                    text: "О программе"
                    onTriggered: root.accountClicked()
                }
                MenuSeparator {}
                MenuItem {
                    text: "Сменить аккаунт"
                    onTriggered: root.logoutClicked()
                }
                MenuItem {
                    text: "Выйти"
                    onTriggered: Qt.quit()
                }
            }
        }
    }

    function getInitials(name) {
        var n = name.trim()
        if (n.length >= 2) return n.substring(0, 2).toUpperCase()
        if (n.length === 1) return n.toUpperCase()
        return "US"
    }

    function getRoleText(role) {
        if (role === "admin") return "Админ"
        if (role === "technician") return "Техник"
        return "Пользователь"
    }
}