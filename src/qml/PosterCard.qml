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

    readonly property bool emphasized: hovered || visualFocus

    padding: 0
    hoverEnabled: true
    focusPolicy: Qt.TabFocus

    background: Item {}

    contentItem: Column {
        spacing: 10

        Item {
            width: parent.width
            height: width * 1.5

            Shadow {
                target: frame
                opacity: root.emphasized ? 1 : 0.6
                Behavior on opacity { NumberAnimation { duration: AppTheme.normal } }
            }

            Rectangle {
                id: frame
                anchors.fill: parent
                radius: AppTheme.radius
                color: AppTheme.surfaceHigh
                clip: true
                scale: root.emphasized ? 1.04 : 1
                Behavior on scale { NumberAnimation { duration: AppTheme.normal; easing.type: Easing.OutCubic } }

                // While loading, or when there is no (working) poster
                ArtworkPlaceholder {
                    anchors.fill: parent
                    visible: !AppTheme.hasArtwork(posterImage)
                    title: root.title
                }

                Image {
                    id: posterImage
                    anchors.fill: parent
                    source: root.poster
                    asynchronous: true
                    cache: true
                    fillMode: Image.PreserveAspectCrop
                    sourceSize.width: 360
                    opacity: AppTheme.hasArtwork(posterImage) ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: AppTheme.normal } }
                }

                // Hover: darken and show a play button
                Rectangle {
                    anchors.fill: parent
                    color: "#80000000"
                    opacity: root.emphasized ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: AppTheme.normal } }
                    Rectangle {
                        anchors.centerIn: parent
                        width: 52
                        height: 52
                        radius: 26
                        color: "#E6FFFFFF"
                        IconImage {
                            anchors.centerIn: parent
                            anchors.horizontalCenterOffset: 2
                            source: "icons/play.svg"
                            sourceSize: Qt.size(24, 24)
                            color: "#0B0B0D"
                        }
                    }
                }

                // Bottom shade for the progress bar
                Rectangle {
                    visible: root.watched > 0
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: parent.height * 0.3
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "transparent" }
                        GradientStop { position: 1.0; color: "#CC000000" }
                    }
                }

                // Rating
                Rectangle {
                    visible: root.rating.length > 0
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.margins: 8
                    width: ratingRow.implicitWidth + 12
                    height: 22
                    radius: 6
                    color: "#B3000000"
                    Row {
                        id: ratingRow
                        anchors.centerIn: parent
                        spacing: 3
                        IconImage {
                            anchors.verticalCenter: parent.verticalCenter
                            source: "icons/star.svg"
                            sourceSize: Qt.size(11, 11)
                            color: AppTheme.favorite
                        }
                        Label {
                            text: root.rating
                            color: "white"
                            font.pixelSize: 11
                            font.weight: Font.Bold
                        }
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
                        sourceSize: Qt.size(13, 13)
                        color: AppTheme.accent
                    }
                }

                // Watched progress
                Rectangle {
                    visible: root.watched > 0
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 10
                    height: 4
                    radius: 2
                    color: "#4DFFFFFF"
                    Rectangle {
                        width: parent.width * Math.min(1, root.watched)
                        height: parent.height
                        radius: 2
                        color: AppTheme.accent
                    }
                }

                // Keyboard / remote focus
                Rectangle {
                    anchors.fill: parent
                    radius: AppTheme.radius
                    color: "transparent"
                    border.width: 2
                    border.color: "white"
                    visible: root.visualFocus
                }
            }
        }

        Column {
            width: parent.width
            spacing: 2
            Label {
                width: parent.width
                text: root.title
                elide: Text.ElideRight
                color: root.emphasized ? "white" : AppTheme.textPrimary
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
            }
        }
    }
}
