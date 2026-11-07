import QtQuick
import QtQuick.Controls
import IPTVPlayer

// Round gradient badge with initials (providers).
Rectangle {
    id: root

    property string name
    property int size: 36

    implicitWidth: size
    implicitHeight: size
    radius: size / 2
    gradient: Gradient {
        orientation: Gradient.Horizontal
        GradientStop { position: 0.0; color: AppTheme.accentStrong }
        GradientStop { position: 1.0; color: AppTheme.accent2 }
    }

    Label {
        anchors.centerIn: parent
        text: AppTheme.initials(root.name)
        color: "white"
        font.pixelSize: Math.round(root.size * 0.38)
        font.weight: Font.Bold
    }
}
