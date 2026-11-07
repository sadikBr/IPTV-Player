import QtQuick
import QtQuick.Templates as T
import IPTVPlayer

// Thin, rounded scroll bar that fades out when not in use and grows a
// little when hovered.
T.ScrollBar {
    id: control

    readonly property real thickness: control.hovered || control.pressed ? 8 : 5

    implicitWidth: horizontal ? 100 : thickness + leftPadding + rightPadding
    implicitHeight: horizontal ? thickness + topPadding + bottomPadding : 100

    padding: 3
    minimumSize: 0.06
    visible: control.policy !== T.ScrollBar.AlwaysOff && control.size < 1.0
    hoverEnabled: true

    contentItem: Rectangle {
        implicitWidth: control.horizontal ? 100 : control.thickness
        implicitHeight: control.horizontal ? control.thickness : 100
        radius: Math.min(width, height) / 2
        color: control.pressed ? "#B3FFFFFF" : control.hovered ? "#80FFFFFF" : "#4DFFFFFF"
        opacity: control.policy === T.ScrollBar.AlwaysOn || control.active || control.hovered ? 1 : 0

        Behavior on opacity { NumberAnimation { duration: 400; easing.type: Easing.OutQuad } }
        Behavior on implicitWidth { NumberAnimation { duration: AppTheme.fast } }
        Behavior on implicitHeight { NumberAnimation { duration: AppTheme.fast } }
        Behavior on color { ColorAnimation { duration: AppTheme.fast } }
    }

    background: Item {}
}
