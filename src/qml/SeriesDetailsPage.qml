import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import IPTVPlayer

// Series overview with a season picker and the episode list.
Page {
    id: page

    property int sourceId: -1
    property string extId

    property var info: VodManager.series(sourceId, extId)
    property int selectedSeason: 0
    property string errorText: ""
    property int loadingTick: 0
    readonly property bool loading: loadingTick >= 0 && VodManager.isLoading(sourceId, extId)
    readonly property bool narrow: width < AppTheme.compactWidth
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

    Image {
        id: backdrop
        anchors.left: parent.left
        anchors.right: parent.right
        height: Math.min(parent.height * 0.5, width * 0.5)
        source: page.info.backdrop || ""
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        opacity: 0.3
        visible: status === Image.Ready
    }
    Rectangle {
        anchors.fill: backdrop
        visible: backdrop.visible
        gradient: Gradient {
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 1.0; color: AppTheme.background }
        }
    }

    IconButton {
        z: 2
        x: 12
        y: 12
        filled: true
        iconName: "back"
        tip: qsTr("Back")
        onClicked: page.backRequested()
    }

    // A plain scrolling column: a season has at most a few dozen episodes, and
    // this keeps the details at the top when their height settles after loading.
    Flickable {
        id: list
        anchors.fill: parent
        anchors.topMargin: page.narrow ? 48 : 56
        contentHeight: column.implicitHeight + AppTheme.margin * 2
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

      Column {
        id: column
        width: list.width

        ColumnLayout {
            width: column.width
            spacing: AppTheme.spacing

            GridLayout {
                Layout.fillWidth: true
                Layout.leftMargin: AppTheme.margin
                Layout.rightMargin: AppTheme.margin
                columns: page.narrow ? 1 : 2
                columnSpacing: AppTheme.margin * 1.5
                rowSpacing: AppTheme.margin

                Rectangle {
                    Layout.alignment: Qt.AlignTop | (page.narrow ? Qt.AlignHCenter : Qt.AlignLeft)
                    Layout.preferredWidth: page.narrow ? 140 : 220
                    Layout.preferredHeight: Layout.preferredWidth * 1.5
                    radius: 8
                    color: AppTheme.surface
                    clip: true
                    Image {
                        anchors.fill: parent
                        source: page.info.poster || ""
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        sourceSize.width: 440
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    spacing: AppTheme.spacing

                    Label {
                        Layout.fillWidth: true
                        text: page.info.name || ""
                        wrapMode: Text.Wrap
                        font.pixelSize: page.narrow ? 24 : 30
                        font.bold: true
                        color: AppTheme.textPrimary
                    }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        color: AppTheme.textSecondary
                        text: [page.info.year,
                               allEpisodes.seasons.length > 0
                               ? (allEpisodes.seasons.length === 1 ? qsTr("1 season")
                                  : qsTr("%1 seasons").arg(allEpisodes.seasons.length)) : "",
                               page.info.rating ? "★ " + page.info.rating : "", page.info.genre]
                              .filter(t => t).join("  ·  ")
                    }
                    RowLayout {
                        spacing: AppTheme.spacing
                        AppButton {
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
                            iconName: page.info.favorite ? "heart" : "heart-outline"
                            tip: page.info.favorite ? qsTr("Remove from favorites") : qsTr("Add to favorites")
                            iconColor: page.info.favorite ? AppTheme.live : AppTheme.textPrimary
                        filled: true
                        size: 44
                            onClicked: {
                                VodManager.setFavorite(page.sourceId, "series", page.extId, !page.info.favorite)
                                page.info = VodManager.series(page.sourceId, page.extId)
                            }
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: text.length > 0
                        text: page.info.plot || ""
                        wrapMode: Text.Wrap
                        maximumLineCount: 6
                        elide: Text.ElideRight
                        color: AppTheme.textPrimary
                        lineHeight: 1.2
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: !!page.info.cast
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        color: AppTheme.textSecondary
                        text: qsTr("Cast: %1").arg(page.info.cast || "")
                    }
                }
            }

            // Seasons
            ListView {
                Layout.fillWidth: true
                Layout.topMargin: AppTheme.margin
                Layout.preferredHeight: 34
                visible: allEpisodes.seasons.length > 1
                orientation: ListView.Horizontal
                spacing: 8
                leftMargin: AppTheme.margin
                rightMargin: AppTheme.margin
                clip: true
                model: allEpisodes.seasons
                delegate: Chip {
                    required property var modelData
                    text: qsTr("Season %1").arg(modelData)
                    selected: page.selectedSeason === modelData
                    onClicked: page.selectedSeason = modelData
                }
            }
            Label {
                Layout.leftMargin: AppTheme.margin
                Layout.topMargin: allEpisodes.seasons.length > 1 ? 4 : AppTheme.margin
                visible: episodes.count > 0
                text: qsTr("%1 episodes").arg(episodes.count)
                color: AppTheme.textTertiary
                font.pixelSize: AppTheme.fontCaption
            }

            BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                visible: page.loading && episodes.count === 0
                running: visible
            }
            Label {
                Layout.fillWidth: true
                Layout.margins: AppTheme.margin
                visible: !page.loading && episodes.count === 0
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                color: page.errorText ? AppTheme.error : AppTheme.textSecondary
                text: page.errorText ? qsTr("Couldn't load the episodes: %1").arg(page.errorText)
                                     : qsTr("No episodes found.")
            }
        }

        Repeater {
          model: episodes
          delegate: ItemDelegate {
            id: episodeDelegate
            required property int index
            required property string extId
            required property string title
            required property int episode
            required property string plot
            required property string image
            required property int durationSecs
            required property real watched

            width: column.width
            padding: AppTheme.spacing
            leftPadding: AppTheme.margin
            rightPadding: AppTheme.margin
            hoverEnabled: true
            onClicked: page.episodeRequested(extId, false)
            background: Rectangle {
                x: 6
                width: parent.width - 12
                height: parent.height
                radius: AppTheme.radius
                color: episodeDelegate.hovered ? AppTheme.surface : "transparent"
            }

            contentItem: RowLayout {
                spacing: AppTheme.margin

                // Thumbnail (or the episode number)
                Rectangle {
                    Layout.preferredWidth: page.narrow ? 112 : 176
                    Layout.preferredHeight: Layout.preferredWidth * 9 / 16
                    Layout.alignment: Qt.AlignTop
                    radius: 6
                    color: AppTheme.surface
                    clip: true
                    Label {
                        anchors.centerIn: parent
                        visible: thumb.status !== Image.Ready
                        text: episodeDelegate.episode
                        font.pixelSize: 22
                        font.bold: true
                        color: AppTheme.textSecondary
                    }
                    Image {
                        id: thumb
                        anchors.fill: parent
                        source: episodeDelegate.image
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        sourceSize.width: 352
                    }
                    Rectangle {
                        visible: episodeDelegate.watched > 0
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 4
                        color: "#88000000"
                        Rectangle {
                            width: parent.width * episodeDelegate.watched
                            height: parent.height
                            color: AppTheme.accent
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    spacing: 4
                    Label {
                        Layout.fillWidth: true
                        text: episodeDelegate.episode + ". " + episodeDelegate.title
                        elide: Text.ElideRight
                        color: AppTheme.textPrimary
                        font.pixelSize: 15
                        font.bold: true
                    }
                    Label {
                        visible: text.length > 0
                        text: [page.formatMinutes(episodeDelegate.durationSecs),
                               episodeDelegate.watched >= 1 ? qsTr("Watched") : ""]
                              .filter(t => t).join(" · ")
                        color: AppTheme.textSecondary
                        font.pixelSize: 12
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: text.length > 0
                        text: episodeDelegate.plot
                        wrapMode: Text.Wrap
                        maximumLineCount: page.narrow ? 2 : 3
                        elide: Text.ElideRight
                        color: AppTheme.textSecondary
                        font.pixelSize: 13
                    }
                }
            }
        }

        }
      }
    }

    Keys.onEscapePressed: page.backRequested()
    Keys.onBackPressed: page.backRequested()
}
