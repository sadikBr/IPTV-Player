import QtQuick
import IPTVPlayer

// Full-width artwork at the top of a details page, fading into the page.
// Without a backdrop, a heavily scaled-down poster gives a soft colour wash.
Item {
    id: root

    property string backdrop
    property string poster

    clip: true

    Image {
        anchors.fill: parent
        source: root.backdrop.length > 0 ? root.backdrop : root.poster
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        // A tiny poster stretched over the area looks blurred.
        sourceSize.width: root.backdrop.length > 0 ? 1600 : 40
        opacity: root.backdrop.length > 0 ? 0.9 : 0.45
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#E60B0B0D" }
            GradientStop { position: 0.5; color: "#800B0B0D" }
            GradientStop { position: 1.0; color: "#1A0B0B0D" }
        }
    }
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: parent.height * 0.65
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#000B0B0D" }
            GradientStop { position: 1.0; color: AppTheme.background }
        }
    }
}
