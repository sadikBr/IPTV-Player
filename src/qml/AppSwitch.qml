import QtQuick
import QtQuick.Templates as T
import IPTVPlayer

// On/off switch: accent track with a white knob when on.
T.Switch {
    id: control

    implicitWidth: 44
    implicitHeight: 26
    padding: 0
    hoverEnabled: true
    focusPolicy: Qt.TabFocus

    indicator: Rectangle {
        implicitWidth: 44
        implicitHeight: 26
        x: control.leftPadding
        y: (control.height - height) / 2
        radius: height / 2
        color: control.checked ? AppTheme.accent : control.hovered ? AppTheme.surfaceHover : AppTheme.surfaceHigh
        border.width: control.checked ? 0 : 1
        border.color: AppTheme.strokeStrong
        Behavior on color { ColorAnimation { duration: AppTheme.fast } }
        FocusRing { target: control }

        Rectangle {
            x: control.checked ? parent.width - width - 3 : 3
            anchors.verticalCenter: parent.verticalCenter
            width: 20
            height: 20
            radius: 10
            color: control.checked ? "white" : AppTheme.textSecondary
            Behavior on x { NumberAnimation { duration: AppTheme.normal; easing.type: Easing.OutCubic } }
        }
    }
    contentItem: Item {}
}
