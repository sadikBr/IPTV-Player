import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T
import IPTVPlayer

// Rounded text input. Built on the Templates module (no Material styling),
// so the placeholder stays inside the box.
T.TextField {
    id: control

    // Space for an icon on the left (SearchField)
    property int iconSpace: 0

    implicitWidth: Math.max(200, placeholder.implicitWidth + leftPadding + rightPadding)
    implicitHeight: 44

    leftPadding: 14 + iconSpace
    rightPadding: 14
    topPadding: 0
    bottomPadding: 0
    verticalAlignment: TextInput.AlignVCenter
    font.family: AppTheme.fontFamily
    font.pixelSize: AppTheme.fontBody
    color: AppTheme.textPrimary
    placeholderTextColor: AppTheme.textTertiary
    selectionColor: AppTheme.accent
    selectedTextColor: "white"
    selectByMouse: true
    hoverEnabled: true

    PlaceholderText {
        id: placeholder
        x: control.leftPadding
        y: control.topPadding
        width: control.width - (control.leftPadding + control.rightPadding)
        height: control.height - (control.topPadding + control.bottomPadding)
        text: control.placeholderText
        font: control.font
        color: control.placeholderTextColor
        verticalAlignment: control.verticalAlignment
        elide: Text.ElideRight
        visible: !control.length && !control.preeditText
    }

    background: Rectangle {
        radius: AppTheme.radius
        color: control.activeFocus ? AppTheme.surfaceHigh : AppTheme.surface
        border.width: 1
        border.color: control.activeFocus ? AppTheme.strokeStrong
                    : control.hovered ? AppTheme.strokeStrong : AppTheme.stroke
        Behavior on color { ColorAnimation { duration: AppTheme.fast } }

        // Accent underline while focused
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            width: control.activeFocus ? parent.width - 2 * parent.radius : 0
            height: 2
            radius: 1
            color: AppTheme.accent
            Behavior on width { NumberAnimation { duration: AppTheme.normal; easing.type: Easing.OutCubic } }
        }
    }
}
