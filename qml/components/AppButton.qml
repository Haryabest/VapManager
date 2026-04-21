import QtQuick 2.12
import QtQuick.Controls 2.12
import "../style"

Button {
    id: root
    width: 300
    height: 45

    property string buttonStyle: "primary"

    background: Rectangle {
        color: {
            if (!root.enabled) {
                return "#E5E7EB"
            }
            
            switch (root.buttonStyle) {
            case "secondary":
                return root.pressed || root.hovered
                        ? "#E5E7EB"
                        : "#FFFFFF"
            case "ghost":
                return "transparent"
            case "primary":
            default:
                return root.pressed || root.hovered
                        ? Theme.primaryHover
                        : Theme.primary
            }
        }
        radius: 10
        border.color: root.buttonStyle === "secondary" ? Theme.border : "transparent"
        border.width: root.buttonStyle === "secondary" ? 1 : 0

        Behavior on color {
            ColorAnimation { duration: 150 }
        }
    }

    contentItem: Text {
        text: root.text
        color: {
            if (!root.enabled) {
                return "#9CA3AF"
            }
            switch (root.buttonStyle) {
            case "secondary":
                return Theme.primary
            case "ghost":
                return Theme.textSecondary
            case "primary":
            default:
                return "#FFFFFF"
            }
        }
        font.pixelSize: 16
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}