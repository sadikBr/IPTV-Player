import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import IPTVPlayer

// Round icon-only button showing one of the bundled SVG icons.
AbstractButton {
    id: control

    property string iconName
    property string tip
    property color iconColor: AppTheme.textPrimary
    property int size: 40
    // Filled circle behind the icon (e.g. over images)
    property bool filled: false

    implicitWidth: size
    implicitHeight: size
    focusPolicy: Qt.NoFocus
    hoverEnabled: true
    opacity: enabled ? 1 : 0.4

    Accessible.name: tip
    ToolTip.visible: hovered && tip.length > 0
    ToolTip.text: tip
    ToolTip.delay: 600

    background: Rectangle {
        radius: width / 2
        color: control.checked ? AppTheme.selection
             : control.pressed ? "#26FFFFFF"
             : control.hovered ? "#1AFFFFFF"
             : control.filled ? "#99000000" : "transparent"
        Behavior on color { ColorAnimation { duration: AppTheme.fast } }
    }

    contentItem: Item {
        IconImage {
            anchors.centerIn: parent
            source: control.iconName ? "icons/" + control.iconName + ".svg" : ""
            sourceSize: Qt.size(Math.round(control.size * 0.55), Math.round(control.size * 0.55))
            color: control.checked ? AppTheme.accent : control.iconColor
        }
    }

    scale: pressed ? 0.92 : 1
    Behavior on scale { NumberAnimation { duration: AppTheme.fast } }
}
