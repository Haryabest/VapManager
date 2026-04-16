import QtQuick 2.12
import QtQuick.Controls 2.12

import "../components"
import "../style"

Rectangle {
    id: root

    function themeColor(name, fallback) {
        if (typeof Theme === "undefined" || Theme === null)
            return fallback
        var value = Theme[name]
        return (value === undefined || value === null) ? fallback : value
    }

    color: root.themeColor("bg", "#0F0F1A")

    signal loginClicked(string login, string password)
    signal registerClicked()
    signal recoveryClicked()

    property alias loginText: loginEdit.text
    property alias passwordText: passEdit.text
    property string errorMessage: ""

    Column {
        width: parent.width
        height: parent.height
        spacing: 0

        Rectangle {
            width: parent.width
            height: 80
            color: root.themeColor("bgSecondary", "#1a1a2e")

            Column {
                anchors.centerIn: parent
                width: parent.width
                padding: 14
                spacing: 2

                Text {
                    text: "AGV Manager"
                    font.pixelSize: 20
                    font.bold: true
                    color: root.themeColor("text", "#FFFFFF")
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Text {
                    text: "Войдите в аккаунт, чтобы продолжить работу"
                    font.pixelSize: 12
                    color: root.themeColor("textSecondary", "#A0A0B0")
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }

        Column {
            width: parent.width
            padding: 20
            spacing: 4

            Text {
                text: "Авторизация"
                font.pixelSize: 24
                font.bold: true
                color: root.themeColor("text", "#FFFFFF")
            }

            Text {
                text: "Введите логин и пароль от вашего аккаунта"
                font.pixelSize: 13
                color: root.themeColor("textSecondary", "#A0A0B0")
                wrapMode: Text.WordWrap
                width: parent.width
            }
        }

        Rectangle {
            width: parent.width - 40
            height: childrenRect.height + 28
            anchors.horizontalCenter: parent.horizontalCenter
            color: root.themeColor("bgSecondary", "#1a1a2e")
            radius: 12

            Column {
                width: parent.width - 28
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 10

                Column {
                    width: parent.width
                    spacing: 4

                    Text {
                        text: "Логин"
                        font.pixelSize: 13
                        color: root.themeColor("textSecondary", "#A0A0B0")
                    }

                    AppInput {
                        id: loginEdit
                        width: parent.width
                        placeholderText: "Логин"
                        onTextChanged: root.errorMessage = ""
                    }
                }

                Column {
                    width: parent.width
                    spacing: 4

                    Text {
                        text: "Пароль"
                        font.pixelSize: 13
                        color: root.themeColor("textSecondary", "#A0A0B0")
                    }

                    AppInput {
                        id: passEdit
                        width: parent.width
                        placeholderText: "Пароль"
                        echoMode: TextInput.Password
                        onTextChanged: root.errorMessage = ""
                    }
                }

                Text {
                    text: "Используйте данные рабочего аккаунта. При утрате доступа можно восстановить вход по ключу."
                    font.pixelSize: 11
                    color: root.themeColor("textSecondary", "#A0A0B0")
                    wrapMode: Text.WordWrap
                    width: parent.width
                }

                Item {
                    width: parent.width
                    height: root.errorMessage.trim().length > 0 ? alertRect.implicitHeight : 0
                    clip: true

                    Behavior on height {
                        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
                    }

                    Rectangle {
                        id: alertRect
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        implicitHeight: alertText.implicitHeight + 18
                        radius: 10
                        color: "#3B1620"
                        border.color: root.themeColor("error", "#FF5252")
                        border.width: 1
                        opacity: root.errorMessage.trim().length > 0 ? 1 : 0
                        y: root.errorMessage.trim().length > 0 ? 0 : -8

                        Behavior on opacity {
                            NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
                        }

                        Behavior on y {
                            NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
                        }

                        Text {
                            id: alertText
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: 9
                            text: root.errorMessage
                            wrapMode: Text.WordWrap
                            font.pixelSize: 13
                            color: "#FFFFFF"
                        }
                    }
                }

                Row {
                    width: parent.width
                    spacing: 10

                    AppButton {
                        width: (parent.width - 10) / 2
                        text: "Регистрация"
                        buttonStyle: "secondary"
                        onClicked: root.registerClicked()
                    }

                    AppButton {
                        width: (parent.width - 10) / 2
                        text: "Вход по ключу"
                        buttonStyle: "ghost"
                        onClicked: root.recoveryClicked()
                    }
                }

                AppButton {
                    width: parent.width
                    text: "Войти"
                    buttonStyle: "primary"
                    onClicked: root.loginClicked(loginEdit.text, passEdit.text)
                }
            }
        }

        Item { width: 1; height: 1 }
    }

    Keys.onPressed: {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            root.loginClicked(loginEdit.text, passEdit.text)
            event.accepted = true
        }
    }

    Component.onCompleted: {
        forceActiveFocus()
    }
}
