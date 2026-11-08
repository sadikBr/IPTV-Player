import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import IPTVPlayer

// Cinematic home: a full-width rotating hero with backdrops, then rows.
Page {
    id: page

    // Keyboard / TV remote focus starts here.
    readonly property Item initialFocusItem: heroPlayButton.visible ? heroPlayButton : null

    signal playChannelRequested(int sourceId, string extId)
    signal playChannelIndexRequested(var model, int index)
    signal playVodRequested(int sourceId, string kind, string extId, string seriesExtId)
    signal openMovieRequested(string extId)
    signal openSeriesRequested(string extId)

    readonly property bool narrow: width < AppTheme.compactWidth
    readonly property real sideMargin: narrow ? AppTheme.margin : AppTheme.pageMargin

    // Hero slides: what you were watching, the newest movies and series.
    property var heroItems: []
    property int heroIndex: 0
    readonly property var hero: heroItems.length > 0 ? heroItems[Math.min(heroIndex, heroItems.length - 1)] : null
    property var requestedDetails: ({})
    // Artwork URLs that failed to load (or came back empty): their slides are
    // left out so the hero never shows an empty banner.
    property var failedArt: ({})

    function hasHeroArt(item) {
        return (item.backdrop.length > 0 && !failedArt[item.backdrop])
                || (item.poster.length > 0 && !failedArt[item.poster])
    }
    function markArtFailed(url) {
        if (!url || failedArt[url])
            return
        const failed = failedArt
        failed[url] = true
        failedArt = failed
        Qt.callLater(rebuildHero)
    }

    // "EN| Some Title" -> "Some Title" (provider language prefixes)
    function cleanTitle(title) {
        return (title || "").replace(/^\s*[\[(]?[A-Za-z]{2,3}[\])]?\s*[|:\-]\s*/, "")
                            .replace(/^\s*\[[A-Za-z]{2,3}\]\s*/, "")
    }

    // A few rows of a model, a different selection every day: the best are
    // shuffled with the date as seed so the banner changes but stays stable.
    function dailyPick(model, count) {
        const rows = []
        for (let i = 0; i < model.count; ++i)
            rows.push(i)
        let seed = Math.floor(Date.now() / 86400000)
        const random = () => {
            seed = (seed * 1103515245 + 12345) & 0x7fffffff
            return seed / 0x7fffffff
        }
        for (let i = rows.length - 1; i > 0; --i) {
            const j = Math.floor(random() * (i + 1))
            const t = rows[i]; rows[i] = rows[j]; rows[j] = t
        }
        return rows.slice(0, count).sort((a, b) => a - b)
    }

    function metaLine(parts) {
        return parts.filter(t => t && String(t).length > 0).join("  ·  ")
    }

    function rebuildHero() {
        const items = []
        const sourceId = AppState.sourceId
        if (sourceId <= 0) {
            heroItems = []
            return
        }

        if (continueWatching.count > 0) {
            const c = continueWatching.get(0)
            if (c.sourceId === sourceId) {
                const info = c.kind === "movie" ? VodManager.movie(sourceId, c.extId)
                                                : VodManager.series(sourceId, c.seriesExtId)
                items.push({
                    type: "continue", kind: c.kind, extId: c.extId, seriesExtId: c.seriesExtId,
                    eyebrow: qsTr("Continue watching"), title: cleanTitle(c.title),
                    meta: metaLine([c.subtitle, qsTr("%1 min left").arg(c.remainingMinutes)]),
                    plot: info.plot || "", backdrop: info.backdrop || "", poster: info.poster || c.image,
                    progress: c.watched, action: qsTr("Resume")
                })
                requestDetails(c.kind === "movie" ? "movie" : "series",
                               c.kind === "movie" ? c.extId : c.seriesExtId, info)
            }
        }
        // Fall back to the newest when the provider gives no ratings.
        const movies = featuredMovies.count > 0 && featuredMovies.get(0).rating ? featuredMovies : newMovies
        const series = featuredSeries.count > 0 && featuredSeries.get(0).rating ? featuredSeries : newSeries
        for (const i of dailyPick(movies, 5)) {
            const m = movies.get(i)
            if (m.watched > 0.9)
                continue
            const info = VodManager.movie(sourceId, m.extId)
            items.push({
                type: "movie", kind: "movie", extId: m.extId,
                eyebrow: movies === featuredMovies ? qsTr("Featured movie") : qsTr("New movie"),
                title: cleanTitle(info.name || m.name),
                meta: metaLine([info.year || m.year, info.rating ? "★ " + info.rating : "",
                                AppTheme.duration(info.durationSecs), info.genre]),
                plot: info.plot || "", backdrop: info.backdrop || "", poster: info.poster || m.logo,
                progress: m.watched, action: m.watched > 0 ? qsTr("Resume") : qsTr("Play")
            })
            requestDetails("movie", m.extId, info)
        }
        for (const j of dailyPick(series, 3)) {
            const s = series.get(j)
            const info = VodManager.series(sourceId, s.extId)
            items.push({
                type: "series", kind: "series", extId: s.extId,
                eyebrow: series === featuredSeries ? qsTr("Featured series") : qsTr("New series"),
                title: cleanTitle(info.name || s.name),
                meta: metaLine([info.year || s.year, info.rating ? "★ " + info.rating : "", info.genre]),
                plot: info.plot || "", backdrop: info.backdrop || "", poster: info.poster || s.logo,
                progress: 0, action: qsTr("Watch")
            })
            requestDetails("series", s.extId, info)
        }
        heroItems = items.filter(hasHeroArt)
        if (heroIndex >= heroItems.length)
            heroIndex = 0
    }

    // Backdrops come with the details; fetch them once per title.
    function requestDetails(kind, extId, info) {
        if (info.backdrop || requestedDetails[kind + extId])
            return
        requestedDetails[kind + extId] = true
        if (kind === "movie")
            VodManager.fetchMovie(AppState.sourceId, extId)
        else
            VodManager.fetchSeries(AppState.sourceId, extId)
    }

    function heroPlay(item) {
        if (!item)
            return
        if (item.type === "continue")
            playVodRequested(AppState.sourceId, item.kind, item.extId, item.seriesExtId)
        else if (item.type === "movie")
            playVodRequested(AppState.sourceId, "movie", item.extId, "")
        else
            openSeriesRequested(item.extId)
    }

    function heroDetails(item) {
        if (!item)
            return
        if (item.kind === "movie")
            openMovieRequested(item.extId)
        else
            openSeriesRequested(item.type === "continue" ? item.seriesExtId : item.extId)
    }

    background: Rectangle { color: AppTheme.background }

    ContinueWatchingModel {
        id: continueWatching
        onModelReset: rebuildTimer.restart()
    }
    ChannelListModel {
        id: favoriteChannels
        sourceId: AppState.sourceId
        categoryId: CategoryListModel.FavoritesCategory
    }
    ChannelListModel {
        id: liveChannels
        sourceId: AppState.sourceId
        limit: 20
    }
    ChannelListModel {
        id: newMovies
        sourceId: AppState.sourceId
        kind: "movie"
        sortOrder: "newest"
        limit: 24
        onModelReset: rebuildTimer.restart()
    }
    ChannelListModel {
        id: newSeries
        sourceId: AppState.sourceId
        kind: "series"
        sortOrder: "newest"
        limit: 24
        onModelReset: rebuildTimer.restart()
    }
    // The banner: well rated, recent titles (not just the newest uploads)
    ChannelListModel {
        id: featuredMovies
        sourceId: AppState.sourceId
        kind: "movie"
        sortOrder: "featured"
        limit: 25
        onModelReset: rebuildTimer.restart()
    }
    ChannelListModel {
        id: featuredSeries
        sourceId: AppState.sourceId
        kind: "series"
        sortOrder: "featured"
        limit: 25
        onModelReset: rebuildTimer.restart()
    }
    ChannelListModel {
        id: favoriteMovies
        sourceId: AppState.sourceId
        kind: "movie"
        categoryId: CategoryListModel.FavoritesCategory
    }

    // Coalesce rebuilds (several models reset at once).
    Timer {
        id: rebuildTimer
        interval: 50
        onTriggered: page.rebuildHero()
    }

    Connections {
        target: VodManager
        function onDetailsLoaded() { rebuildTimer.restart() }
        function onSeriesLoaded() { rebuildTimer.restart() }
    }
    Connections {
        target: AppState
        function onSourceIdChanged() {
            page.requestedDetails = ({})
            rebuildTimer.restart()
        }
    }

    Component.onCompleted: rebuildHero()

    // Next slide every 8 s (not while the mouse is on the hero).
    Timer {
        interval: 8000
        repeat: true
        running: page.heroItems.length > 1 && !heroHover.hovered && page.visible
        onTriggered: page.heroIndex = (page.heroIndex + 1) % page.heroItems.length
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentHeight: content.implicitHeight + 48
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: AppScrollBar {}

        ColumnLayout {
            id: content
            width: flick.width
            spacing: 36

            // Without a banner, the search / provider buttons get a bar of their own.
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 64
                Layout.bottomMargin: -28
                visible: AppState.hasSource && !heroArea.visible && page.width < AppTheme.wideWidth
                Loader {
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    sourceComponent: parent.visible ? cornerButtons : null
                }
            }

            // ---------- Hero ----------
            Item {
                id: heroArea
                Layout.fillWidth: true
                Layout.preferredHeight: page.narrow ? Math.max(420, page.height * 0.62)
                                                    : Math.max(460, Math.min(page.height * 0.72, 640))
                visible: AppState.hasSource && page.heroItems.length > 0

                HoverHandler { id: heroHover }

                // Backdrops (cross-fade between slides)
                Repeater {
                    model: page.heroItems
                    delegate: Item {
                        id: slide
                        required property var modelData
                        required property int index
                        // Backdrop if it loads, else the poster.
                        readonly property bool backdropOk: modelData.backdrop.length > 0
                                                           && !page.failedArt[modelData.backdrop]
                        readonly property bool posterOk: modelData.poster.length > 0
                                                         && !page.failedArt[modelData.poster]
                        function check(image, url) {
                            if (image.status === Image.Error
                                    || (image.status === Image.Ready && image.implicitWidth <= 4))
                                page.markArtFailed(url)
                        }
                        anchors.fill: parent
                        opacity: index === page.heroIndex ? 1 : 0
                        visible: opacity > 0
                        Behavior on opacity { NumberAnimation { duration: AppTheme.slow; easing.type: Easing.InOutQuad } }

                        Image {
                            id: heroBackdrop
                            anchors.fill: parent
                            source: slide.backdropOk ? slide.modelData.backdrop : ""
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            sourceSize.width: 1600
                            visible: slide.backdropOk && AppTheme.hasArtwork(heroBackdrop)
                            onStatusChanged: slide.check(heroBackdrop, slide.modelData.backdrop)
                        }
                        // No (working) backdrop: the poster large on the right
                        Item {
                            visible: !slide.backdropOk && slide.posterOk
                            anchors.fill: parent
                            Image {
                                id: blurredPoster
                                anchors.fill: parent
                                source: modelData.poster
                                fillMode: Image.PreserveAspectCrop
                                sourceSize.width: 48 // tiny on purpose: looks blurred when stretched
                                smooth: true
                                opacity: 0.35
                            }
                            Shadow { target: posterCard; spread: 30 }
                            Rectangle {
                                id: posterCard
                                visible: !page.narrow
                                anchors.right: parent.right
                                anchors.rightMargin: page.sideMargin + 20
                                anchors.verticalCenter: parent.verticalCenter
                                height: parent.height * 0.72
                                width: height / 1.5
                                radius: AppTheme.radius
                                color: AppTheme.surface
                                clip: true
                                Image {
                                    id: heroPoster
                                    anchors.fill: parent
                                    source: slide.posterOk ? slide.modelData.poster : ""
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    sourceSize.width: 500
                                    onStatusChanged: slide.check(heroPoster, slide.modelData.poster)
                                }
                            }
                        }
                    }
                }

                // Shade so the text stays readable
                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: "#F00B0B0D" }
                        GradientStop { position: 0.45; color: "#990B0B0D" }
                        GradientStop { position: 1.0; color: "#000B0B0D" }
                    }
                }
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: parent.height * 0.5
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#000B0B0D" }
                        GradientStop { position: 1.0; color: AppTheme.background }
                    }
                }
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 90
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#990B0B0D" }
                        GradientStop { position: 1.0; color: "#000B0B0D" }
                    }
                }

                // Text and actions
                ColumnLayout {
                    id: heroText
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: page.sideMargin
                    anchors.bottomMargin: page.narrow ? 36 : 64
                    width: Math.min(parent.width - 2 * page.sideMargin, 620)
                    spacing: 12

                    Label {
                        text: page.hero ? page.hero.eyebrow.toUpperCase() : ""
                        color: AppTheme.accent
                        font.pixelSize: AppTheme.fontCaption
                        font.weight: Font.Bold
                        font.letterSpacing: 2
                    }
                    Label {
                        Layout.fillWidth: true
                        text: page.hero ? page.hero.title : ""
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        font.pixelSize: page.narrow ? 30 : AppTheme.fontHero
                        font.weight: Font.Bold
                        font.letterSpacing: -0.5
                        lineHeight: 1.05
                        color: "white"
                    }
                    Label {
                        visible: text.length > 0
                        text: page.hero ? page.hero.meta : ""
                        color: "#D6D6DB"
                        font.pixelSize: AppTheme.fontBody
                        font.weight: Font.Medium
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: text.length > 0 && !page.narrow
                        text: page.hero ? page.hero.plot : ""
                        wrapMode: Text.Wrap
                        maximumLineCount: 3
                        elide: Text.ElideRight
                        color: "#B8B8C0"
                        lineHeight: 1.25
                    }
                    Rectangle {
                        visible: page.hero && page.hero.progress > 0
                        Layout.preferredWidth: 240
                        Layout.preferredHeight: 4
                        radius: 2
                        color: "#40FFFFFF"
                        Rectangle {
                            width: parent.width * (page.hero ? Math.min(1, page.hero.progress) : 0)
                            height: parent.height
                            radius: 2
                            color: AppTheme.accent
                        }
                    }
                    RowLayout {
                        Layout.topMargin: 8
                        spacing: 12
                        AppButton {
                            id: heroPlayButton
                            text: page.hero ? page.hero.action : ""
                            iconName: "play"
                            onClicked: page.heroPlay(page.hero)
                        }
                        AppButton {
                            variant: "secondary"
                            text: qsTr("Details")
                            iconName: "info"
                            onClicked: page.heroDetails(page.hero)
                        }
                    }
                }

                // Slide dots
                Row {
                    // Phones: centred under the buttons; wide: bottom right.
                    x: page.narrow ? (parent.width - width) / 2 : parent.width - width - page.sideMargin
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: page.narrow ? 12 : 76
                    spacing: 8
                    visible: page.heroItems.length > 1
                    Repeater {
                        model: page.heroItems.length
                        delegate: Rectangle {
                            required property int index
                            width: index === page.heroIndex ? 26 : 8
                            height: 8
                            radius: 4
                            color: index === page.heroIndex ? "white" : "#66FFFFFF"
                            Behavior on width { NumberAnimation { duration: AppTheme.normal } }
                            TapHandler { onTapped: page.heroIndex = parent.index }
                        }
                    }
                }

                Loader {
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.margins: 12
                    sourceComponent: cornerButtons
                }
            }

            // ---------- Welcome (no provider yet) ----------
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(460, page.height * 0.7)
                visible: !AppState.hasSource

                // Warm glow
                Rectangle {
                    width: parent.width * 1.2
                    height: parent.height * 1.4
                    x: parent.width * 0.35
                    y: -parent.height * 0.5
                    rotation: -18
                    opacity: 0.22
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: "transparent" }
                        GradientStop { position: 0.5; color: AppTheme.accent }
                        GradientStop { position: 1.0; color: "transparent" }
                    }
                }

                ColumnLayout {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: page.sideMargin
                    width: Math.min(parent.width - 2 * page.sideMargin, 600)
                    spacing: 16
                    Label {
                        text: qsTr("WELCOME")
                        color: AppTheme.accent
                        font.pixelSize: AppTheme.fontCaption
                        font.weight: Font.Bold
                        font.letterSpacing: 2
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("All your TV, movies and series in one place.")
                        wrapMode: Text.Wrap
                        font.pixelSize: page.narrow ? 32 : AppTheme.fontHero
                        font.weight: Font.Bold
                        font.letterSpacing: -0.5
                        lineHeight: 1.05
                        color: "white"
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Add your IPTV provider with an Xtream Codes login or an M3U playlist "
                                   + "link, or start with thousands of free channels. Live TV, the TV "
                                   + "guide, movies and series appear here. And there's radio from all "
                                   + "over the world.")
                        wrapMode: Text.Wrap
                        color: AppTheme.textSecondary
                        font.pixelSize: AppTheme.fontH3
                        lineHeight: 1.3
                    }
                    Flow {
                        Layout.fillWidth: true
                        Layout.topMargin: 12
                        spacing: 12
                        AppButton {
                            text: qsTr("Add your provider")
                            iconName: "add"
                            onClicked: AppState.addProviderRequested()
                        }
                        AppButton {
                            variant: "secondary"
                            text: qsTr("Listen to the radio")
                            iconName: "radio"
                            onClicked: AppState.sectionRequested("radio")
                        }
                    }
                }
            }

            // ---------- Rows ----------
            ContentRow {
                Layout.fillWidth: true
                Layout.topMargin: heroArea.visible ? -28 : 16
                sideMargin: page.sideMargin
                title: qsTr("Continue watching")
                model: continueWatching
                rowHeight: page.narrow ? 236 : 300
                delegate: continueDelegate
            }

            ContentRow {
                Layout.fillWidth: true
                sideMargin: page.sideMargin
                title: favoriteChannels.count > 0 ? qsTr("Your channels") : qsTr("Live TV")
                showSeeAll: true
                onSeeAllClicked: AppState.sectionRequested("live")
                model: favoriteChannels.count > 0 ? favoriteChannels : liveChannels
                rowHeight: 196
                delegate: channelDelegate
            }

            ContentRow {
                Layout.fillWidth: true
                sideMargin: page.sideMargin
                title: qsTr("My movies")
                model: favoriteMovies
                rowHeight: page.narrow ? 236 : 316
                delegate: movieDelegate
            }

            ContentRow {
                Layout.fillWidth: true
                sideMargin: page.sideMargin
                title: qsTr("New movies")
                showSeeAll: true
                onSeeAllClicked: AppState.sectionRequested("movie")
                model: newMovies
                rowHeight: page.narrow ? 236 : 316
                delegate: movieDelegate
            }

            ContentRow {
                Layout.fillWidth: true
                sideMargin: page.sideMargin
                title: qsTr("New series")
                showSeeAll: true
                onSeeAllClicked: AppState.sectionRequested("series")
                model: newSeries
                rowHeight: page.narrow ? 236 : 316
                delegate: seriesDelegate
            }

            // Provider content still loading
            EmptyState {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 40
                visible: AppState.hasSource && page.heroItems.length === 0 && liveChannels.count === 0
                busy: SourceManager.busy
                iconName: "live"
                title: SourceManager.busy ? qsTr("Loading your channels…") : qsTr("Nothing here yet")
                text: SourceManager.busy ? SourceManager.statusText
                                         : qsTr("Update the provider in Settings to load its channels.")
            }
        }
    }

    // Phones (no side bar): search everything from Home

    // Delegates wrap the cards in a plain Item with typed properties named
    // after the model roles. (Re-declaring the cards' own properties as
    // "required" crashed some versions of Qt's QML compiler.)

    // Phones and tablets (no side bar): search, and the provider switch on phones.
    // Part of the page content, so they scroll away instead of covering the rows.
    Component {
        id: cornerButtons
        Row {
            spacing: 8
            IconButton {
                anchors.verticalCenter: parent.verticalCenter
                visible: page.width < AppTheme.wideWidth
                filled: true
                iconName: "search"
                tip: qsTr("Search")
                onClicked: AppState.sectionRequested("search")
            }
            ProviderSwitcher {
                anchors.verticalCenter: parent.verticalCenter
                visible: page.narrow
                compact: true
            }
        }
    }

    Component {
        id: continueDelegate
        Item {
            id: cw
            required property int sourceId
            required property string kind
            required property string extId
            required property string seriesExtId
            required property string title
            required property string subtitle
            required property string image
            required property real watched
            required property int remainingMinutes
            width: page.narrow ? 124 : 164
            height: ListView.view ? ListView.view.height : 300
            PosterCard {
                anchors.fill: parent
                title: cw.title
                poster: cw.image
                watched: cw.watched
                subtitle: page.metaLine([cw.subtitle, qsTr("%1 min left").arg(cw.remainingMinutes)])
                onClicked: page.playVodRequested(cw.sourceId, cw.kind, cw.extId, cw.seriesExtId)
            }
        }
    }

    Component {
        id: channelDelegate
        Item {
            id: ch
            required property int index
            required property string name
            required property string logo
            required property string nowTitle
            required property real nowProgress
            width: 230
            height: 180
            ChannelCard {
                anchors.fill: parent
                anchors.bottomMargin: 10
                name: ch.name
                logo: ch.logo
                nowTitle: ch.nowTitle
                nowProgress: ch.nowProgress
                onClicked: page.playChannelIndexRequested(
                               favoriteChannels.count > 0 ? favoriteChannels : liveChannels, ch.index)
            }
        }
    }

    Component {
        id: movieDelegate
        Item {
            id: mv
            required property string extId
            required property string name
            required property string logo
            required property string year
            required property string rating
            required property real watched
            required property bool favorite
            width: page.narrow ? 124 : 172
            height: ListView.view ? ListView.view.height : 316
            PosterCard {
                anchors.fill: parent
                title: mv.name
                poster: mv.logo
                subtitle: mv.year
                rating: mv.rating
                watched: mv.watched
                favorite: mv.favorite
                onClicked: page.openMovieRequested(mv.extId)
            }
        }
    }

    Component {
        id: seriesDelegate
        Item {
            id: sr
            required property string extId
            required property string name
            required property string logo
            required property string year
            required property string rating
            required property bool favorite
            width: page.narrow ? 124 : 172
            height: ListView.view ? ListView.view.height : 316
            PosterCard {
                anchors.fill: parent
                title: sr.name
                poster: sr.logo
                subtitle: sr.year
                rating: sr.rating
                favorite: sr.favorite
                onClicked: page.openSeriesRequested(sr.extId)
            }
        }
    }
}
