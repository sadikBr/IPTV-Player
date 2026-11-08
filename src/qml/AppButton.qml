import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T
import IPTVPlayer

// Button variants:
//  "primary"   white with dark text (the main action, e.g. Play)
//  "accent"    the warm accent colour
//  "secondary" translucent glass, works on top of artwork
//  "ghost"     text only
T.Button {
    id: control

    property string variant: "primary"
    property string iconName

    readonly property color textColor: variant === "primary" ? "#0B0B0D"
                                     : variant === "accent" ? "white" : AppTheme.textPrimary

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: 44
    leftPadding: iconName ? 18 : 24
    rightPadding: 24
    spacing: 8
    font.family: AppTheme.fontFamily
    font.pixelSize: AppTheme.fontBody
    font.weight: Font.DemiBold
    font.capitalization: Font.MixedCase
    hoverEnabled: true
    opacity: enabled ? 1 : 0.4

    icon.source: iconName ? "icons/" + iconName + ".svg" : ""
    icon.width: 20
    icon.height: 20
    icon.color: textColor

    contentItem: IconLabel {
        spacing: control.spacing
        display: control.display
        icon: control.icon
        text: control.text
        font: control.font
        color: control.textColor
    }

    background: Rectangle {
        implicitWidth: 64
        radius: AppTheme.radius - 2
        color: {
            switch (control.variant) {
            case "primary": return control.pressed ? "#D9D9DC" : control.hovered ? "#EDEDF0" : "white"
            case "accent": return control.pressed ? AppTheme.accentStrong : control.hovered ? "#FF6E4F" : AppTheme.accent
            case "secondary": return control.pressed ? "#59FFFFFF" : control.hovered ? AppTheme.glassHover : AppTheme.glass
            default: return control.hovered ? AppTheme.hover : "transparent"
            }
        }
        border.width: control.variant === "secondary" ? 1 : 0
        border.color: AppTheme.glassStroke
        Behavior on color { ColorAnimation { duration: AppTheme.fast } }

        // Focus ring (keyboard / remote)
        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            radius: parent.radius + 3
            color: "transparent"
            border.color: AppTheme.accent
            border.width: 2
            visible: control.visualFocus
        }
    }

    scale: pressed ? 0.93 : 1
    Behavior on scale { NumberAnimation { duration: 280; easing.type: Easing.OutBack; easing.overshoot: 3 } }
}
