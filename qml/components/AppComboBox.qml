import QtQuick 2.12
import QtQuick.Controls 2.12

import "../style"

ComboBox {
    id: root
    width: 300
    height: 42
    padding: 12
    function themeColor(name, fallback) {
        if (typeof Theme === "undefined" || Theme === null)
            return fallback
        var value = Theme[name]
        return (value === undefined || value === null) ? fallback : value
    }

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
            function onPressedChanged() { canvas.requestPaint() }
        }

        onPaint: {
            context.reset();
            context.moveTo(0, 0);
            context.lineTo(width, 0);
            context.lineTo(width / 2, height);
            context.closePath();
            context.fillStyle = root.themeColor("textSecondary", "#A0A0B0");
            context.fill();
        }
    }

    background: Rectangle {
        color: root.pressed ? "#252540" : "#1a1a2e"
        radius: 10
        border.color: root.activeFocus || root.pressed ? root.themeColor("primary", "#6C63FF") : "#333355"
        border.width: 1

        Behavior on color {
            ColorAnimation { duration: 150 }
        }
    }

    contentItem: Text {
        leftPadding: 12
        rightPadding: root.indicator.width + 12
        text: root.displayText
        font: root.font
        color: root.themeColor("text", "#FFFFFF")
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    delegate: ItemDelegate {
        width: root.width
        highlighted: root.highlightedIndex === index

        contentItem: Text {
            text: modelData
            font: root.font
            color: root.highlightedIndex === index ? root.themeColor("primary", "#6C63FF") : root.themeColor("text", "#FFFFFF")
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            color: root.highlightedIndex === index ? Qt.lighter(root.themeColor("primary", "#6C63FF"), 1.2) : "transparent"
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

            ScrollIndicator.vertical: ScrollIndicator { }
        }

        background: Rectangle {
            color: root.themeColor("bgSecondary", "#1a1a2e")
            radius: 10
            border.color: root.themeColor("primary", "#6C63FF")
            border.width: 1
        }
    }
}
