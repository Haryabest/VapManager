import QtQuick 2.12
import QtQuick.Controls 2.12

Item {
    id: root
    width: parent ? parent.width : 600
    height: parent ? parent.height : 400

    property int selectedMonth: new Date().getMonth() + 1
    property int selectedYear: new Date().getFullYear()
    property var severityByDate: ({})
    property date selectedDate: new Date(selectedYear, selectedMonth - 1, new Date().getDate())
    property bool darkMode: true

    signal dayClicked(date date)

    readonly property var weekDays: ["Пн", "Вт", "Ср", "Чт", "Пт", "Сб", "Вс"]
    readonly property date firstDay: new Date(selectedYear, selectedMonth - 1, 1)
    readonly property int daysInMonth: new Date(selectedYear, selectedMonth, 0).getDate()
    readonly property int startCol: {
        var d = firstDay.getDay()
        return d === 0 ? 6 : d - 1
    }

    readonly property date prevMonth: new Date(selectedYear, selectedMonth - 2, 1)
    readonly property int prevDaysInMonth: new Date(selectedYear, selectedMonth - 1, 0).getDate()
    readonly property int leadingCells: startCol
    readonly property int visibleCells: leadingCells + daysInMonth
    readonly property int calendarRows: Math.ceil(visibleCells / 7)
    readonly property int totalRows: calendarRows + 1
    readonly property real cellHeight: (height - headerHeight) / calendarRows
    readonly property real headerHeight: 42

    function dateKey(dateObj) {
        var y = dateObj.getFullYear()
        var m = ("0" + (dateObj.getMonth() + 1)).slice(-2)
        var d = ("0" + dateObj.getDate()).slice(-2)
        return y + "-" + m + "-" + d
    }

    function severityColor(severity) {
        if (severity === "overdue")
            return "#FF4D4F"
        if (severity === "soon")
            return "#FF9F1A"
        if (severity === "planned")
            return "#22C55E"
        if (severity === "completed")
            return "#38BDF8"
        return "transparent"
    }

    Rectangle {
        anchors.fill: parent
        radius: 12
        color: darkMode ? "#101828" : "#FFFFFF"
        border.color: darkMode ? "#243041" : "#D8DEE9"
        border.width: 1

        Grid {
            anchors.fill: parent
            columns: 7
            rows: root.totalRows
            spacing: 0

            Repeater {
                model: root.totalRows * 7

                Rectangle {
                    width: root.width / 7
                    height: index < 7 ? root.headerHeight : root.cellHeight
                    color: {
                        if (index < 7)
                            return root.darkMode ? "#1E293B" : "#555555"
                        if (mouseArea.containsMouse && isCurrentMonth)
                            return root.darkMode ? "#162033" : "#F4F7FB"
                        if (isSelected)
                            return root.darkMode ? "#18243A" : "#EEF4FF"
                        return "transparent"
                    }

                    readonly property int row: Math.floor(index / 7)
                    readonly property int col: index % 7
                    readonly property bool isHeader: row === 0

                    readonly property var cellDate: {
                        if (isHeader)
                            return null

                        var cellIndex = (row - 1) * 7 + col
                        var currentMonthDay = cellIndex - root.leadingCells + 1

                        if (currentMonthDay < 1) {
                            var d = root.prevDaysInMonth + currentMonthDay
                            return new Date(root.prevMonth.getFullYear(), root.prevMonth.getMonth(), d)
                        } else if (currentMonthDay > root.daysInMonth) {
                            var d2 = currentMonthDay - root.daysInMonth
                            var nextM = new Date(root.selectedYear, root.selectedMonth, 1)
                            return new Date(nextM.getFullYear(), nextM.getMonth(), d2)
                        } else {
                            return new Date(root.selectedYear, root.selectedMonth - 1, currentMonthDay)
                        }
                    }

                    readonly property bool isCurrentMonth: cellDate !== null
                                                           && cellDate.getMonth() === root.selectedMonth - 1
                                                           && cellDate.getFullYear() === root.selectedYear

                    readonly property bool isToday: {
                        if (!cellDate)
                            return false
                        var now = new Date()
                        return cellDate.getFullYear() === now.getFullYear()
                                && cellDate.getMonth() === now.getMonth()
                                && cellDate.getDate() === now.getDate()
                    }

                    readonly property bool isSelected: {
                        if (!cellDate || !root.selectedDate)
                            return false
                        return cellDate.getFullYear() === root.selectedDate.getFullYear()
                                && cellDate.getMonth() === root.selectedDate.getMonth()
                                && cellDate.getDate() === root.selectedDate.getDate()
                    }

                    readonly property string severity: cellDate ? (root.severityByDate[root.dateKey(cellDate)] || "") : ""

                    Rectangle {
                        visible: !isHeader && col < 6
                        width: 1
                        height: parent.height
                        anchors.right: parent.right
                        color: root.darkMode ? "#243041" : "#D3D3D3"
                    }

                    Rectangle {
                        visible: !isHeader && row < root.totalRows - 1
                        width: parent.width
                        height: 1
                        anchors.bottom: parent.bottom
                        color: root.darkMode ? "#243041" : "#D3D3D3"
                    }

                    Item {
                        anchors.fill: parent
                        visible: !isHeader

                        Text {
                            text: cellDate ? cellDate.getDate() : ""
                            font.pixelSize: 14
                            font.bold: true
                            font.family: "Inter"
                            color: {
                                if (!isCurrentMonth)
                                    return root.darkMode ? "#667085" : "#A5A5A5"
                                if (isSelected)
                                    return "#FFFFFF"
                                return root.darkMode ? "#E5E7EB" : "#000000"
                            }
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.leftMargin: 8
                            anchors.topMargin: 6
                        }

                        Rectangle {
                            visible: isSelected
                            width: 28
                            height: 28
                            radius: 14
                            color: "#2563EB"
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.leftMargin: 4
                            anchors.topMargin: 2
                            z: -1
                        }

                        Rectangle {
                            visible: isToday && !isSelected
                            width: 24
                            height: 24
                            radius: 12
                            color: "transparent"
                            border.color: "#3B82F6"
                            border.width: 2
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.leftMargin: 6
                            anchors.topMargin: 4
                        }

                        Rectangle {
                            visible: severity !== ""
                            width: 10
                            height: 10
                            radius: 5
                            color: root.severityColor(severity)
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.rightMargin: 8
                            anchors.topMargin: 8
                        }

                        Rectangle {
                            visible: severity !== ""
                            width: parent.width - 14
                            height: 4
                            radius: 2
                            color: Qt.rgba(Qt.colorEqual(root.severityColor(severity), "transparent") ? 0 : 1, 0, 0, 0)
                            opacity: 0
                        }
                    }

                    Text {
                        visible: isHeader
                        text: root.weekDays[col]
                        font.pixelSize: 15
                        font.bold: true
                        font.family: "Inter"
                        color: root.darkMode ? "#E5E7EB" : "#222222"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        anchors.fill: parent
                    }

                    MouseArea {
                        id: mouseArea
                        anchors.fill: parent
                        visible: !isHeader
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (cellDate) {
                                root.selectedDate = cellDate
                                root.dayClicked(cellDate)
                            }
                        }
                    }
                }
            }
        }
    }
}
