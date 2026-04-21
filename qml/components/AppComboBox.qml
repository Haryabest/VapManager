import QtQuick 2.12
import QtQuick.Controls 2.12
import "../style"

ComboBox {
    id: root
    width: 300
    height: 42
    padding: 12

    font.pixelSize: 14
    indicator: Canvas {
        id: canvas
        x: parent.width - width - 12
        y: (parent.height - height) / 2
        width: 12
        height: 8
        contextType: "2d"

        Connections {
            target: root
            function onPressedChanged() {
                canvas.requestPaint()
            }
        }

        onPaint: {
            context.reset()
            context.moveTo(0, 0)
            context.lineTo(width, 0)
            context.lineTo(width / 2, height)
            context.closePath()
            context.fillStyle = Theme.textSecondary
            context.fill()
        }
    }

    background: Rectangle {
        color: "#FFFFFF"
        radius: 10
        border.color: root.activeFocus || root.pressed ? Theme.primary : Theme.border
        border.width: 1

        Behavior on color {
            ColorAnimation {
                duration: 150
            }
        }
    }

    contentItem: Text {
        leftPadding: 12
        rightPadding: root.indicator.width + 12
        text: root.displayText
        font: root.font
        color: Theme.text
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    delegate: ItemDelegate {
        width: root.width
        highlighted: root.highlightedIndex === index

        contentItem: Text {
            text: modelData
            font: root.font
            color: root.highlightedIndex === index ? Theme.primary : Theme.text
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            color: root.highlightedIndex === index ? Theme.navHover : "transparent"
            radius: 6
        }
    }

    popup: Popup {
        y: root.height
        width: root.width
        implicitHeight: contentItem.implicitHeight
        padding: 4

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: root.popup.visible ? root.delegateModel : null
            currentIndex: root.highlightedIndex

            ScrollIndicator.vertical: ScrollIndicator {}
        }

        background: Rectangle {
            color: "#FFFFFF"
            radius: 10
            border.color: Theme.primary
            border.width: 1
        }
    }
}