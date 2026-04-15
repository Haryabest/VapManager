import QtQuick 2.12
import QtQuick.Controls 2.12

ApplicationWindow {
    id: root
    visible: true
    width: 1280
    height: 800
    minimumWidth: 1080
    minimumHeight: 680
    title: "VAP Manager"

    Rectangle {
        anchors.fill: parent
        color: "#F4F6FA"

        Column {
            anchors.fill: parent
            spacing: 0

            Rectangle {
                width: parent.width
                height: 76
                color: "#FFFFFF"
                border.color: "#E2E8F0"
                border.width: 1

                Row {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12

                    Text {
                        text: "VAP Manager"
                        font.pixelSize: 24
                        font.bold: true
                        color: "#1E293B"
                        verticalAlignment: Text.AlignVCenter
                    }

                    Item { width: 1; height: 1 }

                    TextField {
                        width: 320
                        height: 40
                        placeholderText: "Поиск..."
                    }
                }
            }

            Row {
                width: parent.width
                height: parent.height - 76
                spacing: 0

                Rectangle {
                    width: 280
                    height: parent.height
                    color: "#FFFFFF"
                    border.color: "#E2E8F0"
                    border.width: 1

                    Column {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 8

                        Repeater {
                            model: ["AGV", "Модель AGV", "Пользователи", "Чаты", "Логи", "Настройки"]
                            delegate: Button {
                                width: 256
                                height: 38
                                text: modelData
                            }
                        }
                    }
                }

                Rectangle {
                    width: parent.width - 280
                    height: parent.height
                    color: "#F8FAFC"

                    Text {
                        anchors.centerIn: parent
                        text: "QML-каркас главной страницы.\nДальше переносим модули по блокам."
                        horizontalAlignment: Text.AlignHCenter
                        color: "#64748B"
                        font.pixelSize: 16
                    }
                }
            }
        }
    }
}
