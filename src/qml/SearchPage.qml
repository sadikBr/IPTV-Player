import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import IPTVPlayer

// One search for everything: live channels, movies and series of the
// current provider, and radio stations.
Page {
    id: page
    objectName: "searchPage"

    readonly property bool narrow: width < AppTheme.compactWidth
    readonly property real sideMargin: narrow ? AppTheme.margin : AppTheme.pageMargin
    readonly property string query: queryField.text.trim()
    readonly property int resultCount: channels.count + movies.count + series.count
                                       + RadioManager.searchResults.count

    signal playChannelRequested(var model, int index)
    signal openMovieRequested(string extId)
    signal openSeriesRequested(string extId)
    signal backRequested()

    function goBack() { backRequested() }
    function smokeType(text) { queryField.text = text }

    readonly property Item initialFocusItem: queryField

    background: Rectangle { color: AppTheme.background }
    Component.onCompleted: queryField.forceActiveFocus()

    // Search as you type, without querying on every keystroke.
    Timer {
        id: searchTimer
        interval: 350
        onTriggered: {
            const q = page.query.length >= 2 ? page.query : ""
            channels.searchText = q
            movies.searchText = q
            series.searchText = q
            RadioManager.searchStations(q)
        }
    }

    // Empty search text shows nothing (the models list everything otherwise).
    ChannelListModel {
        id: channels
        sourceId: page.query.length >= 2 ? AppState.sourceId : -1
        kind: "live"
        limit: 30
    }
    ChannelListModel {
        id: movies
        sourceId: page.query.length >= 2 ? AppState.sourceId : -1
        kind: "movie"
        limit: 30
    }
    ChannelListModel {
        id: series
        sourceId: page.query.length >= 2 ? AppState.sourceId : -1
        kind: "series"
        limit: 30
    }

    header: PageHeader {
        title: qsTr("Search")
        showBack: true
        onBackClicked: page.backRequested()
    }

    Flickable {
        anchors.fill: parent
        contentHeight: content.implicitHeight + 32
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: AppScrollBar {}

        ColumnLayout {
            id: content
            width: parent.width
            spacing: 28

            SearchField {
                id: queryField
                Layout.fillWidth: true
                Layout.leftMargin: page.sideMargin
                Layout.rightMargin: page.sideMargin
                Layout.maximumWidth: 720
                placeholderText: AppState.hasSource ? qsTr("Channels, movies, series, radio stations")
                                                    : qsTr("Radio stations")
                onTextChanged: searchTimer.restart()
            }

            ContentRow {
                Layout.fillWidth: true
                sideMargin: page.sideMargin
                title: qsTr("Live TV")
                rowHeight: 180
                model: channels
                delegate: Item {
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
                        onClicked: page.playChannelRequested(channels, ch.index)
                    }
                }
            }

            ContentRow {
                Layout.fillWidth: true
                sideMargin: page.sideMargin
                title: qsTr("Movies")
                rowHeight: page.narrow ? 270 : 316
                model: movies
                delegate: posterDelegate
            }

            ContentRow {
                Layout.fillWidth: true
                sideMargin: page.sideMargin
                title: qsTr("Series")
                rowHeight: page.narrow ? 270 : 316
                model: series
                delegate: seriesDelegate
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: page.sideMargin
                Layout.rightMargin: page.sideMargin
                spacing: 10
                visible: RadioManager.searchResults.count > 0
                Label {
                    text: qsTr("Radio")
                    font.pixelSize: AppTheme.fontH2
                    font.weight: Font.Bold
                    color: AppTheme.textPrimary
                }
                GridLayout {
                    Layout.fillWidth: true
                    columns: Math.max(1, Math.floor(width / 320))
                    columnSpacing: 12
                    rowSpacing: 12
                    Repeater {
                        model: RadioManager.searchResults
                        delegate: Item {
                            id: st
                            required property int index
                            required property string uuid
                            required property string name
                            required property string logo
                            required property string details
                            required property bool favorite
                            Layout.fillWidth: true
                            Layout.preferredHeight: 76
                            StationCard {
                                anchors.fill: parent
                                name: st.name
                                logo: st.logo
                                details: st.details
                                favorite: st.favorite
                                playing: RadioManager.current.uuid === st.uuid
                                onClicked: RadioManager.playSearchResult(st.index)
                                onFavoriteClicked: {} // favorites from the Radio page
                            }
                        }
                    }
                }
            }

            EmptyState {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 40
                visible: page.query.length >= 2 && page.resultCount === 0 && !searchTimer.running
                iconName: "search"
                title: qsTr("No results")
                text: qsTr("Nothing matches “%1”.").arg(page.query)
            }
            EmptyState {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 40
                visible: page.query.length < 2
                iconName: "search"
                title: qsTr("Search everything")
                text: AppState.hasSource ? qsTr("Live channels, movies and series of %1, and radio stations from around the world.")
                                               .arg(AppState.sourceInfo.name || qsTr("your provider"))
                                         : qsTr("Radio stations from around the world.")
            }
        }
    }

    Component {
        id: posterDelegate
        Item {
            id: mv
            required property string extId
            required property string name
            required property string logo
            required property string year
            required property string rating
            width: page.narrow ? 124 : 172
            height: ListView.view ? ListView.view.height : 316
            PosterCard {
                anchors.fill: parent
                title: mv.name
                poster: mv.logo
                subtitle: mv.year
                rating: mv.rating
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
            width: page.narrow ? 124 : 172
            height: ListView.view ? ListView.view.height : 316
            PosterCard {
                anchors.fill: parent
                title: sr.name
                poster: sr.logo
                subtitle: sr.year
                rating: sr.rating
                onClicked: page.openSeriesRequested(sr.extId)
            }
        }
    }
}
