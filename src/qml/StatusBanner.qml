import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import IPTVPlayer

// Slim notice with optional progress: updates in progress, errors.
Rectangle {
    id: root

    property string text
    property real progress: -1 // < 0: indeterminate; ignored for errors
    property bool error: false
    property bool canCancel: false

    signal cancelClicked()
    signal dismissClicked()

    implicitHeight: layout.implicitHeight + 20
    radius: AppTheme.radius
    color: error ? "#2AFF6B7A" : AppTheme.surface
    border.color: error ? "#55FF6B7A" : AppTheme.stroke

    RowLayout {
        id: layout
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 6
        anchors.topMargin: 10
        anchors.bottomMargin: 10
        spacing: AppTheme.spacing

        IconImage {
            visible: root.error
            source: "icons/info.svg"
            sourceSize: Qt.size(18, 18)
            color: AppTheme.error
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6
            Label {
                Layout.fillWidth: true
                text: root.text
                wrapMode: Text.Wrap
                maximumLineCount: 3
                elide: Text.ElideRight
                color: AppTheme.textPrimary
                font.pixelSize: AppTheme.fontSmall
            }
            ProgressBar {
                Layout.fillWidth: true
                visible: !root.error
                indeterminate: root.progress < 0
                value: Math.max(0, root.progress)
            }
        }

        AppButton {
            visible: root.canCancel
            variant: "ghost"
            text: qsTr("Cancel")
            implicitHeight: 34
            onClicked: root.cancelClicked()
        }
        IconButton {
            visible: root.error
            iconName: "close"
            tip: qsTr("Dismiss")
            size: 34
            onClicked: root.dismissClicked()
        }
    }
}
