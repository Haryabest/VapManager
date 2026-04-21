import QtQuick 2.12
import QtQuick.Controls 2.12

import "../../style"
import "../../components"

Rectangle {
    id: root
    visible: true
    width: 520
    height: 700
    minimumWidth: 430
    minimumHeight: 580
    color: "#FFFFFF"

    property string currentView: "login" // login, register, recovery
    property string loginError: ""
    property string registerError: ""
    property string recoveryError: ""

    // User data
    property alias loginUsername: loginPage.username
    property alias loginPassword: loginPage.password

    signal loginRequested(string username, string password)
    signal registerRequested(string username, string password, string confirmPassword, string role, string adminKey, string techKey)
    signal recoveryRequested(string recoveryKey)
    signal recoveryFromFileRequested()
    signal switchToLogin()
    signal switchToRegister()
    signal switchToRecovery()

    function showLogin() {
        currentView = "login"
    }

    function showRegister() {
        currentView = "register"
    }

    function showRecovery() {
        currentView = "recovery"
    }

    // Header
    Rectangle {
        id: header
        width: parent.width
        height: 90
        color: "#F1F2F4"

        Column {
            anchors.centerIn: parent
            spacing: 4

            Text {
                text: "AGV Manager"
                font.pixelSize: 22
                font.bold: true
                font.family: "Inter"
                color: "#1A1A1A"
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text {
                text: getHeaderSubtitle()
                font.pixelSize: 12
                font.family: "Inter"
                color: "#6B7280"
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }

    function getHeaderSubtitle() {
        switch (currentView) {
            case "login": return "Войдите в аккаунт"
            case "register": return "Создайте новый аккаунт"
            case "recovery": return "Восстановите доступ"
            default: return ""
        }
    }

    // Page title
    Column {
        anchors.top: header.bottom
        anchors.topMargin: 24
        anchors.left: parent.left
        anchors.leftMargin: 40
        spacing: 4

        Text {
            text: getPageTitle()
            font.pixelSize: 26
            font.bold: true
            font.family: "Inter"
            color: "#1A1A1A"
        }

        Text {
            text: getPageDescription()
            font.pixelSize: 13
            font.family: "Inter"
            color: "#6B7280"
            wrapMode: Text.WordWrap
            width: root.width - 80
        }
    }

    function getPageTitle() {
        switch (currentView) {
            case "login": return "Авторизация"
            case "register": return "Регистрация"
            case "recovery": return "Восстановление"
            default: return ""
        }
    }

    function getPageDescription() {
        switch (currentView) {
            case "login": return "Введите логин и пароль от вашего аккаунта"
            case "register": return "Укажите логин, пароль и роль пользователя"
            case "recovery": return "Введите ключ восстановления"
            default: return ""
        }
    }

    // Form container
    Rectangle {
        anchors.top: header.bottom
        anchors.topMargin: 120
        anchors.left: parent.left
        anchors.leftMargin: 20
        anchors.right: parent.right
        anchors.rightMargin: 20
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        color: "#FFFFFF"
        radius: 12
        border.color: "#E5E7EB"
        border.width: 1

        Column {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 16
        }
    }

    // Content based on current view
    StackLayout {
        id: contentStack
        anchors.top: header.bottom
        anchors.topMargin: 120
        anchors.left: parent.left
        anchors.leftMargin: 40
        anchors.right: parent.right
        anchors.rightMargin: 40
        currentIndex: currentView === "login" ? 0 : currentView === "register" ? 1 : 2

        // Login form
        LoginForm {
            id: loginPage
            onLogin: root.loginRequested(username, password)
            onSwitchToRegister: root.showRegister()
            onSwitchToRecovery: root.showRecovery()
        }

        // Register form
        RegisterForm {
            id: registerPage
            onRegister: root.registerRequested(username, password, confirmPassword, role, adminKey, techKey)
            onBack: root.showLogin()
        }

        // Recovery form
        RecoveryForm {
            id: recoveryPage
            onRecover: root.recoveryRequested(recoveryKey)
            onLoadFromFile: root.recoveryFromFileRequested()
            onBack: root.showLogin()
        }
    }

    // Show/hide forms based on current view
    onCurrentViewChanged: {
        loginPage.visible = currentView === "login"
        registerPage.visible = currentView === "register"
        recoveryPage.visible = currentView === "recovery"
    }
}