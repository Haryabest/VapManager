import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

import "../../components"
import "../../style"

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

            if (upcomingList) {
                upcomingList.clearItems()
                var upcoming = mainShellBridge.loadUpcomingMaintenance(selectedMonth, selectedYear)
                if (upcoming) {
                    console.log("Upcoming:", upcoming.length)
                    for (var j = 0; j < upcoming.length; j++) {
                        var item = upcoming[j]
                        upcomingList.addItem({
                            agvName: item.agvName || item.agvId || "",
                            agvId: item.agvId || "",
                            taskCount: item.count || 0,
                            severity: item.severity === "red" ? "overdue" : "soon",
                            dueDate: toRuDate(item.date || "")
                        })
                    }
                }
            }
            console.log("Data loading complete")
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

    onSelectedMonthChanged: reloadDashboardData()
    onSelectedYearChanged: reloadDashboardData()
    Component.onCompleted: {
        console.log("MainShellPage loaded")
        // Defer data loading so UI renders first
        Qt.callLater(function() { reloadDashboardData() })
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
                    text: root.currentUsername || "AGV Manager"
                    font.pixelSize: 12
                    font.family: "Inter"
                    color: "#6B7280"
                }
            }

            Item { width: 20; height: 1 }

            // Notifications
            Button {
                width: 40
                height: 40
                anchors.verticalCenter: parent.verticalCenter
                background: Rectangle { color: "transparent" }
                contentItem: Image {
                    source: "qrc:/new/mainWindowIcons/noback/bell.png"
                    width: 24
                    height: 24
                    fillMode: Image.PreserveAspectFit
                    anchors.centerIn: parent
                }
                onClicked: root.showNotifications()

                Rectangle {
                    visible: root.notifCount > 0
                    width: 16
                    height: 16
                    radius: 8
                    color: "#FF3B30"
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.topMargin: 4

                    Text {
                        anchors.centerIn: parent
                        text: root.notifCount > 9 ? "9+" : root.notifCount
                        font.pixelSize: 9
                        font.bold: true
                        font.family: "Inter"
                        color: "white"
                    }
                }
            }

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

                    Rectangle {
                        width: 32
                        height: 32
                        radius: 16
                        color: "#0F00DB"
                        anchors.verticalCenter: parent.verticalCenter

                        Text {
                            anchors.centerIn: parent
                            text: getInitials(root.currentUsername)
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
                            text: root.currentUsername || "User"
                            font.pixelSize: 11
                            font.bold: true
                            font.family: "Inter"
                            color: "#1A1A1A"
                        }

                        Text {
                            text: root.currentUserRole === "admin" ? "Админ" : (root.currentUserRole === "technician" ? "Техник" : "Пользователь")
                            font.pixelSize: 9
                            font.family: "Inter"
                            color: "#6B7280"
                        }
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
                // FIXED: removed childrenRect binding (causes white screen)
                height: 200
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
                        text: "Настройки"
                        iconSource: "qrc:/new/mainWindowIcons/noback/agvSetting.png"
                        selected: root.activePage === "settings"
                        onNavClicked: {
                            root.activePage = "settings"
                            root.showSettings()
                        }
                    }
                }
            }

            // Status
            Rectangle {
                width: parent.width
                height: 220
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
                        height: 24
                        background: Rectangle { color: "transparent" }
                        contentItem: Text {
                            text: "Logs"
                            font.pixelSize: 11
                            font.family: "Inter"
                            color: "#1A1A1A"
                        }
                        onClicked: root.showLogs()
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

            // Upcoming
            UpcomingList {
                id: upcomingList
                width: parent.width
                height: parent.height * 0.38

                onItemClicked: root.showAgvDetails(agvId)
            }
        }
    }

    function getInitials(name) {
        if (!name) return "US"
        var n = name.trim()
        if (n.length >= 2) return n.substring(0, 2).toUpperCase()
        if (n.length === 1) return n.toUpperCase()
        return "US"
    }
}