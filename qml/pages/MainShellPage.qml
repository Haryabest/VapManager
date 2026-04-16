import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

import "../components"
import "../style"

Rectangle {
    id: root
    color: "#0B1220"

    property string currentUsername: ""
    property string currentUserRole: "viewer"
    property int activeAgvCount: 0
    property string activePage: "calendar"
    property string searchText: ""
    property int notifCount: 0
    property int selectedMonth: new Date().getMonth() + 1
    property int selectedYear: new Date().getFullYear()
    property int systemActiveCount: 0
    property int systemMaintenanceCount: 0
    property int systemErrorCount: 0
    property int systemDisabledCount: 0
    property var calendarSeverityByDate: ({})

    signal addAgvRequested
    signal showAgvList
    signal showAgvDetails(string agvId)
    signal showUsersPage
    signal showModelList
    signal showCalendar
    signal showLogs
    signal showProfile
    signal showChats
    signal showSettings
    signal showNotifications
    signal showAnnualReport
    signal changeAvatarRequested
    signal changeLanguageRequested
    signal showAboutRequested
    signal logoutRequested
    signal performSearch(string text)

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

    function setAgvCount(count) {
        root.activeAgvCount = count
    }
    function setNotificationCount(count) {
        root.notifCount = count
    }
    function setActivePage(page) {
        root.activePage = page
    }
    function openAccountPage() {
        openProfileDialog()
        root.showProfile()
    }
    function openCalendarPage() {
        root.activePage = "calendar"
        root.showCalendar()
    }

    function dateKey(dateObj) {
        var y = dateObj.getFullYear()
        var m = ("0" + (dateObj.getMonth() + 1)).slice(-2)
        var d = ("0" + dateObj.getDate()).slice(-2)
        return y + "-" + m + "-" + d
    }

    function severityRank(severity) {
        if (severity === "overdue")
            return 4
        if (severity === "soon")
            return 3
        if (severity === "planned")
            return 2
        if (severity === "completed")
            return 1
        return 0
    }

    function severityColor(severity) {
        if (severity === "overdue")
            return "#FF0000"
        if (severity === "soon")
            return "#FF8800"
        if (severity === "planned")
            return "#18CF00"
        if (severity === "completed")
            return "#00E5FF"
        return "transparent"
    }

    function toRuDate(isoDate) {
        var d = new Date(isoDate + "T00:00:00")
        if (isNaN(d.getTime()))
            return isoDate
        return d.toLocaleDateString("ru-RU", {
                                        day: "2-digit",
                                        month: "long"
                                    })
    }

    function openProfileDialog() {
        if (typeof mainShellBridge === "undefined")
            return
        var p = mainShellBridge.loadCurrentUserProfile()
        fullNameInput.text = p.fullName || ""
        employeeIdInput.text = p.employeeId || ""
        positionInput.text = p.position || ""
        departmentInput.text = p.department || ""
        mobileInput.text = p.mobile || ""
        extPhoneInput.text = p.extPhone || ""
        emailInput.text = p.email || ""
        telegramInput.text = p.telegram || ""
        profileDialog.open()
    }

    function reloadDashboardData() {
        if (typeof mainShellBridge === "undefined")
            return

        var st = mainShellBridge.loadSystemStatus()
        systemActiveCount = st.active || 0
        systemMaintenanceCount = st.maintenance || 0
        systemErrorCount = st.error || 0
        systemDisabledCount = st.disabled || 0
        activeAgvCount = systemActiveCount + systemMaintenanceCount + systemErrorCount
                         + systemDisabledCount
        notifCount = mainShellBridge.unreadNotificationsCount()

        var map = {}
        var events = mainShellBridge.loadCalendarEvents(selectedMonth, selectedYear)
        for (var i = 0; i < events.length; i++) {
            var ev = events[i]
            var key = ev.date
            if (!map[key] || severityRank(ev.severity) > severityRank(map[key]))
                map[key] = ev.severity
        }
        calendarSeverityByDate = map

        upcomingModel.clear()
        var upcoming = mainShellBridge.loadUpcomingMaintenance(selectedMonth, selectedYear)
        for (var j = 0; j < upcoming.length; j++) {
            var item = upcoming[j]
            var sev = item.severity === "red" ? "overdue" : "soon"
            upcomingModel.append({
                                     agvName: item.agvName || item.agvId || "",
                                     agvId: item.agvId || "",
                                     taskName: item.taskName || "",
                                     taskCount: item.count || 0,
                                     severity: sev,
                                     dueDate: toRuDate(item.date || "")
                                 })
        }
    }

    onSelectedMonthChanged: reloadDashboardData()
    onSelectedYearChanged: reloadDashboardData()
    Component.onCompleted: reloadDashboardData()

    ListModel {
        id: upcomingModel
    }

    Column {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            width: parent.width
            height: 76
            color: "#111827"

            Row {
                anchors.fill: parent
                anchors.margins: 10

                Rectangle {
                    width: 370
                    height: parent.height
                    color: "#111827"

                    Image {
                        source: "qrc:/new/mainWindowIcons/noback/VAPManagerLogo.png"
                        fillMode: Image.PreserveAspectFit
                        width: 288
                        height: 62
                        anchors.centerIn: parent
                    }
                }

                Rectangle {
                    width: parent.width - 370
                    height: parent.height
                    color: "#0F172A"

                    Row {
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 8

                        Column {
                            width: parent.width - 470
                            spacing: 0
                            anchors.verticalCenter: parent.verticalCenter

                            Text {
                                text: "Календарь технического обслуживания"
                                font.pixelSize: 22
                                font.bold: true
                                font.family: "Inter"
                                color: "#F8FAFC"
                            }

                            Text {
                                text: "Отслеживание графиков обслуживания AGV\nи истории технического обслуживания"
                                font.pixelSize: 16
                                font.bold: true
                                font.family: "Inter"
                                color: "#94A3B8"
                            }
                        }

                        Rectangle {
                            width: 300
                            height: 56
                            radius: 12
                            color: "#1F2937"
                            anchors.verticalCenter: parent.verticalCenter

                            Row {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 8

                                Image {
                                    source: "qrc:/new/mainWindowIcons/noback/lupa.png"
                                    width: 31
                                    height: 30
                                    fillMode: Image.PreserveAspectFit
                                    anchors.verticalCenter: parent.verticalCenter
                                }

                                TextField {
                                    width: parent.width - 50
                                    height: parent.height
                                    placeholderText: "Поиск AGV..."
                                    placeholderTextColor: "#64748B"
                                    font.pixelSize: 16
                                    font.family: "Inter"
                                    color: "#E5E7EB"
                                    background: Rectangle {
                                        color: "transparent"
                                        border.width: 0
                                    }
                                    onTextChanged: {
                                        root.searchText = text
                                        root.performSearch(text)
                                    }
                                }
                            }
                        }

                        Button {
                            width: 50
                            height: 50
                            anchors.verticalCenter: parent.verticalCenter
                            background: Rectangle {
                                radius: 10
                                color: parent.hovered ? "#243041" : "#1F2937"
                                border.color: "#334155"
                                border.width: 1
                            }
                            contentItem: Image {
                                source: "qrc:/new/mainWindowIcons/noback/logs.png"
                                fillMode: Image.PreserveAspectFit
                                width: 24
                                height: 24
                            }
                            ToolTip.visible: hovered
                            ToolTip.text: "Чаты"
                            onClicked: root.showChats()
                        }

                        Button {
                            width: 58
                            height: 65
                            anchors.verticalCenter: parent.verticalCenter
                            background: Rectangle {
                                color: "transparent"
                                radius: 8
                                border.width: parent.pressed ? 2 : 0
                                border.color: "#3B82F6"
                            }

                            Image {
                                source: "qrc:/new/mainWindowIcons/noback/bell.png"
                                width: 37
                                height: 37
                                fillMode: Image.PreserveAspectFit
                                anchors.centerIn: parent
                            }

                            Rectangle {
                                visible: root.notifCount > 0
                                width: 20
                                height: 20
                                radius: 10
                                color: "#FF3B30"
                                anchors.top: parent.top
                                anchors.left: parent.left
                                anchors.topMargin: 2
                                anchors.leftMargin: 2

                                Text {
                                    anchors.centerIn: parent
                                    text: root.notifCount > 9 ? "9+" : root.notifCount
                                    font.pixelSize: 10
                                    font.bold: true
                                    font.family: "Inter"
                                    color: "white"
                                }
                            }

                            onClicked: root.showNotifications()
                        }

                        Rectangle {
                            width: 220
                            height: 56
                            radius: 16
                            color: "#1F2937"
                            border.color: "#334155"
                            border.width: 1
                            anchors.verticalCenter: parent.verticalCenter

                            Row {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 10

                                Button {
                                    id: userBtn
                                    width: 40
                                    height: 40
                                    anchors.verticalCenter: parent.verticalCenter
                                    background: Rectangle {
                                        radius: 20
                                        color: userBtn.hovered ? "#3B82F6" : "#2563EB"
                                    }

                                    contentItem: Text {
                                        text: {
                                            var name = root.currentUsername.trim()
                                            if (name.length >= 2)
                                                return name.substring(0, 2).toUpperCase()
                                            if (name.length === 1)
                                                return name.toUpperCase()
                                            return "US"
                                        }
                                        font.pixelSize: 13
                                        font.bold: true
                                        font.family: "Inter"
                                        color: "#FFFFFF"
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    ToolTip.visible: hovered
                                    ToolTip.text: root.currentUsername || "Пользователь"
                                    onClicked: userMenu.open()
                                }

                                Column {
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 2

                                    Text {
                                        text: root.currentUsername && root.currentUsername.length > 0 ? root.currentUsername : "Пользователь"
                                        font.pixelSize: 13
                                        font.bold: true
                                        font.family: "Inter"
                                        color: "#F8FAFC"
                                    }

                                    Text {
                                        text: root.currentUserRole === "admin" ? "Администратор"
                                              : root.currentUserRole === "technician" ? "Техник"
                                              : "Пользователь"
                                        font.pixelSize: 11
                                        font.family: "Inter"
                                        color: "#94A3B8"
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
                                    implicitWidth: 220
                                    color: "#111827"
                                    radius: 8
                                    border.color: "#334155"
                                    border.width: 1
                                }

                                MenuItem {
                                    text: "Аккаунт: " + (root.currentUsername || "неизвестно")
                                    onTriggered: root.openAccountPage()
                                }
                                MenuSeparator {}
                                MenuItem {
                                    text: "Редактировать профиль"
                                    onTriggered: root.openAccountPage()
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
            }
        }

        Row {
            width: parent.width
            height: parent.height - 76
            spacing: 5
            topPadding: 5

            Rectangle {
                width: 370
                height: parent.height
                color: "transparent"

                Column {
                    anchors.fill: parent
                    spacing: 8

                    Rectangle {
                        width: parent.width
                        height: childrenRect.height + 20
                        color: "#111827"
                        radius: 8

                        Column {
                            width: parent.width
                            anchors.margins: 10
                            spacing: 0

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
                                spacing: 8

                                SideNavButton {
                                    width: parent.width - 34
                                    text: "AGV"
                                    iconSource: "qrc:/new/mainWindowIcons/noback/agvIcon.png"
                                    selected: root.activePage === "agvList"
                                              || root.activePage === "agvDetails"
                                    onNavClicked: {
                                        root.activePage = "agvList"
                                        root.showAgvList()
                                    }
                                }

                                Rectangle {
                                    width: 26
                                    height: 26
                                    radius: 13
                                    color: "#00C8FF"
                                    anchors.verticalCenter: parent.verticalCenter

                                    Text {
                                        anchors.centerIn: parent
                                        text: root.activeAgvCount
                                        font.pixelSize: 14
                                        font.bold: true
                                        font.family: "Inter"
                                        color: "#0F00DB"
                                    }
                                }
                            }

                            SideNavButton {
                                text: "Модель AGV"
                                iconSource: "qrc:/new/mainWindowIcons/noback/edit.png"
                                selected: root.activePage === "modelList"
                                onNavClicked: {
                                    root.activePage = "modelList"
                                    root.showModelList()
                                }
                            }

                            SideNavButton {
                                text: "Сформировать годовой отчет"
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

                            Item {
                                width: 1
                                height: 10
                            }

                            Button {
                                width: parent.width - 56
                                height: 40
                                anchors.horizontalCenter: parent.horizontalCenter
                                visible: root.currentUserRole !== "viewer"
                                background: Rectangle {
                                    radius: 10
                                    color: parent.hovered ? "#1A4ACD" : "#0F00DB"
                                }
                                contentItem: Text {
                                    text: "+ Добавить AGV"
                                    font.pixelSize: 16
                                    font.bold: true
                                    font.family: "Inter"
                                    color: "white"
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                onClicked: root.addAgvRequested()
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: parent.height - y
                        color: "#111827"
                        radius: 8

                        Column {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 5

                            Text {
                                text: "Статус системы"
                                font.pixelSize: 14
                                font.bold: true
                                font.family: "Inter"
                                color: "#E5E7EB"
                            }

                            Grid {
                                width: parent.width
                                columns: 2
                                columnSpacing: 8
                                rowSpacing: 8

                                StatusCard {
                                    title: "Активные"
                                    currentCount: 0
                                    totalCount: 0
                                    accentColor: "#18CF00"
                                    width: (parent.width - 8) / 2
                                }
                                StatusCard {
                                    title: "Обслуживание"
                                    currentCount: 0
                                    totalCount: 0
                                    accentColor: "#FF8800"
                                    width: (parent.width - 8) / 2
                                }
                                StatusCard {
                                    title: "Ошибка"
                                    currentCount: 0
                                    totalCount: 0
                                    accentColor: "#FF0000"
                                    width: (parent.width - 8) / 2
                                }
                                StatusCard {
                                    title: "Отключены"
                                    currentCount: 0
                                    totalCount: 0
                                    accentColor: "#9CA3AF"
                                    width: (parent.width - 8) / 2
                                }
                            }

                            Button {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: 120
                                height: 25
                                background: Rectangle {
                                    color: "transparent"
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
                                        font.bold: true
                                        font.family: "Inter"
                                        color: "#E5E7EB"
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                                onClicked: {
                                    root.activePage = "logs"
                                    root.showLogs()
                                }
                            }

                            Text {
                                text: "Версия: 1.0.0 от 30.11.2025"
                                font.pixelSize: 10
                                font.family: "Inter"
                                color: "#64748B"
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                        }
                    }
                }
            }

            Rectangle {
                width: parent.width - 375
                height: parent.height
                color: "transparent"

                Column {
                    anchors.fill: parent
                    spacing: 8

                    Rectangle {
                        width: parent.width
                        height: parent.height * 0.58
                        color: "#111827"
                        radius: 12

                        Column {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 8

                            RowLayout {
                                width: parent.width
                                height: 38
                                spacing: 10

                                Text {
                                    text: Qt.locale().standaloneMonthName(
                                              selectedMonth - 1) + " " + selectedYear
                                    font.pixelSize: 26
                                    font.bold: true
                                    font.family: "Inter"
                                    color: "#F8FAFC"
                                    Layout.alignment: Qt.AlignVCenter
                                }

                                Item {
                                    Layout.fillWidth: true
                                    height: 1
                                }

                                Button {
                                    width: 32
                                    height: 32
                                    Layout.alignment: Qt.AlignVCenter
                                    background: Rectangle {
                                        radius: 6
                                        color: parent.hovered ? "#CFCFCF" : "#DFDFDF"
                                    }
                                    contentItem: Text {
                                        text: "<"
                                        font.pixelSize: 18
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    onClicked: changeMonth(-1)
                                }

                                Button {
                                    width: 32
                                    height: 32
                                    Layout.alignment: Qt.AlignVCenter
                                    background: Rectangle {
                                        radius: 6
                                        color: parent.hovered ? "#CFCFCF" : "#DFDFDF"
                                    }
                                    contentItem: Text {
                                        text: ">"
                                        font.pixelSize: 18
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    onClicked: changeMonth(1)
                                }

                                Button {
                                    width: 120
                                    height: 38
                                    Layout.alignment: Qt.AlignVCenter
                                    background: Rectangle {
                                        radius: 8
                                        color: parent.hovered ? "#CFCFCF" : "#DFDFDF"
                                    }
                                    contentItem: Text {
                                        text: "Настройки"
                                        font.pixelSize: 16
                                        font.bold: true
                                        font.family: "Inter"
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    onClicked: root.showSettings()
                                }
                            }

                            Row {
                                spacing: 15
                                Row {
                                    spacing: 4
                                    Rectangle {
                                        width: 12
                                        height: 12
                                        radius: 6
                                        color: "#FF0000"
                                    }
                                    Text {
                                        text: "Просрочен"
                                        font.pixelSize: 17
                                        font.family: "Inter"
                                        color: "#94A3B8"
                                    }
                                }
                                Row {
                                    spacing: 4
                                    Rectangle {
                                        width: 12
                                        height: 12
                                        radius: 6
                                        color: "#FF8800"
                                    }
                                    Text {
                                        text: "Ближайшие события"
                                        font.pixelSize: 17
                                        font.family: "Inter"
                                        color: "#94A3B8"
                                    }
                                }
                                Row {
                                    spacing: 4
                                    Rectangle {
                                        width: 12
                                        height: 12
                                        radius: 6
                                        color: "#18CF00"
                                    }
                                    Text {
                                        text: "Запланировано"
                                        font.pixelSize: 17
                                        font.family: "Inter"
                                        color: "#94A3B8"
                                    }
                                }
                                Row {
                                    spacing: 4
                                    Rectangle {
                                        width: 12
                                        height: 12
                                        radius: 6
                                        color: "#00E5FF"
                                    }
                                    Text {
                                        text: "Обслужено"
                                        font.pixelSize: 17
                                        font.family: "Inter"
                                        color: "#94A3B8"
                                    }
                                }
                            }

                            Rectangle {
                                width: parent.width
                                height: 2
                                color: "#243041"
                            }

                            CalendarGrid {
                                width: parent.width
                                height: parent.height - y
                                selectedMonth: root.selectedMonth
                                selectedYear: root.selectedYear
                                severityByDate: root.calendarSeverityByDate
                                darkMode: true
                                onDayClicked: function (date) {
                                    root.openCalendarPage()
                                }
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: parent.height * 0.38 - 8
                        color: "#111827"
                        radius: 12

                        Column {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 10

                            Text {
                                text: "Предстоящее обслуживание"
                                font.pixelSize: 20
                                font.bold: true
                                font.family: "Inter"
                                color: "#F8FAFC"
                            }

                            ListView {
                                width: parent.width
                                height: parent.height - 34
                                clip: true
                                spacing: 6

                                model: upcomingModel

                                delegate: Rectangle {
                                    width: ListView.view.width
                                    height: 48
                                    radius: 8
                                    color: "#0F172A"
                                    border.color: "#243041"
                                    border.width: 1

                                    Row {
                                        anchors.fill: parent
                                        anchors.margins: 10
                                        spacing: 10

                                        Rectangle {
                                            width: 12
                                            height: 12
                                            radius: 6
                                            color: {
                                                if (severity === "overdue")
                                                    return "#FF0000"
                                                if (severity === "soon")
                                                    return "#FF8800"
                                                if (severity === "planned")
                                                    return "#18CF00"
                                                if (severity === "completed")
                                                    return "#00E5FF"
                                                return "#9CA3AF"
                                            }
                                            anchors.verticalCenter: parent.verticalCenter
                                        }

                                        Text {
                                            text: agvName
                                            font.pixelSize: 14
                                            font.bold: true
                                            font.family: "Inter"
                                            color: "#F8FAFC"
                                            anchors.verticalCenter: parent.verticalCenter
                                        }

                                        Text {
                                            text: taskCount + " задач(и)"
                                            font.pixelSize: 13
                                            font.family: "Inter"
                                            color: "#94A3B8"
                                            anchors.verticalCenter: parent.verticalCenter
                                        }

                                        Item {
                                            width: 16
                                            height: 1
                                        }

                                        Text {
                                            text: dueDate
                                            font.pixelSize: 13
                                            font.family: "Inter"
                                            color: "#64748B"
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: root.showAgvDetails(agvName)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
