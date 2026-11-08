import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import QtMultimedia
import IPTVPlayer

// One screen of the multi-view: a channel with its own player. Only the
// selected tile plays sound.
Item {
    id: tile

    property ChannelListModel channelModel
    property int channelIndex: -1       // row in channelModel, -1 = empty
    property int sourceId: -1
    property bool selected: false

    signal selectRequested()
    signal pickRequested()
    signal clearRequested()
    signal fullScreenRequested()

    readonly property var channel: channelModel && channelIndex >= 0 ? channelModel.get(channelIndex) : ({})
    readonly property bool empty: channelIndex < 0
    property var program: ({})

    function play() {
        if (empty) {
            player.stop()
            return
        }
        player.open(channelModel.streamUrl(channelIndex))
        program = channel.epgKey ? EpgManager.nowNext(sourceId, channel.epgKey) : ({})
    }

    onChannelIndexChanged: play()

    PlayerController {
        id: player
        autoReconnect: AppSettings.autoReconnect
        lowLatency: AppSettings.lowLatency
        // Sound only from the selected tile
        muted: !tile.selected
        volume: AppSettings.volume
        volumeScale: SleepTimer.fade
    }

    Component.onCompleted: {
        player.videoOutput = video
        play()
    }
    Component.onDestruction: player.stop()

    // What's on: refreshed every minute
    Timer {
        interval: 60000
        repeat: true
        running: !tile.empty
        onTriggered: tile.program = tile.channel.epgKey
                     ? EpgManager.nowNext(tile.sourceId, tile.channel.epgKey) : ({})
    }

    Rectangle {
        id: frame
        anchors.fill: parent
        anchors.margins: 3
        radius: AppTheme.radius
        color: "black"
        clip: true

        VideoOutput {
            id: video
            anchors.fill: parent
            fillMode: VideoOutput.PreserveAspectFit
            visible: !tile.empty
        }

        // Empty: add a channel
        AbstractButton {
            anchors.fill: parent
            visible: tile.empty
            focusPolicy: Qt.TabFocus
            onClicked: tile.pickRequested()
            scale: pressed ? 0.95 : 1
            Behavior on scale { NumberAnimation { duration: 260; easing.type: Easing.OutBack; easing.overshoot: 3 } }
            background: Rectangle {
                radius: AppTheme.radius
                color: AppTheme.surface
                border.width: 2
                border.color: AppTheme.strokeStrong
                FocusRing { target: parent.parent }
            }
            contentItem: Column {
                spacing: 8
                Item { width: 1; height: (parent.height - 80) / 2 }
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 48
                    height: 48
                    radius: 24
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: AppTheme.accent }
                        GradientStop { position: 1.0; color: AppTheme.accentPink }
                    }
                    IconImage {
                        anchors.centerIn: parent
                        source: "icons/add.svg"
                        sourceSize: Qt.size(22, 22)
                        color: "white"
                    }
                }
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Add a channel")
                    color: AppTheme.textSecondary
                    font.pixelSize: AppTheme.fontSmall
                }
            }
        }

        // Tap: sound from this one. Double tap: full screen.
        TapHandler {
            enabled: !tile.empty
            onTapped: tile.selectRequested()
            onDoubleTapped: tile.fullScreenRequested()
        }

        // Loading / failed
        BusyIndicator {
            anchors.centerIn: parent
            running: !tile.empty && player.busy && !player.failed
            visible: running
        }
        Label {
            anchors.centerIn: parent
            width: parent.width - 32
            visible: !tile.empty && player.failed
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("Can't play this channel here. Your provider may allow fewer streams at the same time.")
            color: AppTheme.textSecondary
            font.pixelSize: AppTheme.fontSmall
        }

        // Name, what's on, and buttons
        Rectangle {
            visible: !tile.empty
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: info.implicitHeight + 16
            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 1.0; color: "#CC000000" }
            }
            RowLayout {
                id: info
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 8
                spacing: 6
                IconImage {
                    source: tile.selected ? "icons/volume.svg" : "icons/mute.svg"
                    sourceSize: Qt.size(16, 16)
                    color: tile.selected ? AppTheme.accent2 : "#99FFFFFF"
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    spacing: 0
                    Label {
                        Layout.fillWidth: true
                        text: tile.channel.name || ""
                        elide: Text.ElideRight
                        color: "white"
                        font.pixelSize: AppTheme.fontSmall
                        font.weight: Font.DemiBold
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: text.length > 0
                        text: tile.program && tile.program.now ? tile.program.now.title : ""
                        elide: Text.ElideRight
                        color: "#B3FFFFFF"
                        font.pixelSize: AppTheme.fontCaption
                    }
                }
                IconButton {
                    iconName: "list"
                    tip: qsTr("Change channel")
                    onClicked: tile.pickRequested()
                }
                IconButton {
                    iconName: "fullscreen"
                    tip: qsTr("Watch full screen")
                    onClicked: tile.fullScreenRequested()
                }
                IconButton {
                    iconName: "close"
                    tip: qsTr("Remove")
                    onClicked: tile.clearRequested()
                }
            }
        }
    }

    // The selected tile glows
    Rectangle {
        anchors.fill: frame
        anchors.margins: -2
        radius: AppTheme.radius + 2
        color: "transparent"
        border.width: 3
        border.color: AppTheme.accent
        visible: tile.selected && !tile.empty
        opacity: 0.95
    }
}
