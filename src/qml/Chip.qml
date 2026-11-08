import QtQuick
import QtQuick.Controls
import IPTVPlayer

// Rounded filter chip (categories on phones, seasons, ...). The selected one
// glows in the accent gradient; the others carry a faint colour of their own.
AbstractButton {
    id: control

    property bool selected: false
    property int count: -1 // shown after the text when >= 0
    property string emoji  // optional small picture before the text
    property bool tinted: true

    implicitHeight: 36
    implicitWidth: row.implicitWidth + 28
    hoverEnabled: true
    focusPolicy: Qt.TabFocus

    scale: pressed ? 0.92 : 1
    Behavior on scale { NumberAnimation { duration: 260; easing.type: Easing.OutBack; easing.overshoot: 3 } }

    background: Rectangle {
        radius: height / 2
        color: control.hovered ? AppTheme.surfaceHover : AppTheme.surfaceHigh
        border.width: control.selected ? 0 : 1
        border.color: control.tinted ? AppTheme.vivid(control.text, 0.28) : AppTheme.stroke

        // Faint own colour
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            visible: control.tinted && !control.selected
            color: AppTheme.vivid(control.text, 0.10)
        }
        // Accent gradient when selected
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            opacity: control.selected ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: AppTheme.normal } }
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: AppTheme.accent }
                GradientStop { position: 1.0; color: AppTheme.accentPink }
            }
        }
        FocusRing { target: control }
    }

    contentItem: Item {
        Row {
            id: row
            anchors.centerIn: parent
            spacing: 6
            Label {
                visible: control.emoji.length > 0
                text: control.emoji
                font.pixelSize: AppTheme.fontBody
                anchors.verticalCenter: parent.verticalCenter
            }
            Label {
                text: control.text
                color: control.selected ? "white" : AppTheme.textPrimary
                font.pixelSize: AppTheme.fontSmall
                font.weight: control.selected ? Font.DemiBold : Font.Medium
                anchors.verticalCenter: parent.verticalCenter
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
