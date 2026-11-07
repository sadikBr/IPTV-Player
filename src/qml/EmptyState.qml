import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import IPTVPlayer

// Friendly message for empty lists, missing providers, errors, ...
ColumnLayout {
    id: root

    property string iconName: "tv"
    property string title
    property string text
    property string actionText
    property bool busy: false

    signal actionClicked()

    spacing: AppTheme.spacing
    width: Math.min(parent ? parent.width - 2 * AppTheme.margin : 360, 420)

    Rectangle {
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: 72
        Layout.preferredHeight: 72
        radius: 36
        color: AppTheme.surfaceHigh
        border.color: AppTheme.stroke

        IconImage {
            anchors.centerIn: parent
            visible: !root.busy
            source: "icons/" + root.iconName + ".svg"
            sourceSize: Qt.size(32, 32)
            color: AppTheme.accent
        }
        BusyIndicator {
            anchors.centerIn: parent
            running: root.busy
            visible: running
        }
    }
    Label {
        Layout.fillWidth: true
        Layout.topMargin: 4
        visible: text.length > 0
        text: root.title
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        font.pixelSize: AppTheme.fontH2
        font.weight: Font.DemiBold
        color: AppTheme.textPrimary
    }
    Label {
        Layout.fillWidth: true
        visible: text.length > 0
        text: root.text
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        color: AppTheme.textSecondary
        lineHeight: 1.15
    }
    AppButton {
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: AppTheme.spacing
        visible: root.actionText.length > 0
        text: root.actionText
        onClicked: root.actionClicked()
    }
}
