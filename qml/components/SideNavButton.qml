import QtQuick 2.12
import QtQuick.Controls 2.12

AbstractButton {
    id: root
    width: parent ? parent.width : 256
    height: 45

    property string iconSource: ""
    property bool selected: false

    signal navClicked()

    background: Rectangle {
        color: {
            if (root.pressed) return "#78ADD8E6"
            if (root.hovered || root.selected) return "#4CADD8E6"
            return "transparent"
        }
        radius: 8
        border.color: (root.hovered || root.selected) ? "#ADD8E6" : "transparent"
        border.width: 1

        Behavior on color { ColorAnimation { duration: 120 } }
        Behavior on border.color { ColorAnimation { duration: 120 } }
    }

    contentItem: Row {
        spacing: 12
        leftPadding: 15

        Image {
            source: root.iconSource
            width: 24
            height: 24
            fillMode: Image.PreserveAspectFit
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            text: root.text
            font.pixelSize: 14
            font.bold: true
            font.family: "Inter"
            color: root.selected ? "#0F00DB" : "#1E293B"
            anchors.verticalCenter: parent.verticalCenter

            Behavior on color { ColorAnimation { duration: 120 } }
        }
    }

    onClicked: root.navClicked()
}
