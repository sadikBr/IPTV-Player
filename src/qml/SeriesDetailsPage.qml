import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import IPTVPlayer

// Cinematic series page: backdrop, details, season chips and episode cards.
Page {
    id: page

    // Keyboard / TV remote focus starts here.
    readonly property Item initialFocusItem: playButton

    property int sourceId: -1
    property string extId

    property var info: VodManager.series(sourceId, extId)
    property int selectedSeason: 0
    property string errorText: ""
    property int loadingTick: 0
    readonly property bool loading: loadingTick >= 0 && VodManager.isLoading(sourceId, extId)
    readonly property bool narrow: width < AppTheme.compactWidth
    readonly property real sideMargin: narrow ? AppTheme.margin : AppTheme.pageMargin
    readonly property var resumeEpisode: allEpisodes.count > 0 && resumeTick >= 0
                                         ? allEpisodes.get(allEpisodes.resumeIndex()) : null
    property int resumeTick: 0

    signal backRequested()
    signal episodeRequested(string episodeExtId, bool fromStart)

    function formatMinutes(secs) {
        return secs > 0 ? qsTr("%1 min").arg(Math.round(secs / 60)) : ""
    }

    background: Rectangle { color: AppTheme.background }

    // All seasons: for the season list and "continue"
    EpisodeListModel {
        id: allEpisodes
        sourceId: page.sourceId
        seriesExtId: page.extId
        onCountChanged: page.resumeTick++
        onModelReset: page.resumeTick++
    }

    EpisodeListModel {
        id: episodes
        sourceId: page.sourceId
        seriesExtId: page.extId
        season: page.selectedSeason
    }

    Component.onCompleted: VodManager.fetchSeries(sourceId, extId)

    Connections {
        target: VodManager
        function onSeriesLoaded(id, ext, ok, error) {
            if (id !== page.sourceId || ext !== page.extId)
                return
            page.info = VodManager.series(page.sourceId, page.extId)
            page.errorText = ok ? "" : error
        }
        function onLoadingChanged() { page.loadingTick++ }
    }

    // Pick a season once the list is known: the one to continue, or the first.
    Connections {
        target: allEpisodes
        function onSeasonsChanged() {
            if (page.selectedSeason === 0 || allEpisodes.seasons.indexOf(page.selectedSeason) < 0)
                page.selectedSeason = page.resumeEpisode ? page.resumeEpisode.season
                                                         : (allEpisodes.seasons[0] || 0)
        }
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentHeight: content.y + content.implicitHeight + 48
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: AppScrollBar {}

        DetailsBackdrop {
            id: backdrop
            width: flick.width
            height: page.narrow ? 300 : Math.max(420, Math.min(page.height * 0.78, flick.width * 0.5))
            backdrop: page.info.backdrop || ""
            poster: page.info.poster || ""
        }

        ColumnLayout {
            id: content
            x: page.sideMargin
            // Short screens (phones in landscape): start near the top so Play shows.
            y: page.narrow ? 180 : Math.max(72, Math.min(Math.max(120, backdrop.height - 340), page.height - 400))
            width: Math.min(flick.width - 2 * page.sideMargin, 1180)
            spacing: 28

            // ---- Details ----
            GridLayout {
                Layout.fillWidth: true
                columns: page.narrow ? 1 : 2
                columnSpacing: 40
                rowSpacing: AppTheme.margin

                Item {
                    Layout.alignment: Qt.AlignTop | (page.narrow ? Qt.AlignHCenter : Qt.AlignLeft)
                    Layout.preferredWidth: page.narrow ? 150 : 250
                    Layout.preferredHeight: Layout.preferredWidth * 1.5
                    Shadow { target: posterFrame; spread: 30 }
                    Rectangle {
                        id: posterFrame
                        anchors.fill: parent
                        radius: AppTheme.radius
                        color: AppTheme.surfaceHigh
                        clip: true
                        Image {
                            anchors.fill: parent
                            source: page.info.poster || ""
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            sourceSize.width: 500
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    Layout.topMargin: page.narrow ? 0 : 40
                    spacing: 14

                    Label {
                        text: qsTr("SERIES")
                        color: AppTheme.accent
                        font.pixelSize: AppTheme.fontCaption
                        font.weight: Font.Bold
                        font.letterSpacing: 2
                    }
                    Label {
                        Layout.fillWidth: true
                        text: page.info.name || ""
                        wrapMode: Text.Wrap
                        font.pixelSize: page.narrow ? 28 : AppTheme.fontHero
                        font.weight: Font.Bold
                        font.letterSpacing: -0.5
                        lineHeight: 1.05
                        color: "white"
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 8
                        MetaPill { text: page.info.year || "" }
                        MetaPill {
                            text: allEpisodes.seasons.length === 0 ? ""
                                : allEpisodes.seasons.length === 1 ? qsTr("1 season")
                                : qsTr("%1 seasons").arg(allEpisodes.seasons.length)
                        }
                        MetaPill {
                            text: page.info.rating || ""
                            iconName: "star"
                            iconColor: AppTheme.favorite
                        }
                        MetaPill { text: page.info.genre || "" }
                    }

                    RowLayout {
                        Layout.topMargin: 8
                        spacing: 12
                        AppButton {
                            id: playButton
                            objectName: "playButton"
                            enabled: page.resumeEpisode !== null
                            iconName: "play"
                            text: page.resumeEpisode
                                  ? (page.resumeEpisode.watched > 0 && page.resumeEpisode.watched < 1
                                     ? qsTr("Continue S%1 E%2") : qsTr("Play S%1 E%2"))
                                    .arg(page.resumeEpisode.season).arg(page.resumeEpisode.episode)
                                  : qsTr("Play")
                            onClicked: page.episodeRequested(page.resumeEpisode.extId, false)
                        }
                        IconButton {
                            size: 44
                            filled: true
                            iconName: page.info.favorite ? "heart" : "heart-outline"
                            iconColor: page.info.favorite ? AppTheme.accent : AppTheme.textPrimary
                            tip: page.info.favorite ? qsTr("Remove from favorites") : qsTr("Add to favorites")
                            onClicked: {
                                VodManager.setFavorite(page.sourceId, "series", page.extId, !page.info.favorite)
                                page.info = VodManager.series(page.sourceId, page.extId)
                            }
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        Layout.maximumWidth: 760
                        Layout.topMargin: 6
                        visible: text.length > 0
                        text: page.info.plot || ""
                        wrapMode: Text.Wrap
                        maximumLineCount: 6
                        elide: Text.ElideRight
                        color: "#D0D0D6"
                        font.pixelSize: AppTheme.fontH3
                        lineHeight: 1.35
                    }
                    Label {
                        Layout.fillWidth: true
                        Layout.maximumWidth: 760
                        visible: !!page.info.cast
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        color: AppTheme.textSecondary
                        text: qsTr("Cast: %1").arg(page.info.cast || "")
                    }
                }
            }

            // ---- Episodes ----
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 14

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16
                    Label {
                        text: qsTr("Episodes")
                        font.pixelSize: AppTheme.fontH2
                        font.weight: Font.Bold
                        color: AppTheme.textPrimary
                    }
                    Label {
                        visible: episodes.count > 0
                        text: qsTr("%1 in this season").arg(episodes.count)
                        color: AppTheme.textTertiary
                        font.pixelSize: AppTheme.fontSmall
                        Layout.fillWidth: true
                    }
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    visible: allEpisodes.seasons.length > 1
                    Repeater {
                        model: allEpisodes.seasons
                        delegate: Chip {
                            required property var modelData
                            text: qsTr("Season %1").arg(modelData)
                            selected: page.selectedSeason === modelData
                            onClicked: page.selectedSeason = modelData
                        }
                    }
                }

                BusyIndicator {
                    Layout.alignment: Qt.AlignHCenter
                    visible: page.loading && episodes.count === 0
                    running: visible
                }
                Label {
                    Layout.fillWidth: true
                    visible: !page.loading && episodes.count === 0
                    wrapMode: Text.Wrap
                    color: page.errorText ? AppTheme.error : AppTheme.textSecondary
                    text: page.errorText ? qsTr("Couldn't load the episodes: %1").arg(page.errorText)
                                         : qsTr("No episodes found.")
                }

                // Episode cards: a grid on wide screens, a list on phones
                GridLayout {
                    id: episodeGrid
                    Layout.fillWidth: true
                    columns: page.narrow ? 1 : Math.max(2, Math.floor(content.width / 300))
                    columnSpacing: 20
                    rowSpacing: page.narrow ? 12 : 26

                    Repeater {
                        model: episodes
                        delegate: EpisodeCard {
                            // Same width for every card (the grid would otherwise
                            // size columns by content and the stills would differ).
                            // Based on the page content width, not the grid's own width
                            // (which depends on the cards and would shrink with them).
                            readonly property real cellWidth: Math.floor((content.width - episodeGrid.columnSpacing
                                                               * (episodeGrid.columns - 1)) / episodeGrid.columns)
                            Layout.preferredWidth: cellWidth
                            Layout.minimumWidth: cellWidth
                            Layout.maximumWidth: cellWidth
                            Layout.alignment: Qt.AlignTop
                            compact: page.narrow
                            onClicked: page.episodeRequested(extId, false)
                        }
                    }
                }
            }
        }
    }

    IconButton {
        x: 16
        y: 16
        filled: true
        iconName: "back"
        tip: qsTr("Back")
        onClicked: page.backRequested()
    }

    Keys.onEscapePressed: page.backRequested()
    Keys.onBackPressed: page.backRequested()
}
