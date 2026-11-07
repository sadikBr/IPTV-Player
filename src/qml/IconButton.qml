import QtQuick
import QtQuick.Controls

// Flat round button showing one of the bundled SVG icons.
RoundButton {
    id: control

    property string iconName
    property string tip

    flat: true
    focusPolicy: Qt.NoFocus
    icon.source: iconName ? "icons/" + iconName + ".svg" : ""
    icon.width: 24
    icon.height: 24
    icon.color: "white"

    Accessible.name: tip
    ToolTip.visible: hovered && tip.length > 0
    ToolTip.text: tip
    ToolTip.delay: 600
}
