import QtQuick 2.12
import QtQuick.Controls 2.12

import "../style"

TextField {
    id: root
    width: 300
    height: 45
    padding: 12

    font.pixelSize: 14
    function themeColor(name, fallback) {
        if (typeof Theme === "undefined" || Theme === null)
            return fallback
        var value = Theme[name]
        return (value === undefined || value === null) ? fallback : value
    }
    placeholderTextColor: "#888888"
    color: root.themeColor("text", "#FFFFFF")
    selectionColor: root.themeColor("primary", "#6C63FF")
    selectedTextColor: root.themeColor("text", "#FFFFFF")

    background: Rectangle {
        color: root.activeFocus ? "#252540" : "#1a1a2e"
        radius: 10
        border.color: root.activeFocus ? root.themeColor("primary", "#6C63FF") : "#333355"
        border.width: 1

        Behavior on color {
            ColorAnimation { duration: 150 }
        }
    }
}
