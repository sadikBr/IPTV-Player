import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtMultimedia
import IPTVPlayer

Page {
    id: page

    // Either a single URL (streamUrl/streamTitle) ...
    property url streamUrl
    property string streamTitle
    // A catch-up program (from the archive): can be paused and seeked.
    property bool catchup: false
    // ... or a channel list to zap through ...
    property ChannelListModel channelModel: null
    property int channelIndex: -1
    property int sourceId: -1
    // ... or a movie ("movie") or an episode ("episode") of a series.
    property string vodKind: ""
    property string vodExtId: ""        // movie or episode id
    property string seriesExtId: ""     // episodes
    property bool startFromBeginning: false

    property ApplicationWindow appWindow
    // Hides the app navigation while playing.
    readonly property bool immersive: true

    readonly property bool hasChannels: channelModel !== null && channelModel.count > 0
    readonly property bool isVod: vodKind.length > 0
    // Android picture-in-picture: only the video shows in the small window.
    readonly property bool pip: PictureInPicture.active
    readonly property bool isEpisode: vodKind === "episode"
    property int episodeIndex: -1
    property var vodInfo: ({})          // title/image/season/episode for watch progress
    property bool nextEpisodePending: false
    readonly property bool fullScreen: appWindow ? appWindow.fullScreen : false
    property bool controlsVisible: true
    property bool infoVisible: false
    property bool channelListVisible: false
    property bool currentFavorite: false
    property string typedNumber: ""
    property string currentEpgKey: ""
    property var program: ({})

    function updateProgram() {
        program = sourceId > 0 && currentEpgKey.length > 0
                ? EpgManager.nowNext(sourceId, currentEpgKey) : ({})
    }

    signal closeRequested()
    // Multi-view, starting with the current channel
    signal multiViewRequested(int sourceId, string extId)

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
        if (index < 0 || index >= channelModel.count)
            channelModel.loadAll() // wrap around: the list may still be loading
        const count = channelModel.count
        index = ((index % count) + count) % count
        const channel = channelModel.get(index)
        channelIndex = index
        resetRetries()
        streamTitle = channel.number + "  " + channel.name
        currentFavorite = channel.favorite
        currentEpgKey = channel.epgKey
        updateProgram()
        streamUrl = channelModel.streamUrl(index)
        player.open(streamUrl)
        AppSettings.setLastChannel(sourceId, channel.extId, channel.name)
        channelList.currentIndex = index
        channelList.positionViewAtIndex(index, ListView.Center)
        channelOsd.show()
        showControls()
    }

    // --- Movies and episodes ---

    function saveProgress() {
        if (!isVod || player.duration <= 0 || player.position <= 0)
            return
        VodManager.saveProgress(sourceId, vodKind, vodExtId, player.position, player.duration, vodInfo)
    }

    function markFinished() {
        if (isVod && player.duration > 0)
            VodManager.saveProgress(sourceId, vodKind, vodExtId, player.duration, player.duration, vodInfo)
    }

    function playMovie() {
        const movie = VodManager.movie(sourceId, vodExtId)
        streamTitle = movie.name || ""
        vodInfo = { "title": movie.name, "image": movie.poster }
        streamUrl = VodManager.movieUrl(sourceId, vodExtId)
        const resume = startFromBeginning ? 0 : VodManager.resumePosition(sourceId, "movie", vodExtId)
        player.open(streamUrl, resume)
        showControls()
    }

    function playEpisode(index) {
        if (index < 0 || index >= episodes.count)
            return
        saveProgress()
        nextEpisodePending = false
        const ep = episodes.get(index)
        const series = VodManager.series(sourceId, seriesExtId)
        episodeIndex = index
        vodExtId = ep.extId
        streamTitle = (series.name ? series.name + "  ·  " : "")
                + qsTr("S%1 E%2").arg(ep.season).arg(ep.episode)
                + (ep.title ? "  ·  " + ep.title : "")
        vodInfo = { "title": series.name || ep.title, "image": series.poster || ep.image,
                    "seriesExtId": seriesExtId, "season": ep.season, "episode": ep.episode }
        streamUrl = episodes.streamUrl(index)
        const resume = startFromBeginning ? 0 : VodManager.resumePosition(sourceId, "episode", ep.extId)
        startFromBeginning = false
        player.open(streamUrl, resume)
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

    // Picture-in-picture: Android 12+ goes there by itself when the user leaves
    // the app while something plays; Android resizes the window on the way.
    Binding {
        target: PictureInPicture
        property: "autoEnter"
        value: player.playing || player.busy
    }
    onWidthChanged: PictureInPicture.refresh()
    onHeightChanged: PictureInPicture.refresh()

    // Sleep timer ran out: stop (after the fade out).
    Connections {
        target: SleepTimer
        function onExpired() {
            player.stop()
            page.showControls()
        }
    }

    PlayerController {
        id: player
        vod: page.isVod || page.catchup
        autoReconnect: AppSettings.autoReconnect
        lowLatency: AppSettings.lowLatency
        volumeScale: SleepTimer.fade
        onVolumeChanged: AppSettings.volume = volume
        onEnded: {
            page.markFinished()
            if (page.isEpisode && page.episodeIndex + 1 < episodes.count)
                page.nextEpisodePending = true
            else if (page.isVod)
                page.showControls()
        }
    }

    // All episodes of the series, to continue with the next one.
    EpisodeListModel {
        id: episodes
        sourceId: page.isEpisode ? page.sourceId : -1
        seriesExtId: page.isEpisode ? page.seriesExtId : ""
    }

    Component.onCompleted: {
        player.videoOutput = videoOutput
        player.volume = AppSettings.volume
        if (hasChannels && channelIndex >= 0)
            playChannel(channelIndex)
        else if (vodKind === "movie")
            playMovie()
        else if (isEpisode)
            playEpisode(episodes.indexOfExtId(vodExtId))
        else
            player.open(streamUrl)
        forceActiveFocus()
        showControls()
    }

    Component.onDestruction: {
        saveProgress()
        PictureInPicture.autoEnter = false
    }

    // Save the position regularly, so a crash or closed app loses little.
    Timer {
        interval: 15000
        running: page.isVod && player.playing
        repeat: true
        onTriggered: page.saveProgress()
    }

    // "Next episode" countdown
    Timer {
        id: nextEpisodeTimer
        interval: 1000
        repeat: true
        running: page.nextEpisodePending
        property int remaining: 10
        onRunningChanged: remaining = 10
        onTriggered: {
            if (--remaining <= 0)
                page.playEpisode(page.episodeIndex + 1)
        }
    }

    Timer {
        id: hideTimer
        interval: 3500
        onTriggered: {
            if (player.playing && !controls.interacting)
                page.controlsVisible = false
        }
    }

    Timer {
        interval: 30000
        running: page.currentEpgKey.length > 0
        repeat: true
        onTriggered: page.updateProgram()
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
        id: busy
        anchors.centerIn: parent
        running: player.busy
        visible: running
    }

    // A stream that was playing dropped; the player reconnects by itself.
    Rectangle {
        objectName: "reconnectingBanner"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: busy.bottom
        anchors.topMargin: 16
        visible: player.reconnecting && !page.pip
        width: reconnectLabel.implicitWidth + 32
        height: 36
        radius: height / 2
        color: "#B3000000"
        Label {
            id: reconnectLabel
            anchors.centerIn: parent
            color: AppTheme.textPrimary
            font.pixelSize: AppTheme.fontSmall
            text: player.reconnectAttempt > 1
                  ? qsTr("Connection lost, reconnecting (attempt %1 of %2)…")
                        .arg(player.reconnectAttempt).arg(player.maxReconnectAttempts)
                  : qsTr("Connection lost, reconnecting…")
        }
    }

    // Big play button while paused (easy to hit on touch screens)
    IconButton {
        anchors.centerIn: parent
        size: 76
        filled: true
        iconName: "play"
        iconColor: "white"
        tip: qsTr("Play")
        visible: !player.playing && !player.busy && !player.failed && !page.nextEpisodePending && !page.pip
        onClicked: {
            player.togglePause()
            page.showControls()
        }
    }

    // --- Errors: friendly message, automatic retries for live channels ---

    readonly property bool liveContext: !isVod && !catchup
    readonly property var retryDelays: [5, 10, 20]
    property int retryAttempt: 0
    property int retryCountdown: 0

    function resetRetries() {
        retryAttempt = 0
        retryCountdown = 0
    }

    Connections {
        target: player
        function onFailedChanged() {
            if (player.failed && page.liveContext && page.retryAttempt < page.retryDelays.length)
                page.retryCountdown = page.retryDelays[page.retryAttempt]
        }
        function onPlayingChanged() {
            if (player.playing)
                page.resetRetries()
        }
    }

    Timer {
        interval: 1000
        repeat: true
        running: player.failed && page.retryCountdown > 0
        onTriggered: {
            if (--page.retryCountdown <= 0) {
                page.retryAttempt++
                player.retry()
            }
        }
    }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(page.width - 2 * AppTheme.margin, 480)
        height: errorColumn.implicitHeight + 48
        visible: player.failed && !page.pip
        radius: AppTheme.radiusLarge
        color: "#F2141418"
        border.color: AppTheme.stroke

        ColumnLayout {
            id: errorColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 24
            spacing: 12

            RowLayout {
                spacing: 14
                Rectangle {
                    Layout.preferredWidth: 44
                    Layout.preferredHeight: 44
                    radius: 22
                    color: "#26FF3B4E"
                    IconImage {
                        anchors.centerIn: parent
                        source: page.liveContext ? "icons/live.svg" : "icons/film.svg"
                        sourceSize: Qt.size(22, 22)
                        color: AppTheme.live
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: page.liveContext ? qsTr("Channel unavailable") : qsTr("Can't play this video")
                    wrapMode: Text.Wrap
                    font.pixelSize: AppTheme.fontH2
                    font.weight: Font.Bold
                    color: AppTheme.textPrimary
                }
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: AppTheme.textSecondary
                lineHeight: 1.25
                text: page.liveContext
                      ? qsTr("This channel isn't sending a picture right now. It may be off air at "
                             + "the moment, or your provider has a problem with it.")
                      : qsTr("Your provider couldn't deliver this file. Try again in a moment.")
            }
            Label {
                Layout.fillWidth: true
                visible: page.liveContext
                color: page.retryCountdown > 0 ? AppTheme.textPrimary : AppTheme.textTertiary
                font.pixelSize: AppTheme.fontSmall
                text: page.retryCountdown > 0
                      ? qsTr("Trying again in %1 s… (attempt %2 of %3)")
                            .arg(page.retryCountdown).arg(page.retryAttempt + 1).arg(page.retryDelays.length)
                      : page.retryAttempt >= page.retryDelays.length
                        ? qsTr("Tried %1 times without success.").arg(page.retryDelays.length) : ""
            }
            Label {
                Layout.fillWidth: true
                visible: player.errorString.length > 0
                wrapMode: Text.Wrap
                color: AppTheme.textTertiary
                font.pixelSize: AppTheme.fontCaption
                text: qsTr("Details: %1").arg(player.errorString)
            }
            Flow {
                Layout.fillWidth: true
                Layout.topMargin: 6
                spacing: 10
                layoutDirection: Qt.RightToLeft
                AppButton {
                    text: qsTr("Try again")
                    iconName: "retry"
                    onClicked: {
                        page.resetRetries()
                        player.retry()
                    }
                }
                AppButton {
                    text: qsTr("Next channel")
                    iconName: "next"
                    visible: page.hasChannels
                    variant: "secondary"
                    onClicked: page.playChannel(page.channelIndex + 1)
                }
                AppButton {
                    text: qsTr("Back")
                    variant: "ghost"
                    onClicked: page.closeRequested()
                }
            }
        }
    }

    // Next episode prompt
    Pane {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: AppTheme.margin
        anchors.bottomMargin: 96
        visible: page.nextEpisodePending && !page.pip
        padding: AppTheme.margin
        background: Rectangle { color: AppTheme.surface; radius: AppTheme.radius }

        ColumnLayout {
            spacing: AppTheme.spacing
            Label {
                text: qsTr("Next episode in %1 s").arg(nextEpisodeTimer.remaining)
                color: AppTheme.textPrimary
                font.bold: true
            }
            Label {
                readonly property var next: page.nextEpisodePending ? episodes.get(page.episodeIndex + 1) : ({})
                text: next.season ? qsTr("S%1 E%2  %3").arg(next.season).arg(next.episode).arg(next.title || "") : ""
                color: AppTheme.textSecondary
                elide: Text.ElideRight
                Layout.maximumWidth: 320
            }
            RowLayout {
                AppButton {
                    text: qsTr("Cancel")
                    variant: "ghost"
                    onClicked: page.nextEpisodePending = false
                }
                AppButton {
                    text: qsTr("Play now")
                    iconName: "play"
                    onClicked: page.playEpisode(page.episodeIndex + 1)
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
        visible: page.infoVisible && !page.pip
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
        anchors.topMargin: 90
        text: page.program.now ? page.streamTitle + "\n" + page.program.now.title : page.streamTitle
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
        canPrevious: page.hasChannels || (page.isEpisode && page.episodeIndex > 0)
        canNext: page.hasChannels || (page.isEpisode && page.episodeIndex + 1 < episodes.count)
        favorite: page.currentFavorite
        program: page.program
        opacity: (page.controlsVisible || !player.playing) && !page.pip ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 200 } }

        onBackClicked: page.goBack()
        onFullScreenClicked: page.toggleFullScreen()
        onInfoClicked: page.infoVisible = !page.infoVisible
        onPreviousChannel: page.isEpisode ? page.playEpisode(page.episodeIndex - 1)
                                          : page.playChannel(page.channelIndex - 1)
        onNextChannel: page.isEpisode ? page.playEpisode(page.episodeIndex + 1)
                                      : page.playChannel(page.channelIndex + 1)
        onChannelListClicked: page.channelListVisible = !page.channelListVisible
        onMultiViewClicked: page.multiViewRequested(page.sourceId,
                                                    page.channelModel.get(page.channelIndex).extId)
        onFavoriteClicked: page.toggleFavorite()
        onActivity: page.showControls()
    }

    // Channel list panel
    Rectangle {
        id: channelPanel
        width: Math.min(380, page.width * 0.85)
        height: parent.height
        x: page.channelListVisible && !page.pip ? 0 : -width
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
            ScrollBar.vertical: AppScrollBar {}

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
        case Qt.Key_N:
            if (page.isEpisode) page.playEpisode(page.episodeIndex + 1)
            break
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
