import QtQuick
import QtQuick.Controls
import IPTVPlayer

// On-screen message that appears for a moment (channel name, volume, ...).
Rectangle {
    id: root

    property alias text: label.text
    property alias font: label.font
    // Set to keep it visible; or call show() for a timed message.
    property bool shown: false

    function show() {
        timer.restart()
    }

    implicitWidth: label.implicitWidth + 32
    implicitHeight: label.implicitHeight + 16
    radius: AppTheme.radius
    color: AppTheme.overlay
    opacity: shown || timer.running ? 1 : 0
    visible: opacity > 0 && label.text.length > 0
    Behavior on opacity { NumberAnimation { duration: 150 } }

    Timer {
        id: timer
        interval: 2000
    }

    Label {
        id: label
        anchors.centerIn: parent
        color: "white"
        font.pixelSize: 20
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
    }
}
