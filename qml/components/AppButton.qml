import QtQuick 2.12
import QtQuick.Controls 2.12
import "../style"

Button {
    id: root
    width: 300
    height: 45

    // Свойство для выбора стиля кнопки
    property string buttonStyle: "primary" // "primary", "secondary", "ghost"
    function themeColor(name, fallback) {
        if (typeof Theme === "undefined" || Theme === null)
            return fallback
        var value = Theme[name]
        return (value === undefined || value === null) ? fallback : value
    }

    background: Rectangle {
        color: {
            if (!root.enabled) {
                return "#555555"
            }
            
            switch (root.buttonStyle) {
            case "secondary":
                return root.pressed || root.hovered
                        ? Qt.darker(root.themeColor("bgSecondary", "#1a1a2e"), 1.1)
                        : root.themeColor("bgSecondary", "#1a1a2e")
            case "ghost":
                return "transparent"
            case "primary":
            default:
                return root.pressed || root.hovered
                        ? root.themeColor("primaryHover", "#7B73FF")
                        : root.themeColor("primary", "#6C63FF")
            }
        }
        radius: 10
        border.color: root.buttonStyle === "secondary" ? root.themeColor("primary", "#6C63FF") : "transparent"
        border.width: root.buttonStyle === "secondary" ? 1 : 0

        Behavior on color {
            ColorAnimation { duration: 150 }
        }

        Rectangle {
            anchors.fill: parent
            radius: 10
            color: "white"
            opacity: (root.hovered && root.buttonStyle === "primary") ? 0.1 : 0
            Behavior on opacity {
                NumberAnimation { duration: 150 }
            }
        }
    }

    contentItem: Text {
        text: root.text
        color: {
            if (!root.enabled) {
                return "#888888"
            }
            switch (root.buttonStyle) {
            case "secondary":
                return root.themeColor("primary", "#6C63FF")
            case "ghost":
                return root.themeColor("textSecondary", "#A0A0B0")
            case "primary":
            default:
                return "white"
            }
        }
        font.pixelSize: 16
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
