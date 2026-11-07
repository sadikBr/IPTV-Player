import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtMultimedia
import IPTVPlayer

Page {
    id: page

    // Either a single URL (streamUrl/streamTitle) ...
    property url streamUrl
    property string streamTitle
    // ... or a channel list to zap through.
    property ChannelListModel channelModel: null
    property int channelIndex: -1
    property int sourceId: -1

    property ApplicationWindow appWindow

    readonly property bool hasChannels: channelModel !== null && channelModel.count > 0
    readonly property bool fullScreen: appWindow ? appWindow.fullScreen : false
    property bool controlsVisible: true
    property bool infoVisible: false
    property bool channelListVisible: false
    property bool currentFavorite: false
    property string typedNumber: ""

    signal closeRequested()

    function showControls() {
        controlsVisible = true
        hideTimer.restart()
    }

    function toggleFullScreen() {
        if (appWindow)
            appWindow.toggleFullScreen()
        showControls()
    }

    function goBack() {
        if (channelListVisible)
            channelListVisible = false
        else if (fullScreen)
            appWindow.setFullScreen(false)
        else
            page.closeRequested()
    }

    function changeVolume(delta) {
        player.muted = false
        player.volume = Math.max(0, Math.min(1, player.volume + delta))
        volumeOsd.show()
    }

    function playChannel(index) {
        if (!hasChannels)
            return
        const count = channelModel.count
        index = ((index % count) + count) % count
        const channel = channelModel.get(index)
        channelIndex = index
        streamTitle = channel.number + "  " + channel.name
        currentFavorite = channel.favorite
        streamUrl = channelModel.streamUrl(index)
        player.open(streamUrl)
        AppSettings.setLastChannel(sourceId, channel.extId, channel.name)
        channelList.currentIndex = index
        channelList.positionViewAtIndex(index, ListView.Center)
        channelOsd.show()
        showControls()
    }

    function toggleFavorite() {
        if (!hasChannels || channelIndex < 0)
            return
        channelModel.toggleFavorite(channelIndex)
        currentFavorite = channelModel.get(channelIndex).favorite
    }

    function typeDigit(digit) {
        if (!hasChannels)
            return
        typedNumber = (typedNumber + digit).slice(-5)
        numberTimer.restart()
    }

    focus: true
    background: Rectangle { color: "black" }

    PlayerController {
        id: player
        onVolumeChanged: AppSettings.volume = volume
    }

    Component.onCompleted: {
        player.videoOutput = videoOutput
        player.volume = AppSettings.volume
        if (hasChannels && channelIndex >= 0)
            playChannel(channelIndex)
        else
            player.open(streamUrl)
        forceActiveFocus()
        showControls()
    }

    Timer {
        id: hideTimer
        interval: 3500
        onTriggered: {
            if (player.playing && !controls.interacting)
                page.controlsVisible = false
        }
    }

    // Typing a channel number jumps to it after a short pause.
    Timer {
        id: numberTimer
        interval: 1500
        onTriggered: {
            const index = page.channelModel.indexOfNumber(parseInt(page.typedNumber))
            page.typedNumber = ""
            if (index >= 0)
                page.playChannel(index)
        }
    }

    VideoOutput {
        id: videoOutput
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
    }

    // Mouse / touch: move or tap shows controls; tap again hides; double-click toggles
    // fullscreen; wheel changes volume.
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: page.controlsVisible ? Qt.ArrowCursor : Qt.BlankCursor
        onPositionChanged: if (containsMouse && !pressed) page.showControls()
        onClicked: {
            if (page.channelListVisible) {
                page.channelListVisible = false
            } else if (page.controlsVisible && player.playing) {
                page.controlsVisible = false
                hideTimer.stop()
            } else {
                page.showControls()
            }
        }
        onDoubleClicked: page.toggleFullScreen()
        onWheel: (wheel) => page.changeVolume(wheel.angleDelta.y > 0 ? 0.05 : -0.05)
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: player.busy
        visible: running
    }

    // Error state
    Pane {
        anchors.centerIn: parent
        width: Math.min(page.width - 2 * AppTheme.margin, 460)
        visible: player.failed
        padding: AppTheme.margin
        background: Rectangle { color: AppTheme.surface; radius: AppTheme.radius }

        ColumnLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: AppTheme.spacing

            Label {
                text: qsTr("Can't play this stream")
                font.pixelSize: 18
                font.bold: true
                color: AppTheme.textPrimary
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: AppTheme.textSecondary
                text: player.errorString
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                Button {
                    text: qsTr("Back")
                    flat: true
                    onClicked: page.closeRequested()
                }
                Button {
                    text: page.hasChannels ? qsTr("Next channel") : ""
                    visible: page.hasChannels
                    flat: true
                    onClicked: page.playChannel(page.channelIndex + 1)
                }
                Button {
                    text: qsTr("Retry")
                    highlighted: true
                    onClicked: player.retry()
                }
            }
        }
    }

    // Technical stream details, toggled with the (i) button or the I key.
    Pane {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 72
        anchors.rightMargin: AppTheme.margin
        width: Math.min(page.width - 2 * AppTheme.margin, 380)
        visible: page.infoVisible
        padding: AppTheme.spacing
        background: Rectangle { color: AppTheme.overlay; radius: AppTheme.radius }

        Label {
            width: parent.width
            wrapMode: Text.Wrap
            color: AppTheme.textPrimary
            font.family: "monospace"
            text: player.streamInfo.length > 0 ? player.streamInfo : qsTr("Waiting for stream…")
        }
    }

    // Channel name shown briefly after switching channels.
    Osd {
        id: channelOsd
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 80
        text: page.streamTitle
    }

    Osd {
        id: volumeOsd
        anchors.centerIn: parent
        text: player.muted ? qsTr("Muted") : qsTr("Volume %1%").arg(Math.round(player.volume * 100))
    }

    Osd {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 80
        text: page.typedNumber
        shown: page.typedNumber.length > 0
        font.pixelSize: 40
    }

    PlayerControls {
        id: controls
        anchors.fill: parent
        player: player
        title: page.streamTitle.length > 0 ? page.streamTitle : page.streamUrl.toString()
        fullScreen: page.fullScreen
        infoVisible: page.infoVisible
        hasChannels: page.hasChannels
        favorite: page.currentFavorite
        opacity: page.controlsVisible || !player.playing ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 200 } }

        onBackClicked: page.goBack()
        onFullScreenClicked: page.toggleFullScreen()
        onInfoClicked: page.infoVisible = !page.infoVisible
        onPreviousChannel: page.playChannel(page.channelIndex - 1)
        onNextChannel: page.playChannel(page.channelIndex + 1)
        onChannelListClicked: page.channelListVisible = !page.channelListVisible
        onFavoriteClicked: page.toggleFavorite()
        onActivity: page.showControls()
    }

    // Channel list panel
    Rectangle {
        id: channelPanel
        width: Math.min(380, page.width * 0.85)
        height: parent.height
        x: page.channelListVisible ? 0 : -width
        visible: x > -width
        color: "#E6121418"
        Behavior on x { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

        ListView {
            id: channelList
            anchors.fill: parent
            anchors.topMargin: AppTheme.spacing
            model: page.channelModel
            clip: true
            currentIndex: page.channelIndex
            highlightMoveDuration: 0
            ScrollIndicator.vertical: ScrollIndicator {}

            delegate: ChannelDelegate {
                width: channelList.width
                current: index === page.channelIndex
                onClicked: {
                    page.playChannel(index)
                    page.channelListVisible = false
                }
                onFavoriteClicked: {
                    page.channelModel.toggleFavorite(index)
                    if (index === page.channelIndex)
                        page.currentFavorite = !page.currentFavorite
                }
            }
        }
    }

    Keys.onPressed: (event) => {
        if (event.key >= Qt.Key_0 && event.key <= Qt.Key_9 && page.hasChannels) {
            page.typeDigit(event.key - Qt.Key_0)
            event.accepted = true
            return
        }
        switch (event.key) {
        case Qt.Key_Space:
        case Qt.Key_K:
        case Qt.Key_MediaTogglePlayPause:
            player.togglePause(); page.showControls(); break
        case Qt.Key_F:
        case Qt.Key_F11:
            page.toggleFullScreen(); break
        case Qt.Key_M:
            player.muted = !player.muted; volumeOsd.show(); break
        case Qt.Key_I:
            page.infoVisible = !page.infoVisible; break
        case Qt.Key_L:
            page.channelListVisible = !page.channelListVisible; break
        case Qt.Key_S:
            page.toggleFavorite(); break
        case Qt.Key_PageUp:
        case Qt.Key_ChannelUp:
            page.playChannel(page.channelIndex - 1); break
        case Qt.Key_PageDown:
        case Qt.Key_ChannelDown:
            page.playChannel(page.channelIndex + 1); break
        case Qt.Key_Up:
            if (page.hasChannels) page.playChannel(page.channelIndex - 1)
            else page.changeVolume(0.05)
            break
        case Qt.Key_Down:
            if (page.hasChannels) page.playChannel(page.channelIndex + 1)
            else page.changeVolume(-0.05)
            break
        case Qt.Key_Plus:
        case Qt.Key_Equal:
        case Qt.Key_VolumeUp:
            page.changeVolume(0.05); break
        case Qt.Key_Minus:
        case Qt.Key_VolumeDown:
            page.changeVolume(-0.05); break
        case Qt.Key_Left:
            player.seekRelative(-10000); page.showControls(); break
        case Qt.Key_Right:
            player.seekRelative(10000); page.showControls(); break
        case Qt.Key_Escape:
        case Qt.Key_Back:
        case Qt.Key_Backspace:
            page.goBack(); break
        default:
            return
        }
        event.accepted = true
    }
}
