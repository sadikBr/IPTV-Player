import QtQuick
import QtQuick.Controls.impl
import IPTVPlayer

// Search box: rounded, with a magnifier icon and a clear button.
AppTextField {
    id: control

    implicitHeight: 40
    iconSpace: 26
    rightPadding: text.length > 0 ? 40 : 16

    background: Rectangle {
        radius: height / 2
        color: control.activeFocus ? AppTheme.surfaceHigh : AppTheme.surface
        border.width: 1
        border.color: control.activeFocus ? AppTheme.strokeStrong
                    : control.hovered ? AppTheme.strokeStrong : AppTheme.stroke
        Behavior on color { ColorAnimation { duration: AppTheme.fast } }

        IconImage {
            x: 14
            anchors.verticalCenter: parent.verticalCenter
            source: "icons/search.svg"
            sourceSize: Qt.size(18, 18)
            color: control.activeFocus ? AppTheme.textPrimary : AppTheme.textTertiary
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
