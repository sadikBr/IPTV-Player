import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import IPTVPlayer

// Asks for the parental PIN: digit keys or the on-screen keypad (phones and
// TV remotes). Emits entered(pin); the caller checks it and may set error.
AppDialog {
    id: root

    property string message
    property string error
    property string pin
    // Enter when this many digits are typed (4 for an existing PIN).
    property int length: 4

    signal entered(string pin)

    function ask(title, message) {
        root.title = title
        root.message = message
        root.error = ""
        root.pin = ""
        open()
    }

    function addDigit(d) {
        if (pin.length >= 8)
            return
        error = ""
        pin += d
        if (pin.length === length)
            submit()
    }
    function submit() {
        if (pin.length < 4) {
            error = qsTr("The PIN has at least 4 digits.")
            return
        }
        const value = pin
        pin = ""
        entered(value)
    }

    width: Math.min(parent ? parent.width - 32 : 360, 360)
    standardButtons: Dialog.Cancel
    onOpened: keypad.forceActiveFocus()

    contentItem: ColumnLayout {
        spacing: 16

        Label {
            Layout.fillWidth: true
            visible: text.length > 0
            wrapMode: Text.Wrap
            text: root.message
            color: AppTheme.textSecondary
        }

        // Dots
        Row {
            Layout.alignment: Qt.AlignHCenter
            spacing: 14
            Repeater {
                model: Math.max(root.length, root.pin.length)
                delegate: Rectangle {
                    required property int index
                    width: 14
                    height: 14
                    radius: 7
                    color: index < root.pin.length ? AppTheme.accent : "transparent"
                    border.width: 2
                    border.color: index < root.pin.length ? AppTheme.accent : AppTheme.strokeStrong
                }
            }
        }

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            visible: root.error.length > 0
            text: root.error
            color: AppTheme.error
        }

        // Keypad
        GridLayout {
            id: keypad
            Layout.alignment: Qt.AlignHCenter
            columns: 3
            rowSpacing: 8
            columnSpacing: 8
            focus: true
            Keys.onPressed: (event) => {
                if (event.key >= Qt.Key_0 && event.key <= Qt.Key_9) {
                    root.addDigit(String(event.key - Qt.Key_0))
                    event.accepted = true
                } else if (event.key === Qt.Key_Backspace) {
                    root.pin = root.pin.slice(0, -1)
                    event.accepted = true
                }
            }

            Repeater {
                model: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "back", "0", "ok"]
                delegate: AbstractButton {
                    id: key
                    required property string modelData
                    implicitWidth: 64
                    implicitHeight: 52
                    focusPolicy: Qt.TabFocus
                    hoverEnabled: true
                    enabled: modelData !== "ok" || root.pin.length >= 4
                    opacity: enabled ? 1 : 0.4
                    onClicked: {
                        if (modelData === "back")
                            root.pin = root.pin.slice(0, -1)
                        else if (modelData === "ok")
                            root.submit()
                        else
                            root.addDigit(modelData)
                    }
                    background: Rectangle {
                        radius: AppTheme.radius
                        color: key.pressed ? AppTheme.surfaceHover
                             : key.hovered ? AppTheme.surfaceHigh : AppTheme.backgroundRaised
                        border.width: 1
                        border.color: AppTheme.stroke
                        FocusRing { target: key }
                    }
                    contentItem: Item {
                        Label {
                            anchors.centerIn: parent
                            visible: key.modelData.length === 1
                            text: key.modelData
                            font.pixelSize: AppTheme.fontH2
                            font.weight: Font.DemiBold
                            color: AppTheme.textPrimary
                        }
                        IconImage {
                            anchors.centerIn: parent
                            visible: key.modelData.length > 1
                            source: key.modelData === "back" ? "icons/back.svg" : "icons/check.svg"
                            sourceSize: Qt.size(20, 20)
                            color: AppTheme.textPrimary
                        }
                    }
                }
            }
        }
    }
}
