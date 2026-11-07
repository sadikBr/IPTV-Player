import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import IPTVPlayer

// Poster with title underneath, used for movies and series.
ItemDelegate {
    id: root

    property string title
    property string poster
    property string subtitle      // e.g. "2023"
    property string rating        // e.g. "7.4"
    property real watched: 0      // 0..1, progress bar when > 0
    property bool favorite: false

    padding: 0
    hoverEnabled: true

    background: Item {}

    contentItem: Column {
        spacing: 8

        Item {
            width: parent.width
            height: width * 1.5

            Rectangle {
                id: frame
                anchors.fill: parent
                radius: AppTheme.radius
                color: AppTheme.surfaceHigh
                clip: true
                scale: root.hovered ? 1.03 : 1
                Behavior on scale { NumberAnimation { duration: AppTheme.normal; easing.type: Easing.OutCubic } }

                // Placeholder while loading / when there is no poster
                Rectangle {
                    anchors.fill: parent
                    visible: posterImage.status !== Image.Ready
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: AppTheme.surfaceHigh }
                        GradientStop { position: 1.0; color: AppTheme.surface }
                    }
                    Label {
                        anchors.centerIn: parent
                        width: parent.width - 20
                        text: root.title
                        wrapMode: Text.Wrap
                        horizontalAlignment: Text.AlignHCenter
                        maximumLineCount: 4
                        elide: Text.ElideRight
                        color: AppTheme.textSecondary
                        font.pixelSize: AppTheme.fontSmall
                        font.weight: Font.DemiBold
                    }
                }

                Image {
                    id: posterImage
                    anchors.fill: parent
                    source: root.poster
                    asynchronous: true
                    cache: true
                    fillMode: Image.PreserveAspectCrop
                    sourceSize.width: 320
                    opacity: status === Image.Ready ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: AppTheme.normal } }
                }

                // Darken the bottom so the progress bar stands out
                Rectangle {
                    visible: root.watched > 0
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: parent.height * 0.25
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "transparent" }
                        GradientStop { position: 1.0; color: "#AA000000" }
                    }
                }

                // Rating
                Rectangle {
                    visible: root.rating.length > 0
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.margins: 8
                    width: ratingLabel.implicitWidth + 14
                    height: 22
                    radius: 11
                    color: "#B3000000"
                    Label {
                        id: ratingLabel
                        anchors.centerIn: parent
                        text: "★ " + root.rating
                        color: AppTheme.favorite
                        font.pixelSize: 11
                        font.weight: Font.Bold
                    }
                }

                // Favorite
                Rectangle {
                    visible: root.favorite
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.margins: 8
                    width: 24
                    height: 24
                    radius: 12
                    color: "#B3000000"
                    IconImage {
                        anchors.centerIn: parent
                        source: "icons/heart.svg"
                        sourceSize: Qt.size(14, 14)
                        color: AppTheme.live
                    }
                }

                // Watched progress
                Rectangle {
                    visible: root.watched > 0
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 8
                    height: 4
                    radius: 2
                    color: "#55FFFFFF"
                    Rectangle {
                        width: parent.width * Math.min(1, root.watched)
                        height: parent.height
                        radius: 2
                        color: AppTheme.accent
                    }
                }
            }

            // Hover / focus outline
            Rectangle {
                anchors.fill: frame
                scale: frame.scale
                radius: AppTheme.radius
                color: "transparent"
                border.width: 2
                border.color: AppTheme.accent
                visible: root.visualFocus || root.hovered
                opacity: root.visualFocus ? 1 : 0.6
            }
        }

        Label {
            width: parent.width
            text: root.title
            elide: Text.ElideRight
            color: AppTheme.textPrimary
            font.pixelSize: AppTheme.fontSmall
            font.weight: Font.DemiBold
        }
        Label {
            width: parent.width
            visible: text.length > 0
            text: root.subtitle
            elide: Text.ElideRight
            color: AppTheme.textTertiary
            font.pixelSize: AppTheme.fontCaption
            topPadding: -4
        }
    }
}
