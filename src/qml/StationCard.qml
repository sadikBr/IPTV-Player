import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import IPTVPlayer

// A radio station: logo (or a coloured placeholder), name and details.
ItemDelegate {
    id: root

    property string name
    property string logo
    property string details
    property bool favorite: false
    property bool playing: false

    signal favoriteClicked()

    readonly property bool emphasized: hovered || visualFocus

    padding: 0
    hoverEnabled: true
    focusPolicy: Qt.TabFocus

    background: Rectangle {
        radius: AppTheme.radius
        color: root.emphasized ? AppTheme.surfaceHover : AppTheme.surface
        border.width: root.playing ? 2 : 1
        border.color: root.playing ? AppTheme.accent : AppTheme.stroke
        Behavior on color { ColorAnimation { duration: AppTheme.fast } }
        FocusRing { target: root }
    }

    contentItem: Item {
        Rectangle {
            id: art
            x: 12
            anchors.verticalCenter: parent.verticalCenter
            width: 56
            height: 56
            radius: AppTheme.radiusSmall
            color: "white"
            clip: true
            ArtworkPlaceholder {
                anchors.fill: parent
                visible: !AppTheme.hasArtwork(logoImage)
                title: root.name
                iconName: "radio"
                showTitle: false
            }
            Image {
                id: logoImage
                anchors.fill: parent
                anchors.margins: 4
                source: root.logo
                asynchronous: true
                fillMode: Image.PreserveAspectFit
                sourceSize.width: 112
                visible: AppTheme.hasArtwork(logoImage)
            }
        }
        Column {
            anchors.left: art.right
            anchors.leftMargin: 12
            anchors.right: heart.left
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4
            Label {
                width: parent.width
                text: root.name
                elide: Text.ElideRight
                font.weight: Font.DemiBold
                color: root.playing ? AppTheme.accent2 : AppTheme.textPrimary
            }
            Label {
                width: parent.width
                visible: text.length > 0
                text: root.details
                elide: Text.ElideRight
                font.pixelSize: AppTheme.fontCaption
                color: AppTheme.textTertiary
            }
        }
        IconButton {
            id: heart
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            size: 36
            iconName: root.favorite ? "heart" : "heart-outline"
            iconColor: root.favorite ? AppTheme.accent : AppTheme.textTertiary
            tip: root.favorite ? qsTr("Remove from favorites") : qsTr("Add to favorites")
            onClicked: root.favoriteClicked()
        }
    }
}
