import QtQuick 2.12
import QtQuick.Controls 2.12

import "../components"
import "../js/auth.js" as Auth
import "../style"

Rectangle {
    color: Theme.bg

    Column {
        width: parent.width
        spacing: 16
        anchors.centerIn: parent
        padding: 20

        Text {
            text: "Регистрация"
            font.pixelSize: 28
            font.bold: true
            color: Theme.text
            anchors.horizontalCenter: parent.horizontalCenter
        }

        AppInput {
            id: email
            width: 300
            placeholderText: "Email"
            validator: RegExpValidator {
                regExp: /^[^\s@]+@[^\s@]+\.[^\s@]+$/
            }
        }

        AppInput {
            id: login
            width: 300
            placeholderText: "Логин"
        }

        AppInput {
            id: password
            width: 300
            placeholderText: "Пароль"
            echoMode: TextInput.Password
        }

        AppInput {
            id: confirmPassword
            width: 300
            placeholderText: "Подтвердите пароль"
            echoMode: TextInput.Password
        }

        Text {
            id: errorMessage
            color: Theme.error
            font.pixelSize: 14
            visible: false
            wrapMode: Text.WordWrap
            width: 300
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            id: successMessage
            color: Theme.success
            font.pixelSize: 14
            visible: false
            wrapMode: Text.WordWrap
            width: 300
            horizontalAlignment: Text.AlignHCenter
        }

        AppButton {
            width: 300
            text: "Зарегистрироваться"
            onClicked: {
                errorMessage.visible = false
                successMessage.visible = false
                
                var result = Auth.register(email.text, login.text, password.text, confirmPassword.text)
                if (!result.success) {
                    errorMessage.text = result.message
                    errorMessage.visible = true
                } else {
                    successMessage.text = result.message
                    successMessage.visible = true
                    
                    // Очистка полей после успешной регистрации
                    email.text = ""
                    login.text = ""
                    password.text = ""
                    confirmPassword.text = ""
                }
            }
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 5
            
            Text {
                text: "Уже есть аккаунт?"
                color: Theme.textSecondary
                font.pixelSize: 14
            }
            
            Text {
                text: "Войти"
                color: Theme.primary
                font.pixelSize: 14
                font.underline: true
                
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        // TODO: Переход на страницу входа
                        console.log("Переход на страницу входа")
                    }
                }
            }
        }
    }
}
