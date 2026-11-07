import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import IPTVPlayer

// On-screen controls drawn over the video: a top bar and a bottom bar.
Item {
    id: root

    property PlayerController player
    property string title
    property bool fullScreen: false
    property bool infoVisible: false
    property bool hasChannels: false
    property bool favorite: false
    // TV guide: {now: {...}, next: {...}} from EpgManager.nowNext()
    property var program: ({})
    readonly property bool compact: width < AppTheme.compactWidth
    readonly property bool interacting: seekSlider.pressed || volumeSlider.pressed

    signal backClicked()
    signal fullScreenClicked()
    signal infoClicked()
    signal previousChannel()
    signal nextChannel()
    signal channelListClicked()
    signal favoriteClicked()
    signal activity()

    function formatTime(ms) {
        const total = Math.max(0, Math.floor(ms / 1000))
        const h = Math.floor(total / 3600)
        const m = Math.floor((total % 3600) / 60)
        const s = total % 60
        const mm = h > 0 && m < 10 ? "0" + m : "" + m
        const ss = s < 10 ? "0" + s : "" + s
        return h > 0 ? h + ":" + mm + ":" + ss : mm + ":" + ss
    }

    // Top bar
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: topBar.implicitHeight + 2 * AppTheme.spacing
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#CC000000" }
            GradientStop { position: 1.0; color: "transparent" }
        }

        RowLayout {
            id: topBar
            anchors.fill: parent
            anchors.margins: AppTheme.spacing
            spacing: AppTheme.spacing

            IconButton {
                iconName: "back"
                tip: qsTr("Back")
                onClicked: root.backClicked()
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Label {
                    Layout.fillWidth: true
                    text: root.title
                    elide: Text.ElideMiddle
                    color: "white"
                    font.pixelSize: 16
                }
                // Now / next from the TV guide
                RowLayout {
                    visible: root.program.now !== undefined
                    Layout.fillWidth: true
                    spacing: 8
                    Label {
                        text: root.program.now
                              ? root.program.now.title + "  " + AppTheme.time(root.program.now.start)
                                + "–" + AppTheme.time(root.program.now.stop)
                              : ""
                        color: "#DDFFFFFF"
                        font.pixelSize: 13
                        elide: Text.ElideRight
                        Layout.maximumWidth: root.width * 0.5
                    }
                    Rectangle {
                        Layout.preferredWidth: 60
                        Layout.preferredHeight: 3
                        color: "#44FFFFFF"
                        Rectangle {
                            width: parent.width * (root.program.now ? root.program.now.progress : 0)
                            height: parent.height
                            color: AppTheme.accent
                        }
                    }
                    Label {
                        visible: root.program.next !== undefined && !root.compact
                        text: root.program.next ? qsTr("Next: %1").arg(root.program.next.title) : ""
                        color: "#99FFFFFF"
                        font.pixelSize: 13
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Item { Layout.fillWidth: !root.program.next || root.compact }
                }
            }
            IconButton {
                iconName: root.favorite ? "star" : "star-outline"
                tip: root.favorite ? qsTr("Remove from favorites") : qsTr("Add to favorites")
                visible: root.hasChannels
                icon.color: root.favorite ? AppTheme.favorite : "white"
                onClicked: { root.favoriteClicked(); root.activity() }
            }
            IconButton {
                iconName: "info"
                tip: qsTr("Stream info (I)")
                checkable: true
                checked: root.infoVisible
                onClicked: root.infoClicked()
            }
        }
    }

    // Bottom bar
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: bottomBar.implicitHeight + 2 * AppTheme.spacing
        gradient: Gradient {
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 1.0; color: "#CC000000" }
        }

        ColumnLayout {
            id: bottomBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: AppTheme.spacing
            spacing: 0

            // Seek bar for movies/episodes (hidden for live TV)
            RowLayout {
                Layout.fillWidth: true
                visible: root.player && !root.player.live
                spacing: AppTheme.spacing

                Label {
                    text: root.player ? root.formatTime(root.player.position) : ""
                    color: "white"
                }
                Slider {
                    id: seekSlider
                    Layout.fillWidth: true
                    from: 0
                    to: root.player ? Math.max(1, root.player.duration) : 1
                    enabled: root.player && root.player.seekable
                    focusPolicy: Qt.NoFocus
                    onMoved: root.activity()
                    onPressedChanged: if (!pressed && root.player) root.player.seek(value)

                    // Follow playback, except while the user drags the handle.
                    Binding on value {
                        when: !seekSlider.pressed
                        value: root.player ? root.player.position : 0
                        restoreMode: Binding.RestoreNone
                    }
                }
                Label {
                    text: root.player ? root.formatTime(root.player.duration) : ""
                    color: "white"
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: AppTheme.spacing / 2

                IconButton {
                    iconName: "previous"
                    tip: qsTr("Previous channel (Page Up)")
                    visible: root.hasChannels
                    onClicked: { root.previousChannel(); root.activity() }
                }
                IconButton {
                    iconName: root.player && (root.player.playing || root.player.busy) ? "pause" : "play"
                    tip: qsTr("Play / Pause (Space)")
                    onClicked: { root.player.togglePause(); root.activity() }
                }
                IconButton {
                    iconName: "next"
                    tip: qsTr("Next channel (Page Down)")
                    visible: root.hasChannels
                    onClicked: { root.nextChannel(); root.activity() }
                }
                IconButton {
                    iconName: "retry"
                    tip: qsTr("Reconnect")
                    visible: root.player && root.player.live
                    onClicked: { root.player.retry(); root.activity() }
                }

                Rectangle {
                    visible: root.player && root.player.live
                    color: "#E53935"
                    radius: 4
                    implicitWidth: liveLabel.implicitWidth + 12
                    implicitHeight: liveLabel.implicitHeight + 4
                    Label {
                        id: liveLabel
                        anchors.centerIn: parent
                        text: qsTr("LIVE")
                        color: "white"
                        font.bold: true
                        font.pixelSize: 12
                    }
                }

                Item { Layout.fillWidth: true }

                IconButton {
                    iconName: root.player && (root.player.muted || root.player.volume === 0) ? "mute" : "volume"
                    tip: qsTr("Mute (M)")
                    onClicked: { root.player.muted = !root.player.muted; root.activity() }
                }
                Slider {
                    id: volumeSlider
                    visible: !root.compact
                    Layout.preferredWidth: 120
                    from: 0
                    to: 1
                    focusPolicy: Qt.NoFocus
                    value: root.player ? (root.player.muted ? 0 : root.player.volume) : 1
                    onMoved: {
                        root.player.muted = false
                        root.player.volume = value
                        root.activity()
                    }
                }
                IconButton {
                    iconName: "list"
                    tip: qsTr("Channel list (L)")
                    visible: root.hasChannels
                    onClicked: { root.channelListClicked(); root.activity() }
                }
                IconButton {
                    iconName: root.fullScreen ? "fullscreen-exit" : "fullscreen"
                    tip: qsTr("Full screen (F)")
                    onClicked: root.fullScreenClicked()
                }
            }
        }
    }
}
