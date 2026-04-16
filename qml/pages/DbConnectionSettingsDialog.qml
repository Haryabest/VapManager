import QtQuick 2.12
import QtQuick.Controls 2.12

ApplicationWindow {
    id: root
    visible: true
    width: 460
    height: 320
    minimumWidth: 430
    maximumWidth: 560
    minimumHeight: 300
    maximumHeight: 420
    title: "Настройки подключения к базе данных"
    flags: Qt.Dialog | Qt.WindowTitleHint | Qt.WindowSystemMenuHint

    property bool applyRequested: false
    property string selectedHost: typeof dbSettingsHost !== "undefined" ? dbSettingsHost : "localhost"
    property string errorText: typeof dbSettingsError !== "undefined" ? dbSettingsError : ""
    property bool connecting: false

    function startConnect() {
        if (connecting)
            return
        if (typeof dbConnBridge !== "undefined" && dbConnBridge !== null) {
            dbConnBridge.tryConnect(hostInput.text)
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#0F0F1A"

        Column {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            Text {
                text: "Подключение к базе данных"
                font.pixelSize: 22
                font.bold: true
                color: "#FFFFFF"
            }

            Text {
                text: "Введите IP-адрес или хост сервера MySQL"
                font.pixelSize: 12
                color: "#A0A0B0"
                wrapMode: Text.WordWrap
                width: parent.width
            }

            TextField {
                id: hostInput
                width: parent.width
                height: 42
                padding: 12
                placeholderText: "localhost"
                placeholderTextColor: "#888888"
                text: root.selectedHost
                color: "#FFFFFF"
                selectionColor: "#6C63FF"
                selectedTextColor: "#FFFFFF"
                font.pixelSize: 14
                background: Rectangle {
                    color: hostInput.activeFocus ? "#252540" : "#1a1a2e"
                    radius: 10
                    border.color: hostInput.activeFocus ? "#6C63FF" : "#333355"
                    border.width: 1
                }
            }

            Rectangle {
                width: parent.width
                visible: root.errorText.trim().length > 0
                color: "#3B1620"
                border.color: "#FF5252"
                border.width: 1
                radius: 10
                implicitHeight: errText.implicitHeight + 16

                Text {
                    id: errText
                    anchors.fill: parent
                    anchors.margins: 8
                    text: root.errorText
                    wrapMode: Text.WordWrap
                    color: "#FFFFFF"
                    font.pixelSize: 12
                }
            }

            Item { width: 1; height: 2 }

            Row {
                width: parent.width
                spacing: 10

                Item { width: 1; height: 1 }

                Button {
                    width: 120
                    height: 40
                    text: "Отмена"
                    enabled: !root.connecting
                    onClicked: root.close()
                }

                Button {
                    id: okButton
                    width: 120
                    height: 40
                    text: root.connecting ? "Подключение..." : "OK"
                    enabled: !root.connecting
                    onClicked: root.startConnect()

                    background: Rectangle {
                        implicitWidth: 120
                        implicitHeight: 40
                        radius: 10
                        color: okButton.enabled ? "#6C63FF" : "#4A4A5E"
                        border.width: 0
                    }

                    contentItem: Text {
                        text: okButton.text
                        color: "#FFFFFF"
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }

    Shortcut {
        sequence: "Return"
        enabled: root.visible && !root.connecting
        onActivated: root.startConnect()
    }
    Shortcut {
        sequence: "Enter"
        enabled: root.visible && !root.connecting
        onActivated: root.startConnect()
    }
    Shortcut {
        sequence: "Escape"
        enabled: root.visible && !root.connecting
        onActivated: root.close()
    }

    Connections {
        target: (typeof dbConnBridge !== "undefined") ? dbConnBridge : null
        function onConnectStarted() {
            root.connecting = true
            root.errorText = "Пробуем подключиться..."
        }
        function onConnectFinished(ok, host, errorText) {
            root.connecting = false
            if (ok) {
                root.selectedHost = host
                root.applyRequested = true
                root.close()
            } else {
                root.errorText = errorText
            }
        }
    }

    onClosing: function(closeEvent) {
        if (root.connecting) {
            closeEvent.accepted = false
            return
        }
        if (!root.applyRequested && root.errorText === "Пробуем подключиться...") {
            root.errorText = typeof dbSettingsError !== "undefined" ? dbSettingsError : ""
        }
    }
}
