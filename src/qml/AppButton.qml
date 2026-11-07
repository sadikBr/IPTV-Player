import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import IPTVPlayer

// Pill-shaped button. variant: "primary" (gradient), "secondary" or "ghost".
Button {
    id: control

    property string variant: "primary"
    property string iconName

    readonly property bool primary: variant === "primary"

    implicitHeight: 44
    leftPadding: iconName ? 18 : 22
    rightPadding: 22
    font.pixelSize: AppTheme.fontBody
    font.weight: Font.DemiBold
    hoverEnabled: true
    opacity: enabled ? 1 : 0.45

    icon.source: iconName ? "icons/" + iconName + ".svg" : ""
    icon.width: 20
    icon.height: 20
    icon.color: primary ? "white" : AppTheme.textPrimary

    contentItem: IconLabel {
        spacing: 8
        display: control.display
        icon: control.icon
        text: control.text
        font: control.font
        color: control.primary ? "white" : AppTheme.textPrimary
    }

    background: Rectangle {
        implicitWidth: 64
        radius: height / 2
        color: control.variant === "secondary"
               ? (control.hovered ? AppTheme.surfaceHover : AppTheme.surfaceHigh)
               : control.variant === "ghost" ? (control.hovered ? AppTheme.hover : "transparent")
                                             : AppTheme.accentStrong
        border.width: control.variant === "secondary" ? 1 : 0
        border.color: AppTheme.strokeStrong
        gradient: control.primary ? primaryGradient : null

        Gradient {
            id: primaryGradient
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: control.hovered ? "#7B6BFF" : AppTheme.accentStrong }
            GradientStop { position: 1.0; color: control.hovered ? "#4BDBF7" : "#2FB9DC" }
        }

        // Focus ring (keyboard / remote)
        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            radius: height / 2
            color: "transparent"
            border.color: AppTheme.accent
            border.width: 2
            visible: control.visualFocus
        }
    }

    scale: pressed ? 0.97 : 1
    Behavior on scale { NumberAnimation { duration: AppTheme.fast } }
}
