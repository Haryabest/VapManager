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

    // Сигналы для C++
    signal recoveryClicked(string recoveryKey)
    signal recoveryFromFileClicked()
    signal backClicked()

    property string errorMessage: ""
    property alias recoveryKeyText: recoveryKeyEdit.text

    Column {
        width: parent.width
        height: parent.height
        spacing: 0

        // Заголовок
        Column {
            width: parent.width
            padding: 20
            spacing: 4

            Text {
                text: "Восстановление доступа"
                font.pixelSize: 24
                font.bold: true
                color: root.themeColor("text", "#FFFFFF")
            }

            Text {
                text: "Введите ключ восстановления, полученный при регистрации"
                font.pixelSize: 13
                color: root.themeColor("textSecondary", "#A0A0B0")
                wrapMode: Text.WordWrap
                width: parent.width
            }
        }

        // Форма
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

                // Ключ восстановления
                Column {
                    width: parent.width
                    spacing: 4

                    Text {
                        text: "Ключ восстановления"
                        font.pixelSize: 13
                        color: root.themeColor("textSecondary", "#A0A0B0")
                    }

                    AppInput {
                        id: recoveryKeyEdit
                        width: parent.width
                        placeholderText: "RK-XXXX-XXXX-XXXX"
                        onTextChanged: root.errorMessage = ""
                    }
                }

                // Анимированный alert ошибки (inline, чтобы избежать проблем qmlcache)
                Item {
                    width: parent.width
                    height: root.errorMessage.trim().length > 0 ? recAlertRect.implicitHeight : 0
                    clip: true
                    Behavior on height {
                        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
                    }
                    Rectangle {
                        id: recAlertRect
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        implicitHeight: recAlertText.implicitHeight + 18
                        radius: 10
                        color: "#3B1620"
                        border.color: root.themeColor("error", "#FF5252")
                        border.width: 1
                        opacity: root.errorMessage.trim().length > 0 ? 1 : 0
                        y: root.errorMessage.trim().length > 0 ? 0 : -8
                        Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                        Behavior on y { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                        Text {
                            id: recAlertText
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

                // Кнопка восстановления
                AppButton {
                    width: parent.width
                    text: "Войти"
                    buttonStyle: "primary"
                    onClicked: {
                        root.recoveryClicked(recoveryKeyEdit.text)
                    }
                }

                // Кнопка из файла
                AppButton {
                    width: parent.width
                    text: "Вставить ключ из файла"
                    buttonStyle: "secondary"
                    onClicked: {
                        root.recoveryFromFileClicked()
                    }
                }

                // Кнопка назад
                AppButton {
                    width: parent.width
                    text: "Назад"
                    buttonStyle: "ghost"
                    onClicked: {
                        root.backClicked()
                    }
                }
            }
        }

        Item { width: 1; height: 1 }
    }

    Keys.onPressed: {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            recoveryClicked(recoveryKeyEdit.text)
            event.accepted = true
        }
    }
}
