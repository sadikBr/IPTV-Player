import QtQuick
import QtQuick.Controls
import IPTVPlayer

// Poster with title underneath, used for movies and series.
ItemDelegate {
    id: root

    property string title
    property string poster
    property string subtitle      // e.g. "2023 · ★ 7.4"
    property real watched: 0      // 0..1, progress bar when > 0
    property bool favorite: false

    padding: 6
    hoverEnabled: true

    background: Rectangle {
        radius: AppTheme.radius
        color: root.hovered || root.visualFocus ? "#14FFFFFF" : "transparent"
        border.color: root.visualFocus ? AppTheme.accent : "transparent"
    }

    contentItem: Column {
        spacing: 6

        Rectangle {
            id: frame
            width: parent.width
            height: width * 1.5
            radius: 6
            color: AppTheme.surface
            clip: true

            Label {
                anchors.centerIn: parent
                width: parent.width - 16
                visible: posterImage.status !== Image.Ready
                text: root.title
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                maximumLineCount: 4
                elide: Text.ElideRight
                color: AppTheme.textSecondary
                font.pixelSize: 13
            }
            Image {
                id: posterImage
                anchors.fill: parent
                source: root.poster
                asynchronous: true
                cache: true
                fillMode: Image.PreserveAspectCrop
                sourceSize.width: 300
            }

            // Favorite badge
            Rectangle {
                visible: root.favorite
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: 6
                width: 24
                height: 24
                radius: 12
                color: "#AA000000"
                Label {
                    anchors.centerIn: parent
                    text: "★"
                    color: AppTheme.favorite
                    font.pixelSize: 14
                }
            }

            // Watched progress
            Rectangle {
                visible: root.watched > 0
                anchors.bottom: parent.bottom
                width: parent.width
                height: 4
                color: "#88000000"
                Rectangle {
                    width: parent.width * root.watched
                    height: parent.height
                    color: AppTheme.accent
                }
            }
        }

        Label {
            width: parent.width
            text: root.title
            elide: Text.ElideRight
            maximumLineCount: 2
            wrapMode: Text.Wrap
            color: AppTheme.textPrimary
            font.pixelSize: 13
        }
        Label {
            width: parent.width
            visible: text.length > 0
            text: root.subtitle
            elide: Text.ElideRight
            color: AppTheme.textSecondary
            font.pixelSize: 11
        }
    }
}
