import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import IPTVPlayer

ApplicationWindow {
    id: window

    width: 1280
    height: 800
    minimumWidth: 320
    minimumHeight: 240
    visible: true
    title: qsTr("IPTV Player")
    color: AppTheme.background

    font.family: AppTheme.fontFamily
    font.pixelSize: AppTheme.fontBody

    Material.theme: Material.Dark
    Material.accent: AppTheme.accent
    Material.primary: AppTheme.surface
    Material.background: AppTheme.surface
    Material.foreground: AppTheme.textPrimary

    readonly property bool wide: width >= AppTheme.wideWidth
    readonly property bool fullScreen: visibility === Window.FullScreen
    property int visibilityBeforeFullScreen: Window.Windowed
    // Pages like the player hide the navigation.
    readonly property bool chromeVisible: !(stack.currentItem && stack.currentItem.immersive === true)

    FontLoader { source: "fonts/Inter-Regular.ttf" }
    FontLoader { source: "fonts/Inter-Medium.ttf" }
    FontLoader { source: "fonts/Inter-SemiBold.ttf" }
    FontLoader { source: "fonts/Inter-Bold.ttf" }

    // --- Window helpers ---

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

    // --- Navigation ---

    function sectionComponent(name) {
        switch (name) {
        case "live": return liveComponent
        case "movie": return movieComponent
        case "series": return seriesComponent
        case "guide": return guideComponent
        case "settings": return settingsComponent
        default: return homeComponent
        }
    }

    // Shows a section, replacing everything on the stack.
    function goSection(name) {
        if (name !== "home" && name !== "settings" && !AppState.hasSource)
            name = "home"
        AppState.section = name
        stack.replace(null, sectionComponent(name), {}, StackView.Immediate)
    }

    // Back one screen; returns false when there is nothing to go back to.
    function goBack() {
        if (stack.depth > 1) {
            if (stack.currentItem.goBack)
                stack.currentItem.goBack()
            else
                stack.pop()
        } else if (AppState.section !== "home") {
            goSection("home")
        } else {
            return false
        }
        return true
    }

    function openStream(url, title) {
        stack.push(playerComponent, { "streamUrl": url, "streamTitle": title || "" })
    }

    function playChannel(channelModel, sourceId, index) {
        stack.push(playerComponent, { "channelModel": channelModel, "channelIndex": index,
                                      "sourceId": sourceId })
    }

    // Opens Live TV and starts a channel there (so channel up/down works).
    function playChannelByExtId(sourceId, extId) {
        if (sourceId !== AppState.sourceId)
            AppState.setSource(sourceId)
        goSection("live")
        stack.currentItem.playExtId(extId)
    }

    function openGuide(channelModel) {
        stack.push(guideComponent, { "channelModel": channelModel, "showBack": true })
    }

    function openMovie(sourceId, extId) {
        stack.push(movieDetailsComponent, { "sourceId": sourceId, "extId": extId })
    }

    function openSeries(sourceId, extId) {
        stack.push(seriesDetailsComponent, { "sourceId": sourceId, "extId": extId })
    }

    function playMovie(sourceId, extId, fromStart) {
        stack.push(playerComponent, { "sourceId": sourceId, "vodKind": "movie",
                                      "vodExtId": extId, "startFromBeginning": !!fromStart })
    }

    function playEpisode(sourceId, seriesExtId, episodeExtId, fromStart) {
        stack.push(playerComponent, { "sourceId": sourceId, "vodKind": "episode",
                                      "seriesExtId": seriesExtId, "vodExtId": episodeExtId,
                                      "startFromBeginning": !!fromStart })
    }

    function addProvider() {
        stack.push(addSourceComponent)
    }

    // --- Smoke test hooks (see main.cpp) ---

    function smokeShowAddPage() { addProvider() }
    function smokeCloseAddPage() { goSection("home") }
    function smokeAddPlaylist(url) { SourceManager.addM3uSource("Smoke test", url, "") }
    function smokeAddXtream(server, username, password) {
        SourceManager.addXtreamSource("Smoke test", server, username, password, "ts")
    }
    function smokeOpenGuide() { goSection("guide") }
    function smokePlayFirst() {
        goSection("live")
        stack.currentItem.playIndex(0)
    }
    function smokeOpenSection(kind) { goSection(kind) }
    function smokeOpenFirst() {
        if (stack.currentItem && stack.currentItem.openFirst)
            stack.currentItem.openFirst()
    }
    function smokePlay() {
        const current = stack.currentItem
        if (!current || current.extId === undefined)
            return // not a movie or series page
        if (current.playRequested)
            current.playRequested(false)
        else if (current.episodeRequested && current.resumeEpisode)
            current.episodeRequested(current.resumeEpisode.extId, false)
    }
    function smokeBack() { goBack() }

    // --- Wiring ---

    Connections {
        target: AppState
        function onSectionRequested(name) { window.goSection(name) }
        function onAddProviderRequested() { window.addProvider() }
        function onPlayStreamRequested(url, title) { window.openStream(url, title) }
        function onSourceIdChanged() {
            // Switching provider: show the same section for the new one.
            if (stack.depth > 0 && AppState.section !== "home" && AppState.section !== "settings")
                window.goSection(AppState.section)
        }
    }

    Connections {
        target: SourceManager
        function onFinished(id, ok, error, isNew) {
            if (ok && isNew) {
                AppState.setSource(id)
                window.goSection("live")
            }
            if (ok)
                EpgManager.refreshIfStale(id, 12)
        }
        function onSourcesChanged() {
            // The current provider was removed: fall back to another one.
            if (AppState.sourceId > 0 && !SourceManager.sourceExists(AppState.sourceId))
                AppState.ensureSource(sources.sourceIdAt(0))
        }
    }

    SourceListModel {
        id: sources
    }

    // Android's back button closes the window when nothing handles it:
    // go back one screen instead.
    onClosing: (close) => {
        if (Qt.platform.os === "android" && goBack())
            close.accepted = false
    }

    Component.onCompleted: {
        AppState.ensureSource(sources.sourceIdAt(0))
        goSection("home")
    }

    // --- Layout ---

    NavRail {
        id: rail
        width: 240
        height: parent.height
        visible: window.wide && window.chromeVisible
    }

    StackView {
        id: stack
        anchors.left: rail.visible ? rail.right : parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: bottomNav.visible ? bottomNav.top : parent.bottom

        pushEnter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: AppTheme.normal }
                NumberAnimation { property: "y"; from: 24; to: 0; duration: AppTheme.normal; easing.type: Easing.OutCubic }
            }
        }
        pushExit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: AppTheme.normal }
        }
        popEnter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: AppTheme.normal }
        }
        popExit: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 1; to: 0; duration: AppTheme.normal }
                NumberAnimation { property: "y"; from: 0; to: 24; duration: AppTheme.normal; easing.type: Easing.InCubic }
            }
        }
    }

    BottomNav {
        id: bottomNav
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: !window.wide && window.chromeVisible
    }

    // --- Pages ---

    Component {
        id: homeComponent
        HomePage {
            onPlayChannelRequested: (sourceId, extId) => window.playChannelByExtId(sourceId, extId)
            onPlayVodRequested: (sourceId, kind, extId, seriesExtId) => kind === "movie"
                                ? window.playMovie(sourceId, extId, false)
                                : window.playEpisode(sourceId, seriesExtId, extId, false)
            onOpenMovieRequested: (extId) => window.openMovie(AppState.sourceId, extId)
            onOpenSeriesRequested: (extId) => window.openSeries(AppState.sourceId, extId)
            onPlayChannelIndexRequested: (model, index) => window.playChannel(model, AppState.sourceId, index)
        }
    }

    Component {
        id: liveComponent
        LivePage {
            id: livePage
            sourceId: AppState.sourceId
            onChannelActivated: (index) => window.playChannel(livePage.channelModel,
                                                              livePage.sourceId, index)
            onGuideRequested: window.openGuide(livePage.channelModel)
        }
    }

    Component {
        id: movieComponent
        VodPage {
            kind: "movie"
            sourceId: AppState.sourceId
            onItemActivated: (extId) => window.openMovie(AppState.sourceId, extId)
        }
    }

    Component {
        id: seriesComponent
        VodPage {
            kind: "series"
            sourceId: AppState.sourceId
            onItemActivated: (extId) => window.openSeries(AppState.sourceId, extId)
        }
    }

    Component {
        id: guideComponent
        GuidePage {
            id: guidePage
            sourceId: AppState.sourceId
            onBackRequested: stack.pop()
            onChannelActivated: (index) => window.playChannel(guidePage.activeModel,
                                                              guidePage.sourceId, index)
        }
    }

    Component {
        id: settingsComponent
        SettingsPage {}
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
        id: addSourceComponent
        AddSourcePage {
            onBackRequested: stack.pop()
        }
    }

    Component {
        id: playerComponent
        PlayerPage {
            appWindow: window
            onCloseRequested: {
                window.setFullScreen(false)
                stack.pop()
            }
        }
    }
}
