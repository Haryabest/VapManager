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
    color: root.themeColor("bg", "#FFFFFF")

    signal registerClicked(string login, string password, string confirmPassword, string role, string adminKey, string techKey)
    signal backClicked()
    signal loginEdited(string text)
    signal password1Edited(string text)
    signal password2Edited(string text)
    signal roleIndexChanged(int index)

    property alias loginText: regLoginEdit.text
    property alias password1Text: regPass1Edit.text
    property alias password2Text: regPass2Edit.text
    property string errorMessage: ""
    property int passwordStrength: 0
    property string passwordStrengthText: "Надёжность: —"
    property color passwordStrengthColor: "#6B7280"
    property bool adminKeyVisible: false
    property bool techKeyVisible: false

    Column {
        width: parent.width
        height: parent.height
        spacing: 0

        // Hero секция
        Rectangle {
            width: parent.width
            height: 80
            color: root.themeColor("bgSecondary", "#F3F4F6")

            Column {
                anchors.centerIn: parent
                width: parent.width
                padding: 14
                spacing: 2

                Text {
                    text: "Создание аккаунта"
                    font.pixelSize: 20
                    font.bold: true
                    color: root.themeColor("text", "#1A1A1A")
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Text {
                    text: "Заполните данные и сохраните ключ восстановления"
                    font.pixelSize: 12
                    color: root.themeColor("textSecondary", "#6B7280")
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }

        // Заголовок
        Column {
            width: parent.width
            padding: 20
            spacing: 4

            Text {
                text: "Регистрация"
                font.pixelSize: 24
                font.bold: true
                color: root.themeColor("text", "#1A1A1A")
            }

            Text {
                text: "Укажите логин, пароль и роль пользователя"
                font.pixelSize: 13
                color: root.themeColor("textSecondary", "#6B7280")
                wrapMode: Text.WordWrap
                width: parent.width
            }
        }

        // Форма
        Rectangle {
            width: parent.width - 40
            height: childrenRect.height + 28
            anchors.horizontalCenter: parent.horizontalCenter
            color: root.themeColor("bgSecondary", "#F3F4F6")
            radius: 12

            Column {
                width: parent.width - 28
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 10

                // Логин
                Column {
                    width: parent.width
                    spacing: 4

                    Text {
                        text: "Логин"
                        font.pixelSize: 13
                        color: root.themeColor("textSecondary", "#6B7280")
                    }

                    AppInput {
                        id: regLoginEdit
                        width: parent.width
                        placeholderText: "Логин"
                        onTextChanged: root.loginEdited(text)
                    }
                }

                // Пароль
                Column {
                    width: parent.width
                    spacing: 4

                    Text {
                        text: "Пароль"
                        font.pixelSize: 13
                        color: root.themeColor("textSecondary", "#6B7280")
                    }

                    AppInput {
                        id: regPass1Edit
                        width: parent.width
                        placeholderText: "Пароль"
                        echoMode: TextInput.Password
                        onTextChanged: root.password1Edited(text)
                    }
                }

                // Повторите пароль
                Column {
                    width: parent.width
                    spacing: 4

                    Text {
                        text: "Повторите пароль"
                        font.pixelSize: 13
                        color: root.themeColor("textSecondary", "#6B7280")
                    }

                    AppInput {
                        id: regPass2Edit
                        width: parent.width
                        placeholderText: "Повторите пароль"
                        echoMode: TextInput.Password
                        onTextChanged: root.password2Edited(text)
                    }
                }

                // Сила пароля
                Row {
                    width: parent.width
                    spacing: 10

                    ProgressBar {
                        id: passStrength
                        width: parent.width - 120
                        height: 6
                        from: 0
                        to: 100
                        value: root.passwordStrength
                        background: Rectangle {
                            color: Qt.lighter(root.passwordStrengthColor, 1.5)
                            radius: 3
                        }
                        contentItem: Item {
                            Rectangle {
                                width: passStrength.visualPosition * parent.width
                                height: parent.height
                                color: root.passwordStrengthColor
                                radius: 3
                            }
                        }
                    }

                    Text {
                        id: passStrengthLabel
                        text: root.passwordStrengthText
                        color: root.passwordStrengthColor
                        font.pixelSize: 12
                        font.bold: true
                    }
                }

                // Роль
                Column {
                    width: parent.width
                    spacing: 4

                    Text {
                        text: "Роль"
                        font.pixelSize: 13
                        color: root.themeColor("textSecondary", "#6B7280")
                    }

                    AppComboBox {
                        id: regRoleCombo
                        width: parent.width
                        model: ["Пользователь", "Администратор", "Техник"]
                        onCurrentIndexChanged: root.roleIndexChanged(currentIndex)
                    }
                }

                // Ключ администратора
                Column {
                    width: parent.width
                    spacing: 4
                    visible: root.adminKeyVisible

                    Text {
                        text: "Ключ от администратора"
                        font.pixelSize: 13
                        color: root.themeColor("textSecondary", "#6B7280")
                    }

                    AppInput {
                        id: regAdminKeyEdit
                        width: parent.width
                        placeholderText: "Запросите ключ у действующего админа"
                    }
                }

                // Ключ техника
                Column {
                    width: parent.width
                    spacing: 4
                    visible: root.techKeyVisible

                    Text {
                        text: "Ключ от техника"
                        font.pixelSize: 13
                        color: root.themeColor("textSecondary", "#6B7280")
                    }

                    AppInput {
                        id: regTechKeyEdit
                        width: parent.width
                        placeholderText: "Запросите ключ у действующего техника"
                    }
                }

                // Анимированный alert ошибки
                Item {
                    id: regError
                    width: parent.width
                    height: root.errorMessage.trim().length > 0 ? regAlertRect.implicitHeight : 0
                    clip: true
                    Behavior on height {
                        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
                    }
                    Rectangle {
                        id: regAlertRect
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        implicitHeight: regAlertText.implicitHeight + 18
                        radius: 10
                        color: "#FFF0F0"
                        border.color: root.themeColor("error", "#FF3B30")
                        border.width: 1
                        opacity: root.errorMessage.trim().length > 0 ? 1 : 0
                        y: root.errorMessage.trim().length > 0 ? 0 : -8
                        Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                        Behavior on y { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                        Text {
                            id: regAlertText
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: 9
                            text: root.errorMessage
                            wrapMode: Text.WordWrap
                            font.pixelSize: 13
                            color: root.themeColor("error", "#FF3B30")
                        }
                    }
                }

                // Кнопки
                Row {
                    width: parent.width
                    spacing: 10

                    AppButton {
                        width: (parent.width - 10) / 3
                        text: "Назад"
                        buttonStyle: "secondary"
                        onClicked: root.backClicked()
                    }

                    AppButton {
                        width: (parent.width - 10) * 2 / 3
                        text: "Создать аккаунт"
                        buttonStyle: "primary"
                        onClicked: {
                            root.registerClicked(
                                regLoginEdit.text,
                                regPass1Edit.text,
                                regPass2Edit.text,
                                regRoleCombo.currentData || "viewer",
                                regAdminKeyEdit.text,
                                regTechKeyEdit.text
                            )
                        }
                    }
                }
            }
        }

        Item { width: 1; height: 1 }
    }
}