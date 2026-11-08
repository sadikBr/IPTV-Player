import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import IPTVPlayer

// Live TV: categories (side panel or chips), channels with what's on now.
Page {
    id: page

    property int sourceId: -1
    property int selectedCategory: CategoryListModel.AllCategories
    property string errorText: ""
    readonly property bool wide: width >= 760
    readonly property bool narrow: width < AppTheme.compactWidth
    readonly property bool syncing: SourceManager.busy && SourceManager.activeSourceId === sourceId
    readonly property alias channelModel: channels
    readonly property bool gridMode: AppSettings.liveViewMode === "grid"
    readonly property Item activeView: gridMode ? channelGrid : channelList

    signal channelActivated(int index)
    signal guideRequested()

    function playIndex(index) {
        if (index >= 0 && index < channels.count)
            page.channelActivated(index)
    }

    // Plays a channel by its provider id (used by "Last watched" on the home page).
    function playExtId(extId) {
        selectCategory(CategoryListModel.AllCategories)
        const index = channels.indexOfExtId(extId)
        if (index >= 0) {
            activeView.currentIndex = index
            activeView.positionViewAtIndex(index, ListView.Center)
            playIndex(index)
        }
    }

    function hideCategory(categoryId) {
        if (categoryId === selectedCategory)
            selectCategory(CategoryListModel.AllCategories)
        categories.hideCategory(categoryId)
    }

    function selectCategory(categoryId) {
        selectedCategory = categoryId
        searchField.text = ""
        channels.searchText = ""
        channelList.positionViewAtBeginning()
        channelGrid.positionViewAtBeginning()
    }

    background: Rectangle { color: AppTheme.background }

    CategoryListModel {
        id: categories
        sourceId: page.sourceId
    }

    ChannelListModel {
        id: channels
        sourceId: page.sourceId
        categoryId: page.selectedCategory
    }

    // Search as you type, without querying on every keystroke.
    Timer {
        id: searchTimer
        interval: 250
        onTriggered: channels.searchText = searchField.text
    }

    Connections {
        target: SourceManager
        function onFinished(id, ok, error, isNew) {
            if (id === page.sourceId)
                page.errorText = ok ? "" : error
        }
    }

    Component.onCompleted: {
        if (AppSettings.lastChannelSourceId === sourceId) {
            const last = channels.indexOfExtId(AppSettings.lastChannelExtId)
            if (last >= 0) {
                activeView.currentIndex = last
                activeView.positionViewAtIndex(last, ListView.Center)
            }
        }
        SourceManager.refreshIfStale(sourceId, 24)
        EpgManager.refreshIfStale(sourceId, 12)
        activeView.forceActiveFocus()
    }

    header: PageHeader {
        title: qsTr("Live TV")
        subtitle: qsTr("%1 channels").arg(Number(AppState.sourceInfo.liveCount || 0).toLocaleString(Qt.locale(), "f", 0))

        SearchField {
            id: searchField
            visible: !page.narrow
            Layout.preferredWidth: 300
            placeholderText: qsTr("Search channels")
            onTextChanged: searchTimer.restart()
            Keys.onDownPressed: channelList.forceActiveFocus()
            Keys.onEscapePressed: {
                text = ""
                channelList.forceActiveFocus()
            }
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
                    channels.searchText = ""
                }
            }
        }
        IconButton {
            iconName: page.gridMode ? "list" : "grid"
            tip: page.gridMode ? qsTr("Show as list") : qsTr("Show as grid")
            onClicked: AppSettings.liveViewMode = page.gridMode ? "list" : "grid"
        }
        IconButton {
            iconName: "guide"
            tip: qsTr("TV guide")
            onClicked: page.guideRequested()
        }
        SortButton {
            model: channels
            kind: "live"
        }
        IconButton {
            iconName: "filter"
            tip: qsTr("Choose categories")
            onClicked: AppState.manageCategoriesRequested(page.sourceId, "live")
        }
        IconButton {
            iconName: "retry"
            tip: qsTr("Update channel list and TV guide")
            enabled: !SourceManager.busy
            onClicked: {
                page.errorText = ""
                SourceManager.refresh(page.sourceId)
                EpgManager.refresh(page.sourceId)
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        // Search (phones)
        SearchField {
            id: phoneSearch
            visible: page.narrow && searchToggle.checked
            Layout.fillWidth: true
            Layout.leftMargin: AppTheme.margin
            Layout.rightMargin: AppTheme.margin
            placeholderText: qsTr("Search channels")
            onTextChanged: {
                searchField.text = text
                searchTimer.restart()
            }
        }

        // Updating
        StatusBanner {
            Layout.fillWidth: true
            Layout.leftMargin: page.narrow ? AppTheme.margin : AppTheme.pageMargin
            Layout.rightMargin: page.narrow ? AppTheme.margin : AppTheme.pageMargin
            visible: page.syncing || (EpgManager.busy && EpgManager.activeSourceId === page.sourceId)
            text: page.syncing ? SourceManager.statusText : EpgManager.statusText
            progress: page.syncing ? SourceManager.progress : EpgManager.progress
            canCancel: page.syncing && SourceManager.canCancel
            onCancelClicked: SourceManager.cancel()
        }
        StatusBanner {
            Layout.fillWidth: true
            Layout.leftMargin: page.narrow ? AppTheme.margin : AppTheme.pageMargin
            Layout.rightMargin: page.narrow ? AppTheme.margin : AppTheme.pageMargin
            visible: page.errorText.length > 0
            error: true
            text: qsTr("Update failed: %1").arg(page.errorText)
            onDismissClicked: page.errorText = ""
        }

        CategoryChips {
            visible: !page.wide && categories.count > 0
            Layout.fillWidth: true
            model: categories
            selectedCategory: page.selectedCategory
            onCategorySelected: (id) => page.selectCategory(id)
            onHideRequested: (id) => page.hideCategory(id)
            onManageRequested: AppState.manageCategoriesRequested(page.sourceId, "live")
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
                onManageRequested: AppState.manageCategoriesRequested(page.sourceId, "live")
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                // Tiles
                GridView {
                    id: channelGrid
                    visible: page.gridMode
                    anchors.fill: parent
                    anchors.leftMargin: page.wide ? 18 : AppTheme.margin - 6
                    anchors.rightMargin: page.wide ? AppTheme.pageMargin - 10 : AppTheme.margin - 6
                    readonly property int columns: Math.max(2, Math.floor(width / (page.narrow ? 160 : 220)))
                    cellWidth: Math.floor(width / columns)
                    cellHeight: page.narrow ? 170 : 196
                    model: channels
                    clip: true
                    keyNavigationEnabled: true
                    activeFocusOnTab: true
                    highlightMoveDuration: 0
                    topMargin: 6
                    bottomMargin: 16
                    cacheBuffer: cellHeight * 2
                    ScrollBar.vertical: AppScrollBar {}

                    delegate: Item {
                        id: tile
                        required property int index
                        required property string name
                        required property string logo
                        required property int number
                        required property bool favorite
                        required property string nowTitle
                        required property real nowProgress
                        width: channelGrid.cellWidth
                        height: channelGrid.cellHeight

                        ChannelCard {
                            anchors.fill: parent
                            anchors.margins: page.narrow ? 6 : 9
                            name: tile.name
                            logo: tile.logo
                            number: tile.number
                            favorite: tile.favorite
                            nowTitle: tile.nowTitle
                            nowProgress: tile.nowProgress
                            current: tile.GridView.isCurrentItem && channelGrid.activeFocus
                            onClicked: {
                                channelGrid.currentIndex = tile.index
                                page.playIndex(tile.index)
                            }
                            onPressAndHold: channels.toggleFavorite(tile.index)
                            ToolTip.visible: hovered
                            ToolTip.delay: 1200
                            ToolTip.text: qsTr("Long-press or S to add/remove favorite")
                        }
                    }

                    Keys.onReturnPressed: page.playIndex(currentIndex)
                    Keys.onEnterPressed: page.playIndex(currentIndex)
                    Keys.onPressed: (event) => {
                        if (event.key === Qt.Key_S) {
                            channels.toggleFavorite(currentIndex)
                            event.accepted = true
                        }
                    }
                }

                ListView {
                    id: channelList
                    objectName: "channelList"
                    visible: !page.gridMode
                    anchors.fill: parent
                    anchors.leftMargin: page.wide ? 16 : 8
                    anchors.rightMargin: page.wide ? AppTheme.pageMargin - 6 : 8
                    model: channels
                    clip: true
                    spacing: 2
                    keyNavigationEnabled: true
                    activeFocusOnTab: true
                    highlightMoveDuration: 0
                    bottomMargin: 16
                    ScrollBar.vertical: AppScrollBar {}

                    delegate: ChannelDelegate {
                        width: channelList.width
                        current: ListView.isCurrentItem && channelList.activeFocus
                        onClicked: {
                            channelList.currentIndex = index
                            page.playIndex(index)
                        }
                        onFavoriteClicked: channels.toggleFavorite(index)
                    }

                    Keys.onReturnPressed: page.playIndex(currentIndex)
                    Keys.onEnterPressed: page.playIndex(currentIndex)
                    Keys.onPressed: (event) => {
                        if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_F) {
                            if (page.narrow) {
                                searchToggle.checked = true
                                phoneSearch.forceActiveFocus()
                            } else {
                                searchField.forceActiveFocus()
                            }
                            event.accepted = true
                        } else if (event.key === Qt.Key_S) {
                            channels.toggleFavorite(currentIndex)
                            event.accepted = true
                        }
                    }
                }

                EmptyState {
                    anchors.centerIn: parent
                    visible: channels.count === 0
                    busy: page.syncing && (AppState.sourceInfo.liveCount || 0) === 0
                    iconName: channels.searchText.length > 0 ? "search"
                            : page.selectedCategory === CategoryListModel.FavoritesCategory ? "star-outline" : "live"
                    title: channels.searchText.length > 0 ? qsTr("No results")
                         : page.selectedCategory === CategoryListModel.FavoritesCategory ? qsTr("No favorites yet")
                         : page.syncing ? qsTr("Loading channels…") : qsTr("No channels here")
                    text: channels.searchText.length > 0
                          ? qsTr("No channel matches “%1”.").arg(channels.searchText)
                          : page.selectedCategory === CategoryListModel.FavoritesCategory
                            ? qsTr("Tap the star next to a channel to keep it here.")
                            : ""
                }
            }
        }
    }
}
