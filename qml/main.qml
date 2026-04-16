import QtQuick 2.0
import QtQuick.Controls 2.0
import QtQml 2.12

import "pages"

ApplicationWindow {
    visible: true
    width: 1280
    height: 800
    minimumWidth: 1080
    minimumHeight: 680
    title: "VAP Manager"

    // После авторизации показываем MainShellPage
    MainShellPage {
        id: mainShellPage
        anchors.fill: parent

        // Получаем данные из C++ context properties
        currentUsername: (typeof mainShellBridge !== "undefined" && mainShellBridge.currentUsername) ? mainShellBridge.currentUsername
                                                                                                     : (typeof currentUsername !== "undefined" ? currentUsername : "")
        currentUserRole: (typeof mainShellBridge !== "undefined" && mainShellBridge.currentUserRole) ? mainShellBridge.currentUserRole
                                                                                                       : (typeof currentUserRole !== "undefined" ? currentUserRole : "viewer")
    }

    Connections {
        target: mainShellPage

        function onChangeAvatarRequested() {
            if (typeof mainShellBridge !== "undefined")
                mainShellBridge.changeAvatar()
        }

        function onChangeLanguageRequested() {
            if (typeof mainShellBridge !== "undefined")
                mainShellBridge.changeLanguage()
        }

        function onShowAboutRequested() {
            if (typeof mainShellBridge !== "undefined")
                mainShellBridge.showAboutDialog()
        }

        function onShowSettings() {
            if (typeof mainShellBridge !== "undefined")
                mainShellBridge.showSettingsDialog()
        }

        function onLogoutRequested() {
            if (typeof mainShellBridge !== "undefined") {
                var ok = mainShellBridge.switchAccount()
                if (ok) {
                    mainShellPage.currentUsername = mainShellBridge.currentUsername
                    mainShellPage.currentUserRole = mainShellBridge.currentUserRole
                    if (mainShellPage.reloadDashboardData)
                        mainShellPage.reloadDashboardData()
                }
            }
        }
    }
}
