import QtQuick 2.12
import QtQuick.Controls 2.12
import "../style"

TextField {
    id: root
    width: 300
    height: 45
    padding: 12

    font.pixelSize: 14
    placeholderTextColor: Theme.textMuted
    color: Theme.text
    selectionColor: Theme.primary
    selectedTextColor: Theme.text

    background: Rectangle {
        color: "#FFFFFF"
        radius: 10
        border.color: root.activeFocus ? Theme.primary : Theme.border
        border.width: 1

        Behavior on border.color {
            ColorAnimation { duration: 150 }
        }
    }
}