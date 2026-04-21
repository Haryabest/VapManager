import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

import "../components"
import "../style"
import "../../modules/shell"

Rectangle {
    id: root

    // Property bindings from C++
    property string currentUsername: mainShellBridge ? mainShellBridge.currentUsername : ""
    property string currentUserRole: mainShellBridge ? mainShellBridge.currentUserRole : "viewer"
    property int activeAgvCount: 0
    property string activePage: "calendar"
    property int notifCount: 0
    property int selectedMonth: new Date().getMonth() + 1
    property int selectedYear: new Date().getFullYear()
    property int systemActiveCount: 0
    property int systemMaintenanceCount: 0
    property int systemErrorCount: 0
    property int systemDisabledCount: 0
    property var calendarSeverityByDate: ({})
    property string userAvatar: ""

    // Signals
    signal addAgvRequested()
    signal showAgvList()
    signal showAgvDetails(string agvId)
    signal showUsersPage()
    signal showModelList()
    signal showCalendar()
    signal showLogs()
    signal showSettings()
    signal showNotifications()
    signal showChats()
    signal showAnnualReport()
    signal changeAvatarRequested()
    signal changeLanguageRequested()
    signal showAboutRequested()
    signal logoutRequested()
    signal performSearch(string text)

    color: "#F1F2F4"

    // Helper functions
    function changeMonth(delta) {
        selectedMonth += delta
        if (selectedMonth < 1) {
            selectedMonth = 12
            selectedYear--
        } else if (selectedMonth > 12) {
            selectedMonth = 1
            selectedYear++
        }
    }

    function reloadDashboardData() {
        if (typeof mainShellBridge === "undefined" || !mainShellBridge) {
            console.log("mainShellBridge not available")
            return
        }
        console.log("reloadDashboardData called")
        try {
            var st = mainShellBridge.loadSystemStatus()
            if (st) {
                systemActiveCount = st.active || 0
                systemMaintenanceCount = st.maintenance || 0
                systemErrorCount = st.error || 0
                systemDisabledCount = st.disabled || 0
                activeAgvCount = systemActiveCount + systemMaintenanceCount + systemErrorCount + systemDisabledCount
            }
            console.log("Status loaded:", systemActiveCount, systemMaintenanceCount, systemErrorCount, systemDisabledCount)

            notifCount = mainShellBridge.unreadNotificationsCount() || 0
            console.log("Notif count:", notifCount)

            var map = {}
            var events = mainShellBridge.loadCalendarEvents(selectedMonth, selectedYear)
            if (events) {
                for (var i = 0; i < events.length; i++) {
                    var ev = events[i]
                    map[ev.date] = ev.severity
                }
            }
            calendarSeverityByDate = map
            console.log("Calendar events:", events ? events.length : 0)

            upcomingModel.clear()
            var upcoming = mainShellBridge.loadUpcomingMaintenance(selectedMonth, selectedYear)
            if (upcoming) {
                console.log("Upcoming:", upcoming.length)
                for (var j = 0; j < upcoming.length; j++) {
                    var item = upcoming[j]
                    upcomingModel.append({
                        agvName: item.agvName || item.agvId || "",
                        agvId: item.agvId || "",
                        taskCount: item.count || 0,
                        severity: item.severity === "red" ? "overdue" : "soon",
                        dueDate: toRuDate(item.date || "")
                    })
                }
            }
            console.log("Data loading complete")
            
            userAvatar = mainShellBridge.loadUserAvatar() || ""
        } catch (e) {
            console.log("Error loading data: " + e)
        }
    }

    function toRuDate(isoDate) {
        if (!isoDate) return ""
        var d = new Date(isoDate + "T00:00:00")
        if (isNaN(d.getTime())) return isoDate
        return d.toLocaleDateString("ru-RU", { day: "2-digit", month: "long" })
    }

    function getInitials(name) {
        if (!name) return "US"
        var n = name.trim()
        if (n.length >= 2) return n.substring(0, 2).toUpperCase()
        if (n.length === 1) return n.toUpperCase()
        return "US"
    }

    function severityColor(sev) {
        if (sev === "overdue") return "#FF0000"
        if (sev === "soon") return "#FF8800"
        if (sev === "planned") return "#18CF00"
        if (sev === "completed") return "#00E5FF"
        return "transparent"
    }

    onSelectedMonthChanged: reloadDashboardData()
    onSelectedYearChanged: reloadDashboardData()
    Component.onCompleted: {
        console.log("MainShellPage loaded")
        Qt.callLater(function() { reloadDashboardData() })
    }

    ListModel {
        id: upcomingModel
    }

    // Header
    Rectangle {
        id: headerBar
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
            spacing: 8

            // Logo
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

            // Title
            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4

                Text {
                    text: "Календарь технического обслуживания"
                    font.pixelSize: 18
                    font.bold: true
                    font.family: "Inter"
                    color: "#1A1A1A"
                }

                Text {
                    text: "Отслеживание графиков обслуживания AGV"
                    font.pixelSize: 12
                    font.family: "Inter"
                    color: "#6B7280"
                }
            }

            Item { width: 20; height: 1 }

            // Spacer to push right section
            Item { Layout.fillWidth: true }

            // Chats button
            Button {
                width: 50
                height: 50
                anchors.verticalCenter: parent.verticalCenter
                background: Rectangle {
                    radius: 10
                    color: parent.hovered ? "#E5E7EB" : "#F3F4F6"
                    border.color: "#E5E7EB"
                    border.width: 1
                }
                contentItem: Image {
                    source: "qrc:/new/mainWindowIcons/noback/logs.png"
                    width: 24
                    height: 24
                    fillMode: Image.PreserveAspectFit
                    anchors.centerIn: parent
                }
                ToolTip.visible: hovered
                ToolTip.text: "Чаты"
                onClicked: root.showChats()
            }

            // Notifications
            Button {
                width: 50
                height: 50
                anchors.verticalCenter: parent.verticalCenter
                background: Rectangle {
                    radius: 10
                    color: parent.hovered ? "#E5E7EB" : "#F3F4F6"
                    border.color: "#E5E7EB"
                    border.width: 1
                }
                contentItem: Image {
                    source: "qrc:/new/mainWindowIcons/noback/bell.png"
                    width: 24
                    height: 24
                    fillMode: Image.PreserveAspectFit
                    anchors.centerIn: parent
                }
                ToolTip.visible: hovered
                ToolTip.text: "Уведомления"
                onClicked: root.showNotifications()

                Rectangle {
                    visible: root.notifCount > 0
                    width: 18
                    height: 18
                    radius: 9
                    color: "#FF3B30"
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.topMargin: 2
                    anchors.rightMargin: 2

                    Text {
                        anchors.centerIn: parent
                        text: root.notifCount > 9 ? "9+" : root.notifCount
                        font.pixelSize: 10
                        font.bold: true
                        font.family: "Inter"
                        color: "white"
                    }
                }
            }

            // User button
            Rectangle {
                width: 180
                height: 50
                radius: 10
                color: "#F3F4F6"
                border.color: "#E5E7EB"
                border.width: 1
                anchors.verticalCenter: parent.verticalCenter

                Row {
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 8

                    // Avatar
                    Item {
                        width: 38
                        height: 38
                        anchors.verticalCenter: parent.verticalCenter

                        Rectangle {
                            id: avatarBg
                            anchors.fill: parent
                            radius: 19
                            color: "#0F00DB"
                            visible: userAvatar.length === 0
                        }

                        Image {
                            id: avatarImg
                            anchors.fill: parent
                            source: userAvatar
                            fillMode: Image.PreserveAspectCrop
                            visible: userAvatar.length > 0
                            layer.enabled: true
                            layer.smooth: true
                        }

                        Text {
                            anchors.centerIn: parent
                            text: getInitials(root.currentUsername)
                            font.pixelSize: 13
                            font.bold: true
                            font.family: "Inter"
                            color: "#FFFFFF"
                            visible: userAvatar.length === 0
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: userMenu.open()
                        }
                    }

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2

                        Text {
                            text: root.currentUsername || "Пользователь"
                            font.pixelSize: 12
                            font.bold: true
                            font.family: "Inter"
                            color: "#1A1A1A"
                        }

                        Text {
                            text: root.currentUserRole === "admin" ? "Админ" : (root.currentUserRole === "technician" ? "Техник" : "Пользователь")
                            font.pixelSize: 10
                            font.family: "Inter"
                            color: "#6B7280"
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: userMenu.open()
                }

                Menu {
                    id: userMenu
                    y: parent.height + 5

                    background: Rectangle {
                        implicitWidth: 200
                        color: "#FFFFFF"
                        radius: 8
                        border.color: "#E5E7EB"
                        border.width: 1
                    }

                    MenuItem {
                        text: "Аккаунт: " + (root.currentUsername || "неизвестно")
                        onTriggered: root.showProfile()
                    }
                    MenuSeparator {}
                    MenuItem {
                        text: "Редактировать профиль"
                        onTriggered: root.showProfile()
                    }
                    MenuItem {
                        text: "Сменить аватар"
                        onTriggered: root.changeAvatarRequested()
                    }
                    MenuItem {
                        text: "Сменить язык"
                        onTriggered: root.changeLanguageRequested()
                    }
                    MenuSeparator {}
                    MenuItem {
                        text: "О программе"
                        onTriggered: root.showAboutRequested()
                    }
                    MenuSeparator {}
                    MenuItem {
                        text: "Сменить аккаунт"
                        onTriggered: root.logoutRequested()
                    }
                    MenuItem {
                        text: "Выйти из приложения"
                        onTriggered: Qt.quit()
                    }
                }
            }
        }
    }

    // Main content
    Row {
        width: parent.width
        height: parent.height - 76
        spacing: 8
        anchors.margins: 8

        // Left sidebar
        Column {
            width: 260
            height: parent.height
            spacing: 8

            // Navigation
            Rectangle {
                width: parent.width
                height: childrenRect.height + 20
                color: "#FFFFFF"
                radius: 10
                border.color: "#E5E7EB"
                border.width: 1

                Column {
                    width: parent.width
                    anchors.margins: 10
                    spacing: 2

                    SideNavButton {
                        text: "Пользователи"
                        iconSource: "qrc:/new/mainWindowIcons/noback/user.png"
                        selected: root.activePage === "users"
                        onNavClicked: {
                            root.activePage = "users"
                            root.showUsersPage()
                        }
                    }

                    Row {
                        width: parent.width
                        spacing: 6

                        SideNavButton {
                            width: parent.width - 32
                            text: "AGV"
                            iconSource: "qrc:/new/mainWindowIcons/noback/agvIcon.png"
                            selected: root.activePage === "agvList"
                            onNavClicked: {
                                root.activePage = "agvList"
                                root.showAgvList()
                            }
                        }

                        Rectangle {
                            width: 24
                            height: 24
                            radius: 12
                            color: "#0F00DB"
                            anchors.verticalCenter: parent.verticalCenter

                            Text {
                                anchors.centerIn: parent
                                text: root.activeAgvCount
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
                        onNavClicked: {
                            root.activePage = "models"
                            root.showModelList()
                        }
                    }

                    SideNavButton {
                        text: "Годовой отчёт"
                        iconSource: "qrc:/new/mainWindowIcons/noback/YearListPrint.png"
                        selected: root.activePage === "annualReport"
                        onNavClicked: {
                            root.activePage = "annualReport"
                            root.showAnnualReport()
                        }
                    }

                    SideNavButton {
                        text: "Настройки"
                        iconSource: "qrc:/new/mainWindowIcons/noback/agvSetting.png"
                        selected: root.activePage === "settings"
                        onNavClicked: {
                            root.activePage = "settings"
                            root.showSettings()
                        }
                    }

                    Item { width: 1; height: 10 }

                    // Add AGV button
                    Button {
                        width: parent.width - 20
                        height: 40
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: root.currentUserRole !== "viewer"
                        background: Rectangle {
                            radius: 8
                            color: parent.hovered ? "#1A4ACD" : "#0F00DB"
                        }
                        contentItem: Text {
                            text: "+ Добавить AGV"
                            font.pixelSize: 14
                            font.bold: true
                            font.family: "Inter"
                            color: "#FFFFFF"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: root.addAgvRequested()
                    }
                }
            }

            // Status
            Rectangle {
                width: parent.width
                height: parent.height - y
                color: "#FFFFFF"
                radius: 10
                border.color: "#E5E7EB"
                border.width: 1

                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    Text {
                        text: "Статус системы"
                        font.pixelSize: 14
                        font.bold: true
                        font.family: "Inter"
                        color: "#1A1A1A"
                    }

                    Grid {
                        width: parent.width
                        columns: 2
                        columnSpacing: 8
                        rowSpacing: 8

                        StatusCard {
                            title: "Активные"
                            currentCount: root.systemActiveCount
                            totalCount: root.activeAgvCount
                            accentColor: "#18CF00"
                            width: (parent.width - 8) / 2
                        }

                        StatusCard {
                            title: "Обслуживание"
                            currentCount: root.systemMaintenanceCount
                            totalCount: root.activeAgvCount
                            accentColor: "#FF8800"
                            width: (parent.width - 8) / 2
                        }

                        StatusCard {
                            title: "Ошибка"
                            currentCount: root.systemErrorCount
                            totalCount: root.activeAgvCount
                            accentColor: "#FF0000"
                            width: (parent.width - 8) / 2
                        }

                        StatusCard {
                            title: "Отключены"
                            currentCount: root.systemDisabledCount
                            totalCount: root.activeAgvCount
                            accentColor: "#6B7280"
                            width: (parent.width - 8) / 2
                        }
                    }

                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 80
                        height: 28
                        background: Rectangle {
                            radius: 6
                            color: parent.hovered ? "#E5E7EB" : "#F3F4F6"
                            border.color: "#E5E7EB"
                            border.width: 1
                        }
                        contentItem: Row {
                            spacing: 4
                            Image {
                                source: "qrc:/new/mainWindowIcons/noback/logs.png"
                                width: 14
                                height: 14
                                fillMode: Image.PreserveAspectFit
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: "Logs"
                                font.pixelSize: 12
                                font.family: "Inter"
                                color: "#1A1A1A"
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                        onClicked: {
                            root.activePage = "logs"
                            root.showLogs()
                        }
                    }

                    Text {
                        text: "Версия: 1.0.0"
                        font.pixelSize: 10
                        font.family: "Inter"
                        color: "#9CA3AF"
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }
            }
        }

        // Right content
        Column {
            width: parent.width - 280
            height: parent.height
            spacing: 8

            // Calendar
            CalendarView {
                width: parent.width
                height: parent.height * 0.58
                selectedMonth: root.selectedMonth
                selectedYear: root.selectedYear
                severityByDate: root.calendarSeverityByDate

                onMonthChanged: {
                    root.selectedMonth = month
                    root.selectedYear = year
                }

                onDayClicked: root.showCalendar()
                onSettingsClicked: root.showSettings()
            }

            // Upcoming maintenance list
            Rectangle {
                width: parent.width
                height: parent.height * 0.38
                color: "#FFFFFF"
                radius: 10
                border.color: "#E5E7EB"
                border.width: 1

                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    Text {
                        text: "Предстоящее обслуживание"
                        font.pixelSize: 16
                        font.bold: true
                        font.family: "Inter"
                        color: "#1A1A1A"
                    }

                    ListView {
                        id: upcomingList
                        width: parent.width
                        height: parent.height - 40
                        clip: true
                        spacing: 6
                        model: upcomingModel

                        delegate: Rectangle {
                            width: ListView.view.width
                            height: 48
                            radius: 8
                            color: "#F8FAFC"
                            border.color: "#E5E7EB"
                            border.width: 1

                            Row {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 10

                                Rectangle {
                                    width: 10
                                    height: 10
                                    radius: 5
                                    color: severityColor(severity)
                                    anchors.verticalCenter: parent.verticalCenter
                                }

                                Text {
                                    text: agvName
                                    font.pixelSize: 13
                                    font.bold: true
                                    font.family: "Inter"
                                    color: "#1A1A1A"
                                    anchors.verticalCenter: parent.verticalCenter
                                }

                                Text {
                                    text: taskCount + " задач"
                                    font.pixelSize: 12
                                    font.family: "Inter"
                                    color: "#6B7280"
                                    anchors.verticalCenter: parent.verticalCenter
                                }

                                Item { width: 10; height: 1 }

                                Text {
                                    text: dueDate
                                    font.pixelSize: 12
                                    font.family: "Inter"
                                    color: "#9CA3AF"
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.showAgvDetails(agvId)
                            }
                        }

                        Rectangle {
                            visible: upcomingModel.count === 0
                            anchors.centerIn: parent
                            color: "transparent"

                            Text {
                                text: "Нет предстоящего обслуживания"
                                font.pixelSize: 13
                                font.family: "Inter"
                                color: "#9CA3AF"
                            }
                        }
                    }
                }
            }
        }
    }
}