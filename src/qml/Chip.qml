import QtQuick
import QtQuick.Controls
import IPTVPlayer

// Rounded filter chip (categories on phones, seasons, ...).
AbstractButton {
    id: control

    property bool selected: false
    property int count: -1 // shown after the text when >= 0

    implicitHeight: 34
    implicitWidth: row.implicitWidth + 28
    hoverEnabled: true
    focusPolicy: Qt.TabFocus

    background: Rectangle {
        radius: height / 2
        color: control.selected ? AppTheme.accentStrong
             : control.hovered ? AppTheme.surfaceHover : AppTheme.surfaceHigh
        border.width: control.selected ? 0 : 1
        border.color: AppTheme.stroke
        Behavior on color { ColorAnimation { duration: AppTheme.fast } }
        FocusRing { target: control }
    }

    contentItem: Item {
        Row {
            id: row
            anchors.centerIn: parent
            spacing: 6
            Label {
                text: control.text
                color: control.selected ? "white" : AppTheme.textPrimary
                font.pixelSize: AppTheme.fontSmall
                font.weight: Font.Medium
            }
            Label {
                visible: control.count >= 0
                text: control.count
                color: control.selected ? "#CCFFFFFF" : AppTheme.textTertiary
                font.pixelSize: AppTheme.fontCaption
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }
}
