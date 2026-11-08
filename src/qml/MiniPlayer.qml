import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import IPTVPlayer

// Bar at the bottom of the window while a radio station plays: logo, name,
// what's on (song title when the station sends it), play/pause, favorite,
// stop. Stays while you browse the rest of the app.
Rectangle {
    id: root

    property PlayerController player

    readonly property var station: RadioManager.current

    implicitHeight: 68
    color: AppTheme.surface

    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 1
        color: AppTheme.stroke
    }

    Rectangle {
        id: art
        x: 12
        anchors.verticalCenter: parent.verticalCenter
        width: 48
        height: 48
        radius: AppTheme.radiusSmall
        color: "white"
        clip: true
        ArtworkPlaceholder {
            anchors.fill: parent
            visible: !AppTheme.hasArtwork(logo)
            title: root.station.name || ""
            iconName: "radio"
            showTitle: false
        }
        Image {
            id: logo
            anchors.fill: parent
            anchors.margins: 3
            source: root.station.logo || ""
            asynchronous: true
            fillMode: Image.PreserveAspectFit
            sourceSize.width: 96
            visible: AppTheme.hasArtwork(logo)
        }
    }

    Column {
        anchors.left: art.right
        anchors.leftMargin: 12
        anchors.right: buttons.left
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        spacing: 3
        Label {
            width: parent.width
            text: root.station.name || ""
            elide: Text.ElideRight
            font.weight: Font.DemiBold
            color: AppTheme.textPrimary
        }
        Label {
            width: parent.width
            elide: Text.ElideRight
            font.pixelSize: AppTheme.fontCaption
            color: root.player.failed ? AppTheme.error : AppTheme.textTertiary
            text: root.player.failed ? qsTr("This station isn't playing right now")
                : root.player.busy ? qsTr("Connecting…")
                : root.player.mediaTitle.length > 0 ? "♪ " + root.player.mediaTitle
                : SleepTimer.active ? SleepTimer.label()
                : root.station.details || ""
        }
    }

    Row {
        id: buttons
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2
        SleepTimerButton {}
        IconButton {
            iconName: root.station.favorite ? "heart" : "heart-outline"
            iconColor: root.station.favorite ? AppTheme.accent : AppTheme.textSecondary
            tip: root.station.favorite ? qsTr("Remove from favorites") : qsTr("Add to favorites")
            onClicked: RadioManager.toggleCurrentFavorite()
        }
        IconButton {
            filled: true
            iconName: root.player.playing || root.player.busy ? "pause" : "play"
            tip: root.player.playing ? qsTr("Pause") : qsTr("Play")
            onClicked: {
                if (root.player.playing || root.player.busy)
                    root.player.stop()
                else
                    root.player.retry()
            }
        }
        IconButton {
            iconName: "close"
            tip: qsTr("Stop the radio")
            onClicked: {
                root.player.stop()
                root.player.open("")
            }
        }
    }
}
