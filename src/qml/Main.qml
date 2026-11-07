import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import IPTVPlayer

ApplicationWindow {
    id: window

    width: 1100
    height: 700
    minimumWidth: 320
    minimumHeight: 240
    visible: true
    title: qsTr("IPTV Player")
    color: AppTheme.background

    Material.theme: Material.Dark
    Material.accent: AppTheme.accent
    Material.primary: AppTheme.surface

    readonly property bool fullScreen: visibility === Window.FullScreen
    property int visibilityBeforeFullScreen: Window.Windowed

    function setFullScreen(on) {
        if (on === fullScreen)
            return
        if (on) {
            visibilityBeforeFullScreen = visibility
            showFullScreen()
        } else if (visibilityBeforeFullScreen === Window.Maximized) {
            showMaximized()
        } else {
            showNormal()
        }
    }

    function toggleFullScreen() {
        setFullScreen(!fullScreen)
    }

    // Plays a single URL (no channel list).
    function openStream(url, title) {
        stack.push(playerPageComponent, { "streamUrl": url, "streamTitle": title || "" })
    }

    // Shows the channel browser of a source; optionally starts a channel.
    function openSource(sourceId, autoPlayExtId) {
        AppSettings.lastSourceId = sourceId
        stack.pop(null) // back to home
        stack.push(livePageComponent, { "sourceId": sourceId,
                                        "autoPlayExtId": autoPlayExtId || "" })
    }

    function playChannel(channelModel, sourceId, index) {
        stack.push(playerPageComponent, { "channelModel": channelModel,
                                          "channelIndex": index,
                                          "sourceId": sourceId })
    }

    // Switches between Live TV, Movies and Series of the source on screen.
    function openSection(sourceId, kind) {
        if (kind === "live")
            stack.replace(livePageComponent, { "sourceId": sourceId })
        else
            stack.replace(vodPageComponent, { "sourceId": sourceId, "kind": kind })
    }

    function openMovie(sourceId, extId) {
        stack.push(movieDetailsComponent, { "sourceId": sourceId, "extId": extId })
    }

    function openSeries(sourceId, extId) {
        stack.push(seriesDetailsComponent, { "sourceId": sourceId, "extId": extId })
    }

    function playMovie(sourceId, extId, fromStart) {
        stack.push(playerPageComponent, { "sourceId": sourceId, "vodKind": "movie",
                                          "vodExtId": extId, "startFromBeginning": !!fromStart })
    }

    function playEpisode(sourceId, seriesExtId, episodeExtId, fromStart) {
        stack.push(playerPageComponent, { "sourceId": sourceId, "vodKind": "episode",
                                          "seriesExtId": seriesExtId, "vodExtId": episodeExtId,
                                          "startFromBeginning": !!fromStart })
    }

    function openGuide(livePage) {
        stack.push(guidePageComponent, { "channelModel": livePage.channelModel,
                                         "sourceId": livePage.sourceId })
    }

    // Used by the --smoke-test mode (see main.cpp).
    function smokeAddPlaylist(url) {
        SourceManager.addM3uSource("Smoke test", url, "")
    }
    function smokeShowAddPage() {
        stack.push(addSourcePageComponent)
    }
    function smokeCloseAddPage() {
        stack.pop(null)
    }
    function smokeAddXtream(server, username, password) {
        SourceManager.addXtreamSource("Smoke test", server, username, password, "ts")
    }
    function smokeOpenGuide() {
        if (stack.currentItem && stack.currentItem.guideRequested)
            stack.currentItem.guideRequested()
    }
    function smokeOpenSection(kind) {
        const current = stack.currentItem
        if (current && current.sourceId > 0)
            openSection(current.sourceId, kind)
    }
    function smokeOpenFirst() {
        if (stack.currentItem && stack.currentItem.openFirst)
            stack.currentItem.openFirst()
    }
    function smokePlay() {
        const current = stack.currentItem
        if (!current)
            return
        if (current.extId === undefined)
            return // not a movie or series page
        if (current.playRequested)
            current.playRequested(false)
        else if (current.episodeRequested && current.resumeEpisode)
            current.episodeRequested(current.resumeEpisode.extId, false)
    }
    function smokeBack() {
        if (stack.depth > 1)
            stack.pop()
    }
    function smokePlayFirst() {
        if (stack.currentItem && stack.currentItem.backRequested && !stack.currentItem.playIndex)
            stack.pop() // leave the guide
        if (stack.currentItem && stack.currentItem.playIndex)
            stack.currentItem.playIndex(0)
    }

    // Android's back button closes the window when nothing handles it:
    // go back one screen instead (the player first closes its own panels).
    onClosing: (close) => {
        if (Qt.platform.os !== "android" || stack.depth <= 1)
            return
        close.accepted = false
        if (stack.currentItem.goBack)
            stack.currentItem.goBack()
        else
            stack.pop()
    }

    Connections {
        target: SourceManager
        // Open a source as soon as it has been added (from the form or the smoke test).
        function onFinished(id, ok, error, isNew) {
            if (ok && isNew)
                window.openSource(id)
            if (ok)
                EpgManager.refreshIfStale(id, 12)
        }
    }

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: HomePage {
            onPlayRequested: (url, title) => window.openStream(url, title)
            onSourceSelected: (sourceId) => window.openSource(sourceId)
            onAddSourceRequested: stack.push(addSourcePageComponent)
            onResumeRequested: (sourceId, extId) => window.openSource(sourceId, extId)
            onResumeVodRequested: (sourceId, kind, extId, seriesExtId) => kind === "movie"
                                  ? window.playMovie(sourceId, extId, false)
                                  : window.playEpisode(sourceId, seriesExtId, extId, false)
        }

        Component.onCompleted: {
            if (SourceManager.sourceExists(AppSettings.lastSourceId))
                window.openSource(AppSettings.lastSourceId)
        }
    }

    Component {
        id: livePageComponent
        LivePage {
            id: livePage
            onBackRequested: stack.pop()
            onChannelActivated: (index) => window.playChannel(livePage.channelModel,
                                                              livePage.sourceId, index)
            onGuideRequested: window.openGuide(livePage)
            onSectionRequested: (kind) => window.openSection(livePage.sourceId, kind)
        }
    }

    Component {
        id: vodPageComponent
        VodPage {
            id: vodPage
            onBackRequested: stack.pop()
            onSectionRequested: (kind) => window.openSection(vodPage.sourceId, kind)
            onItemActivated: (extId) => vodPage.kind === "movie"
                             ? window.openMovie(vodPage.sourceId, extId)
                             : window.openSeries(vodPage.sourceId, extId)
        }
    }

    Component {
        id: movieDetailsComponent
        MovieDetailsPage {
            id: moviePage
            onBackRequested: stack.pop()
            onPlayRequested: (fromStart) => window.playMovie(moviePage.sourceId, moviePage.extId, fromStart)
        }
    }

    Component {
        id: seriesDetailsComponent
        SeriesDetailsPage {
            id: seriesPage
            onBackRequested: stack.pop()
            onEpisodeRequested: (episodeExtId, fromStart) =>
                window.playEpisode(seriesPage.sourceId, seriesPage.extId, episodeExtId, fromStart)
        }
    }

    Component {
        id: guidePageComponent
        GuidePage {
            id: guidePage
            onBackRequested: stack.pop()
            onChannelActivated: (index) => window.playChannel(guidePage.channelModel,
                                                              guidePage.sourceId, index)
        }
    }

    Component {
        id: addSourcePageComponent
        AddSourcePage {
            onBackRequested: stack.pop()
        }
    }

    Component {
        id: playerPageComponent
        PlayerPage {
            appWindow: window
            onCloseRequested: {
                window.setFullScreen(false)
                stack.pop()
            }
        }
    }
}
