import QtQuick 2.12
import QtQuick.Controls 2.12

import "../components"
import "../style"

ApplicationWindow {
    id: authWindow
    visible: true
    width: 520
    height: 750
    minimumWidth: 430
    minimumHeight: 600
    title: "Вход в систему"
    flags: Qt.Dialog | Qt.WindowTitleHint | Qt.WindowSystemMenuHint | Qt.WindowMaximizeButtonHint

    property string loginErrorMessage: ""
    property string registerErrorMessage: ""
    property string recoveryErrorMessage: ""

    // Stack для переключения страниц
    StackView {
        id: stackView
        anchors.fill: parent
        initialItem: loginPage

        pushEnter: Transition {
            PropertyAnimation {
                property: "x"
                from: stackView.width
                to: 0
                duration: 200
            }
        }
        pushExit: Transition {
            PropertyAnimation {
                property: "x"
                from: 0
                to: -stackView.width
                duration: 200
            }
        }
        popEnter: Transition {
            PropertyAnimation {
                property: "x"
                from: -stackView.width
                to: 0
                duration: 200
            }
        }
        popExit: Transition {
            PropertyAnimation {
                property: "x"
                from: 0
                to: stackView.width
                duration: 200
            }
        }
    }

    // Страница входа
    Component {
        id: loginPage
        LoginPage {
            errorMessage: authWindow.loginErrorMessage
            onLoginClicked: function(login, password) {
                authWindow.loginErrorMessage = ""
                authController.onLogin(login, password)
            }
            onRegisterClicked: {
                authWindow.loginErrorMessage = ""
                authWindow.registerErrorMessage = ""
                stackView.push(registerPage)
            }
            onRecoveryClicked: {
                authWindow.loginErrorMessage = ""
                authWindow.recoveryErrorMessage = ""
                stackView.push(recoveryPage)
            }
        }
    }

    // Страница регистрации
    Component {
        id: registerPage
        RegisterPage {
            errorMessage: authWindow.registerErrorMessage
            onRegisterClicked: function(login, password, confirmPassword, role, adminKey, techKey) {
                authWindow.registerErrorMessage = ""
                authController.onRegister(login, password, confirmPassword, role, adminKey, techKey)
            }
            onBackClicked: {
                authWindow.registerErrorMessage = ""
                stackView.pop()
            }
        }
    }

    // Страница восстановления
    Component {
        id: recoveryPage
        RecoveryPage {
            errorMessage: authWindow.recoveryErrorMessage
            onRecoveryClicked: function(recoveryKey) {
                authWindow.recoveryErrorMessage = ""
                authController.onRecovery(recoveryKey)
            }
            onRecoveryFromFileClicked: {
                authWindow.recoveryErrorMessage = ""
                authController.onRecoveryFromFile()
            }
            onBackClicked: {
                authWindow.recoveryErrorMessage = ""
                stackView.pop()
            }
        }
    }

    // Методы для установки ошибок из C++
    function setLoginError(error) {
        loginErrorMessage = error
    }
    function setRegisterError(error) {
        registerErrorMessage = error
    }
    function setRecoveryError(error) {
        recoveryErrorMessage = error
    }
}
