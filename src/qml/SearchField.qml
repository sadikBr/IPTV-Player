import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import IPTVPlayer

// Pill-shaped search box with an icon and a clear button.
TextField {
    id: control

    implicitHeight: 40
    leftPadding: 42
    rightPadding: text.length > 0 ? 40 : 16
    topPadding: 0
    bottomPadding: 0
    verticalAlignment: TextInput.AlignVCenter
    font.pixelSize: AppTheme.fontBody
    color: AppTheme.textPrimary
    placeholderTextColor: AppTheme.textTertiary
    selectByMouse: true
    selectionColor: AppTheme.accentStrong

    background: Rectangle {
        radius: height / 2
        color: control.activeFocus ? AppTheme.surfaceHigh : AppTheme.surface
        border.width: 1
        border.color: control.activeFocus ? AppTheme.accent : AppTheme.stroke
        Behavior on border.color { ColorAnimation { duration: AppTheme.fast } }

        IconImage {
            x: 14
            anchors.verticalCenter: parent.verticalCenter
            source: "icons/search.svg"
            sourceSize: Qt.size(18, 18)
            color: AppTheme.textTertiary
        }
    }

    IconButton {
        anchors.right: parent.right
        anchors.rightMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        size: 32
        visible: control.text.length > 0
        iconName: "close"
        iconColor: AppTheme.textSecondary
        tip: qsTr("Clear")
        onClicked: control.clear()
    }
}
