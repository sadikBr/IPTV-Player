import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import IPTVPlayer

// Movies or series of one source: categories plus a poster grid.
Page {
    id: page

    property int sourceId: -1
    property string kind: "movie" // "movie" | "series"

    property int selectedCategory: CategoryListModel.AllCategories
    readonly property bool wide: width >= 760
    readonly property bool narrow: width < AppTheme.compactWidth
    readonly property bool syncing: SourceManager.busy && SourceManager.activeSourceId === sourceId
    readonly property int total: kind === "movie" ? (AppState.sourceInfo.movieCount || 0)
                                                  : (AppState.sourceInfo.seriesCount || 0)

    signal itemActivated(string extId)

    function hideCategory(categoryId) {
        if (categoryId === selectedCategory)
            selectCategory(CategoryListModel.AllCategories)
        categories.hideCategory(categoryId)
    }

    function selectCategory(categoryId) {
        selectedCategory = categoryId
        searchField.text = ""
        items.searchText = ""
        grid.positionViewAtBeginning()
    }

    function openFirst() {
        if (items.count > 0)
            itemActivated(items.get(0).extId)
    }

    background: Rectangle { color: AppTheme.background }

    CategoryListModel {
        id: categories
        sourceId: page.sourceId
        kind: page.kind
    }

    ChannelListModel {
        id: items
        sourceId: page.sourceId
        kind: page.kind
        categoryId: page.selectedCategory
    }

    Timer {
        id: searchTimer
        interval: 250
        onTriggered: items.searchText = searchField.text
    }

    Component.onCompleted: grid.forceActiveFocus()

    header: PageHeader {
        title: page.kind === "movie" ? qsTr("Movies") : qsTr("Series")
        subtitle: page.kind === "movie"
                  ? qsTr("%1 movies").arg(Number(page.total).toLocaleString(Qt.locale(), "f", 0))
                  : qsTr("%1 series").arg(Number(page.total).toLocaleString(Qt.locale(), "f", 0))

        SearchField {
            id: searchField
            visible: !page.narrow
            Layout.preferredWidth: 300
            placeholderText: page.kind === "movie" ? qsTr("Search movies") : qsTr("Search series")
            onTextChanged: searchTimer.restart()
            Keys.onDownPressed: grid.forceActiveFocus()
        }
        IconButton {
            id: searchToggle
            visible: page.narrow
            iconName: checked ? "close" : "search"
            tip: qsTr("Search")
            checkable: true
            onToggled: {
                if (checked) {
                    phoneSearch.forceActiveFocus()
                } else {
                    phoneSearch.text = ""
                    items.searchText = ""
                }
            }
        }
        IconButton {
            iconName: "filter"
            tip: qsTr("Choose categories")
            onClicked: AppState.manageCategoriesRequested(page.sourceId, page.kind)
        }
        IconButton {
            iconName: "retry"
            tip: qsTr("Update lists")
            enabled: !SourceManager.busy
            onClicked: SourceManager.refresh(page.sourceId)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        SearchField {
            id: phoneSearch
            visible: page.narrow && searchToggle.checked
            Layout.fillWidth: true
            Layout.leftMargin: AppTheme.margin
            Layout.rightMargin: AppTheme.margin
            placeholderText: searchField.placeholderText
            onTextChanged: {
                searchField.text = text
                searchTimer.restart()
            }
        }

        StatusBanner {
            Layout.fillWidth: true
            Layout.leftMargin: page.narrow ? AppTheme.margin : AppTheme.pageMargin
            Layout.rightMargin: page.narrow ? AppTheme.margin : AppTheme.pageMargin
            visible: page.syncing
            text: SourceManager.statusText
            progress: SourceManager.progress
        }

        CategoryChips {
            visible: !page.wide && categories.count > 0
            Layout.fillWidth: true
            model: categories
            selectedCategory: page.selectedCategory
            onCategorySelected: (id) => page.selectCategory(id)
            onHideRequested: (id) => page.hideCategory(id)
            onManageRequested: AppState.manageCategoriesRequested(page.sourceId, page.kind)
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            CategoryPanel {
                visible: page.wide
                Layout.preferredWidth: 250
                Layout.fillHeight: true
                Layout.leftMargin: AppTheme.pageMargin - 10
                model: categories
                hiddenCount: categories.hiddenCount
                selectedCategory: page.selectedCategory
                onCategorySelected: (id) => page.selectCategory(id)
                onHideRequested: (id) => page.hideCategory(id)
                onManageRequested: AppState.manageCategoriesRequested(page.sourceId, page.kind)
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                GridView {
                    id: grid
                    anchors.fill: parent
                    anchors.leftMargin: page.wide ? 18 : AppTheme.margin - 6
                    anchors.rightMargin: page.wide ? AppTheme.pageMargin - 10 : AppTheme.margin - 6
                    readonly property int columns: Math.max(2, Math.floor(width / (page.wide ? 180 : 140)))
                    readonly property int gap: page.narrow ? 12 : 18
                    cellWidth: Math.floor(width / columns)
                    cellHeight: (cellWidth - gap) * 1.5 + 66
                    model: items
                    clip: true
                    keyNavigationEnabled: true
                    highlightMoveDuration: 0
                    cacheBuffer: cellHeight * 2
                    bottomMargin: 16
                    topMargin: 4
                    ScrollBar.vertical: AppScrollBar {}

                    delegate: Item {
                        id: cell
                        required property int index
                        required property string extId
                        required property string name
                        required property string logo
                        required property string year
                        required property string rating
                        required property real watched
                        required property bool favorite

                        width: grid.cellWidth
                        height: grid.cellHeight

                        PosterCard {
                            anchors.fill: parent
                            anchors.margins: grid.gap / 2
                            title: cell.name
                            poster: cell.logo
                            subtitle: cell.year
                            rating: cell.rating
                            watched: cell.watched
                            favorite: cell.favorite
                            focus: cell.GridView.isCurrentItem
                            onClicked: {
                                grid.currentIndex = cell.index
                                page.itemActivated(cell.extId)
                            }
                        }
                    }

                    Keys.onReturnPressed: page.itemActivated(items.get(currentIndex).extId)
                    Keys.onEnterPressed: page.itemActivated(items.get(currentIndex).extId)
                }

                EmptyState {
                    anchors.centerIn: parent
                    visible: items.count === 0
                    busy: page.syncing && page.total === 0
                    iconName: items.searchText.length > 0 ? "search"
                            : page.kind === "movie" ? "film" : "series"
                    title: items.searchText.length > 0 ? qsTr("No results")
                         : page.selectedCategory === CategoryListModel.FavoritesCategory ? qsTr("No favorites yet")
                         : page.syncing ? qsTr("Loading…") : qsTr("Nothing here")
                    text: items.searchText.length > 0
                          ? qsTr("Nothing matches “%1”.").arg(items.searchText)
                          : page.selectedCategory === CategoryListModel.FavoritesCategory
                            ? qsTr("Open a title and tap the heart to keep it here.")
                            : ""
                }
            }
        }
    }
}
