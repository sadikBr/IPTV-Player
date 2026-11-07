import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import IPTVPlayer

// Dashboard: a hero for what to watch next, then rows of content.
Page {
    id: page

    signal playChannelRequested(int sourceId, string extId)
    signal playChannelIndexRequested(var model, int index)
    signal playVodRequested(int sourceId, string kind, string extId, string seriesExtId)
    signal openMovieRequested(string extId)
    signal openSeriesRequested(string extId)

    readonly property bool narrow: width < AppTheme.compactWidth
    readonly property real sideMargin: narrow ? AppTheme.margin : AppTheme.pageMargin
    readonly property bool hasLastChannel: AppSettings.lastChannelExtId.length > 0
            && AppSettings.lastChannelSourceId === AppState.sourceId

    // What the hero shows: the most recent unfinished movie/episode, else the
    // last channel, else the newest movie.
    readonly property var hero: {
        if (continueWatching.count > 0 && heroTick >= 0) {
            const item = continueWatching.get(0)
            return { kind: item.kind, eyebrow: qsTr("Continue watching"), title: item.title,
                     subtitle: [item.subtitle, qsTr("%1 min left").arg(item.remainingMinutes)]
                               .filter(t => t).join("  ·  "),
                     image: item.image, progress: item.watched, action: qsTr("Resume"), data: item }
        }
        if (hasLastChannel)
            return { kind: "live", eyebrow: qsTr("Last watched"), title: AppSettings.lastChannelName,
                     subtitle: qsTr("Live TV"), image: "", progress: 0, action: qsTr("Watch live") }
        if (newMovies.count > 0 && heroTick >= 0) {
            const movie = newMovies.get(0)
            return { kind: "newMovie", eyebrow: qsTr("New movie"), title: movie.name,
                     subtitle: [movie.year, movie.rating ? "★ " + movie.rating : ""].filter(t => t).join("  ·  "),
                     image: movie.logo, progress: 0, action: qsTr("Details"), data: movie }
        }
        return null
    }
    property int heroTick: 0

    function greeting() {
        const h = new Date().getHours()
        return h < 5 ? qsTr("Good night") : h < 12 ? qsTr("Good morning")
             : h < 18 ? qsTr("Good afternoon") : qsTr("Good evening")
    }

    function heroAction() {
        if (!hero)
            return
        if (hero.kind === "live")
            playChannelRequested(AppSettings.lastChannelSourceId, AppSettings.lastChannelExtId)
        else if (hero.kind === "newMovie")
            openMovieRequested(hero.data.extId)
        else
            playVodRequested(hero.data.sourceId, hero.data.kind, hero.data.extId, hero.data.seriesExtId)
    }

    background: Rectangle { color: AppTheme.background }

    ContinueWatchingModel {
        id: continueWatching
        onModelReset: page.heroTick++
    }
    ChannelListModel {
        id: favoriteChannels
        sourceId: AppState.sourceId
        categoryId: CategoryListModel.FavoritesCategory
    }
    ChannelListModel {
        id: liveChannels
        sourceId: AppState.sourceId
        limit: 16
    }
    ChannelListModel {
        id: newMovies
        sourceId: AppState.sourceId
        kind: "movie"
        sortOrder: "newest"
        limit: 20
        onModelReset: page.heroTick++
    }
    ChannelListModel {
        id: newSeries
        sourceId: AppState.sourceId
        kind: "series"
        sortOrder: "newest"
        limit: 20
    }
    ChannelListModel {
        id: favoriteMovies
        sourceId: AppState.sourceId
        kind: "movie"
        categoryId: CategoryListModel.FavoritesCategory
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentHeight: content.implicitHeight + 40
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: content
            width: flick.width
            spacing: 28

            // Greeting
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: page.narrow ? 16 : 28
                Layout.leftMargin: page.sideMargin
                Layout.rightMargin: page.narrow ? 8 : page.sideMargin
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label {
                        text: page.greeting()
                        font.pixelSize: page.narrow ? 24 : AppTheme.fontH1
                        font.weight: Font.Bold
                        color: AppTheme.textPrimary
                    }
                    Label {
                        text: new Date().toLocaleDateString(Qt.locale(), "dddd d MMMM")
                        color: AppTheme.textTertiary
                        font.pixelSize: AppTheme.fontSmall
                    }
                }
                ProviderSwitcher {
                    visible: page.narrow && AppState.hasSource
                    compact: true
                }
            }

            // Welcome (no provider yet)
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: page.sideMargin
                Layout.rightMargin: page.sideMargin
                Layout.preferredHeight: welcome.implicitHeight + 64
                visible: !AppState.hasSource
                radius: AppTheme.radiusLarge
                border.color: AppTheme.stroke
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: "#2A2160" }
                    GradientStop { position: 1.0; color: "#0F3A4A" }
                }

                ColumnLayout {
                    id: welcome
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: page.narrow ? 24 : 40
                    spacing: 14
                    Label {
                        text: qsTr("WELCOME")
                        color: AppTheme.accent2
                        font.pixelSize: AppTheme.fontCaption
                        font.weight: Font.Bold
                        font.letterSpacing: 1.5
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("All your TV in one place")
                        wrapMode: Text.Wrap
                        font.pixelSize: page.narrow ? 26 : AppTheme.fontDisplay
                        font.weight: Font.Bold
                        color: "white"
                    }
                    Label {
                        Layout.fillWidth: true
                        Layout.maximumWidth: 560
                        text: qsTr("Add your IPTV provider with an Xtream Codes login or an M3U "
                                   + "playlist link to watch live TV, movies and series, with a TV guide.")
                        wrapMode: Text.Wrap
                        color: "#D0D6E4"
                        lineHeight: 1.2
                    }
                    AppButton {
                        Layout.topMargin: 8
                        text: qsTr("Add your provider")
                        iconName: "add"
                        onClicked: AppState.addProviderRequested()
                    }
                }
            }

            // Hero
            Rectangle {
                id: heroCard
                Layout.fillWidth: true
                Layout.leftMargin: page.sideMargin
                Layout.rightMargin: page.sideMargin
                Layout.preferredHeight: page.narrow ? 220 : 300
                visible: AppState.hasSource && page.hero !== null
                radius: AppTheme.radiusLarge
                color: AppTheme.surface
                border.color: AppTheme.stroke
                clip: true

                // Artwork as a dimmed backdrop
                Image {
                    anchors.fill: parent
                    source: page.hero ? page.hero.image : ""
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    opacity: 0.35
                    sourceSize.width: 640
                }
                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: "#F20A0C11" }
                        GradientStop { position: 0.55; color: "#B30A0C11" }
                        GradientStop { position: 1.0; color: "#260A0C11" }
                    }
                }
                // Brand glow
                Rectangle {
                    width: parent.width * 0.6
                    height: parent.height * 2
                    x: -width * 0.3
                    y: -height * 0.25
                    rotation: 20
                    opacity: 0.25
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: AppTheme.accentStrong }
                        GradientStop { position: 1.0; color: "transparent" }
                    }
                }

                // Poster on the right (wide screens)
                Rectangle {
                    visible: !page.narrow && page.hero && page.hero.image.length > 0
                    anchors.right: parent.right
                    anchors.rightMargin: 40
                    anchors.verticalCenter: parent.verticalCenter
                    height: parent.height - 56
                    width: height / 1.5
                    radius: AppTheme.radius
                    color: AppTheme.surfaceHigh
                    clip: true
                    rotation: 3
                    Image {
                        anchors.fill: parent
                        source: page.hero ? page.hero.image : ""
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        sourceSize.width: 400
                    }
                }

                ColumnLayout {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: page.narrow ? 22 : 40
                    width: Math.min(parent.width - (page.narrow ? 44 : 320), 620)
                    spacing: 10

                    Label {
                        text: page.hero ? page.hero.eyebrow.toUpperCase() : ""
                        color: AppTheme.accent2
                        font.pixelSize: AppTheme.fontCaption
                        font.weight: Font.Bold
                        font.letterSpacing: 1.5
                    }
                    Label {
                        Layout.fillWidth: true
                        text: page.hero ? page.hero.title : ""
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        font.pixelSize: page.narrow ? 24 : AppTheme.fontDisplay
                        font.weight: Font.Bold
                        color: "white"
                    }
                    Label {
                        visible: text.length > 0
                        text: page.hero ? page.hero.subtitle : ""
                        color: "#C8CEDC"
                    }
                    Rectangle {
                        visible: page.hero && page.hero.progress > 0
                        Layout.preferredWidth: 220
                        Layout.preferredHeight: 4
                        radius: 2
                        color: "#33FFFFFF"
                        Rectangle {
                            width: parent.width * (page.hero ? page.hero.progress : 0)
                            height: parent.height
                            radius: 2
                            color: AppTheme.accent
                        }
                    }
                    AppButton {
                        Layout.topMargin: 10
                        text: page.hero ? page.hero.action : ""
                        iconName: page.hero && page.hero.kind === "newMovie" ? "info" : "play"
                        onClicked: page.heroAction()
                    }
                }
            }

            ContentRow {
                Layout.fillWidth: true
                sideMargin: page.sideMargin
                title: qsTr("Continue watching")
                model: continueWatching
                rowHeight: 250
                delegate: PosterCard {
                    required property int sourceId
                    required property string kind
                    required property string extId
                    required property string seriesExtId
                    required property string image
                    required property int remainingMinutes
                    required property var model // "subtitle" role (name clash)
                    required watched
                    required title

                    width: 140
                    poster: image
                    subtitle: [model.subtitle, qsTr("%1 min left").arg(remainingMinutes)]
                              .filter(t => t).join(" · ")
                    onClicked: page.playVodRequested(sourceId, kind, extId, seriesExtId)
                }
            }

            ContentRow {
                Layout.fillWidth: true
                sideMargin: page.sideMargin
                title: favoriteChannels.count > 0 ? qsTr("Favorite channels") : qsTr("Live TV")
                showSeeAll: true
                onSeeAllClicked: AppState.sectionRequested("live")
                model: favoriteChannels.count > 0 ? favoriteChannels : liveChannels
                rowHeight: 176
                delegate: ChannelCard {
                    required property int index
                    required name
                    required logo
                    required nowTitle
                    required nowProgress
                    onClicked: page.playChannelIndexRequested(
                                   favoriteChannels.count > 0 ? favoriteChannels : liveChannels, index)
                }
            }

            ContentRow {
                Layout.fillWidth: true
                sideMargin: page.sideMargin
                title: qsTr("Favorite movies")
                model: favoriteMovies
                rowHeight: 285
                delegate: posterDelegate
            }

            ContentRow {
                Layout.fillWidth: true
                sideMargin: page.sideMargin
                title: qsTr("Recently added movies")
                showSeeAll: true
                onSeeAllClicked: AppState.sectionRequested("movie")
                model: newMovies
                rowHeight: 285
                delegate: posterDelegate
            }

            ContentRow {
                Layout.fillWidth: true
                sideMargin: page.sideMargin
                title: qsTr("Recently added series")
                showSeeAll: true
                onSeeAllClicked: AppState.sectionRequested("series")
                model: newSeries
                rowHeight: 285
                delegate: Component {
                    PosterCard {
                        required property string extId
                        required property string name
                        required property string logo
                        required property string year
                        required rating
                        required favorite
                        width: 150
                        title: name
                        poster: logo
                        subtitle: year
                        onClicked: page.openSeriesRequested(extId)
                    }
                }
            }

            // Provider content still loading
            EmptyState {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 20
                visible: AppState.hasSource && page.hero === null && liveChannels.count === 0
                busy: SourceManager.busy
                iconName: "live"
                title: SourceManager.busy ? qsTr("Loading your channels…") : qsTr("Nothing here yet")
                text: SourceManager.busy ? SourceManager.statusText
                                         : qsTr("Update the provider in Settings to load its channels.")
            }
        }
    }

    Component {
        id: posterDelegate
        PosterCard {
            required property string extId
            required property string name
            required property string logo
            required property string year
            required rating
            required watched
            required favorite
            width: 150
            title: name
            poster: logo
            subtitle: year
            onClicked: page.openMovieRequested(extId)
        }
    }
}
