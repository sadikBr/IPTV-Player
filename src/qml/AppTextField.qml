import QtQuick
import QtQuick.Controls
import IPTVPlayer

// Rounded text input matching the rest of the design.
TextField {
    id: control

    implicitHeight: 44
    leftPadding: 14
    rightPadding: 14
    topPadding: 0
    bottomPadding: 0
    verticalAlignment: TextInput.AlignVCenter
    font.pixelSize: AppTheme.fontBody
    color: AppTheme.textPrimary
    placeholderTextColor: AppTheme.textTertiary
    selectByMouse: true
    selectionColor: AppTheme.accentStrong
    selectedTextColor: "white"

    background: Rectangle {
        radius: AppTheme.radiusSmall + 2
        color: control.activeFocus ? AppTheme.surfaceHigh : AppTheme.backgroundRaised
        border.width: 1
        border.color: control.activeFocus ? AppTheme.accent
                    : control.hovered ? AppTheme.strokeStrong : AppTheme.stroke
        Behavior on border.color { ColorAnimation { duration: AppTheme.fast } }
    }
}
