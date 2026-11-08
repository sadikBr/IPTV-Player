import QtQuick
import IPTVPlayer

// Accent outline around the control that has keyboard / TV remote focus.
// Put it inside a control's background: FocusRing { target: control }
Rectangle {
    id: ring

    property Item target
    property real gap: 3

    anchors.fill: parent
    anchors.margins: -gap
    radius: parent && parent.radius !== undefined ? parent.radius + gap : AppTheme.radius
    color: "transparent"
    border.width: 2
    border.color: AppTheme.accent
    // Controls have visualFocus (keyboard focus only); plain items activeFocus.
    visible: target ? (target.visualFocus !== undefined ? target.visualFocus === true
                                                        : target.activeFocus) : false
}
