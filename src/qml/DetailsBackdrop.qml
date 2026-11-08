import QtQuick
import IPTVPlayer

// Full-width artwork at the top of a details page, fading into the page.
// Without a backdrop, a heavily scaled-down poster gives a soft colour wash.
Item {
    id: root

    property string backdrop
    property string poster
    property string title // colour of the wash when there is no artwork

    clip: true

    // No artwork (or it doesn't load): a soft wash in the title's colour.
    Rectangle {
        anchors.fill: parent
        visible: !AppTheme.hasArtwork(art)
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: AppTheme.background }
            GradientStop { position: 1.0; color: AppTheme.artColor(root.title, 0.2) }
        }
    }

    Image {
        id: art
        anchors.fill: parent
        visible: AppTheme.hasArtwork(art)
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
