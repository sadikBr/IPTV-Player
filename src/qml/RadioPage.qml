import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import IPTVPlayer

// Internet radio from the radio-browser.info directory.
Page {
    id: page

    readonly property bool narrow: width < AppTheme.compactWidth
    readonly property real sideMargin: narrow ? AppTheme.margin : AppTheme.pageMargin

    background: Rectangle { color: AppTheme.background }

    Component.onCompleted: {
        if (RadioManager.mode.length > 0)
            return
        if (RadioManager.favoriteCount > 0)
            RadioManager.showFavorites()
        else
            RadioManager.showTop()
    }

    // Search as you type, without querying on every keystroke.
    Timer {
        id: searchTimer
        interval: 450
        onTriggered: RadioManager.search(search.text)
    }

    header: PageHeader {
        title: qsTr("Radio")
        subtitle: qsTr("Stations from around the world, from radio-browser.info")
        SearchField {
            id: search
            Layout.preferredWidth: page.narrow ? 160 : 300
            placeholderText: qsTr("Search stations")
            onTextChanged: searchTimer.restart()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 14

        // What to show
        ListView {
            id: chips
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            orientation: ListView.Horizontal
            spacing: 8
            leftMargin: page.sideMargin
            rightMargin: page.sideMargin
            clip: true
            keyNavigationEnabled: false
            boundsBehavior: Flickable.StopAtBounds
            model: {
                const list = [
                    { mode: "favorites", value: "", label: qsTr("Favorites") },
                    { mode: "recent", value: "", label: qsTr("Recently played") },
                    { mode: "top", value: "", label: qsTr("Popular") },
                    { mode: "trending", value: "", label: qsTr("Trending") },
                ]
                if (RadioManager.localCountry.length > 0)
                    list.push({ mode: "country", value: RadioManager.localCountry,
                                label: qsTr("Your country (%1)").arg(RadioManager.localCountry) })
                for (const g of RadioManager.genres)
                    list.push({ mode: "genre", value: g, label: g.charAt(0).toUpperCase() + g.slice(1) })
                return list
            }
            delegate: Chip {
                required property var modelData
                text: modelData.label
                selected: RadioManager.mode === modelData.mode
                          && (modelData.value.length === 0 || RadioManager.modeValue === modelData.value)
                onClicked: {
                    search.text = ""
                    searchTimer.stop()
                    switch (modelData.mode) {
                    case "favorites": RadioManager.showFavorites(); break
                    case "recent": RadioManager.showRecent(); break
                    case "top": RadioManager.showTop(); break
                    case "trending": RadioManager.showTrending(); break
                    case "country": RadioManager.showCountry(modelData.value); break
                    case "genre": RadioManager.showGenre(modelData.value); break
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            GridView {
                id: grid
                anchors.fill: parent
                anchors.leftMargin: page.sideMargin - 6
                anchors.rightMargin: page.sideMargin - 6
                readonly property int columns: Math.max(1, Math.floor(width / 320))
                cellWidth: Math.floor(width / columns)
                cellHeight: 88
                clip: true
                model: RadioManager.stations
                keyNavigationEnabled: true
                activeFocusOnTab: true
                highlightMoveDuration: 0
                bottomMargin: 16
                ScrollBar.vertical: AppScrollBar {}

                delegate: Item {
                    id: cell
                    required property int index
                    required property string uuid
                    required property string name
                    required property string logo
                    required property string details
                    required property bool favorite
                    width: grid.cellWidth
                    height: grid.cellHeight
                    StationCard {
                        anchors.fill: parent
                        anchors.margins: 6
                        name: cell.name
                        logo: cell.logo
                        details: cell.details
                        favorite: cell.favorite
                        playing: RadioManager.current.uuid === cell.uuid
                        focus: cell.GridView.isCurrentItem
                        onClicked: {
                            grid.currentIndex = cell.index
                            RadioManager.play(cell.index)
                        }
                        onFavoriteClicked: RadioManager.toggleFavorite(cell.index)
                    }
                }

                Keys.onReturnPressed: RadioManager.play(currentIndex)
                Keys.onEnterPressed: RadioManager.play(currentIndex)
            }

            BusyIndicator {
                anchors.centerIn: parent
                running: RadioManager.loading
                visible: running
            }

            EmptyState {
                anchors.centerIn: parent
                visible: !RadioManager.loading && grid.count === 0
                iconName: "radio"
                title: RadioManager.errorString.length > 0 ? qsTr("Can't load stations")
                     : RadioManager.mode === "favorites" ? qsTr("No favorite stations yet")
                     : RadioManager.mode === "recent" ? qsTr("Nothing played yet")
                     : qsTr("No stations found")
                text: RadioManager.errorString.length > 0 ? RadioManager.errorString
                    : RadioManager.mode === "favorites" ? qsTr("Tap the heart on a station to keep it here.")
                    : ""
            }
        }
    }
}
