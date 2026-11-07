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

    property var sourceInfo: SourceManager.source(sourceId)
    property int selectedCategory: CategoryListModel.AllCategories
    readonly property bool wide: width >= 760
    readonly property bool syncing: SourceManager.busy && SourceManager.activeSourceId === sourceId

    signal backRequested()
    signal itemActivated(string extId)
    signal sectionRequested(string kind)

    function selectCategory(categoryId) {
        selectedCategory = categoryId
        searchField.text = ""
        grid.positionViewAtBeginning()
    }

    function openFirst() {
        if (items.count > 0)
            itemActivated(items.get(0).extId)
    }

    function subtitleFor(year, rating) {
        const parts = []
        if (year) parts.push(year)
        if (rating) parts.push("★ " + rating)
        return parts.join(" · ")
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

    Connections {
        target: SourceManager
        function onFinished(id, ok, error, isNew) {
            if (id === page.sourceId)
                page.sourceInfo = SourceManager.source(page.sourceId)
        }
    }

    Component.onCompleted: grid.forceActiveFocus()

    header: ToolBar {
        Material.background: AppTheme.surface

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 4
            anchors.rightMargin: 4
            spacing: 4

            IconButton {
                iconName: "back"
                tip: qsTr("Providers")
                onClicked: page.backRequested()
            }
            Label {
                text: page.sourceInfo.name || ""
                font.pixelSize: 18
                font.bold: true
                elide: Text.ElideRight
                color: AppTheme.textPrimary
                visible: page.wide || !searchField.visible
                Layout.fillWidth: !searchField.visible
                Layout.maximumWidth: page.wide ? 260 : page.width
            }
            TextField {
                id: searchField
                visible: page.wide || searchButton.checked
                placeholderText: page.kind === "movie" ? qsTr("Search movies") : qsTr("Search series")
                selectByMouse: true
                Layout.fillWidth: true
                Layout.maximumWidth: 480
                onTextChanged: searchTimer.restart()
                Keys.onDownPressed: grid.forceActiveFocus()
                Keys.onEscapePressed: {
                    text = ""
                    grid.forceActiveFocus()
                }
            }
            Item {
                Layout.fillWidth: true
                visible: page.wide
            }
            IconButton {
                id: searchButton
                iconName: checked ? "close" : "search"
                tip: qsTr("Search")
                visible: !page.wide
                checkable: true
                onToggled: {
                    if (checked)
                        searchField.forceActiveFocus()
                    else
                        searchField.text = ""
                }
            }
            IconButton {
                iconName: "retry"
                tip: qsTr("Update lists")
                enabled: !SourceManager.busy
                onClicked: SourceManager.refresh(page.sourceId)
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        SectionTabs {
            Layout.fillWidth: true
            sourceInfo: page.sourceInfo
            current: page.kind
            onSectionSelected: (kind) => page.sectionRequested(kind)
        }

        ProgressBar {
            Layout.fillWidth: true
            visible: page.syncing
            indeterminate: SourceManager.progress < 0
            value: Math.max(0, SourceManager.progress)
        }

        ComboBox {
            id: categoryCombo
            visible: !page.wide && categories.count > 0
            Layout.fillWidth: true
            Layout.margins: AppTheme.spacing
            model: categories
            textRole: "name"
            valueRole: "categoryId"
            // categories.count makes these update once the list is loaded
            currentIndex: categories.count > 0 ? categories.indexOf(page.selectedCategory) : -1
            displayText: categories.count > 0 ? categories.nameOf(page.selectedCategory) : ""
            onActivated: page.selectCategory(currentValue)

            delegate: ItemDelegate {
                required property int index
                required property string name
                required property int count
                width: categoryCombo.width
                text: name + "  (" + count + ")"
                highlighted: categoryCombo.highlightedIndex === index
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            ListView {
                id: categoryList
                visible: page.wide
                Layout.preferredWidth: 260
                Layout.fillHeight: true
                model: categories
                clip: true
                ScrollIndicator.vertical: ScrollIndicator {}

                delegate: ItemDelegate {
                    id: categoryDelegate
                    required property int categoryId
                    required property string name
                    required property int count
                    readonly property bool selected: categoryId === page.selectedCategory

                    width: categoryList.width
                    onClicked: page.selectCategory(categoryId)
                    background: Rectangle {
                        color: categoryDelegate.selected ? AppTheme.selection
                             : categoryDelegate.hovered ? "#14FFFFFF" : "transparent"
                    }
                    contentItem: RowLayout {
                        spacing: AppTheme.spacing
                        Label {
                            text: categoryDelegate.name
                            elide: Text.ElideRight
                            color: categoryDelegate.selected ? AppTheme.accent : AppTheme.textPrimary
                            font.bold: categoryDelegate.categoryId < 0
                            Layout.fillWidth: true
                        }
                        Label {
                            text: categoryDelegate.count
                            color: AppTheme.textSecondary
                            font.pixelSize: 12
                        }
                    }
                }
            }

            Rectangle {
                visible: page.wide
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                color: "#22FFFFFF"
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                GridView {
                    id: grid
                    anchors.fill: parent
                    anchors.leftMargin: AppTheme.spacing / 2
                    anchors.rightMargin: AppTheme.spacing / 2
                    readonly property int columns: Math.max(2, Math.floor(width / (page.wide ? 170 : 130)))
                    cellWidth: Math.floor(width / columns)
                    cellHeight: cellWidth * 1.5 + 64
                    model: items
                    clip: true
                    keyNavigationEnabled: true
                    highlightMoveDuration: 0
                    cacheBuffer: cellHeight * 2
                    ScrollBar.vertical: ScrollBar {}

                    delegate: PosterCard {
                        required property int index
                        required property string extId
                        required property string name
                        required property string logo
                        required property string year
                        required property string rating
                        // Model roles with the same names as PosterCard properties
                        required watched
                        required favorite

                        width: grid.cellWidth
                        height: grid.cellHeight
                        title: name
                        poster: logo
                        subtitle: page.subtitleFor(year, rating)
                        focus: GridView.isCurrentItem
                        onClicked: {
                            grid.currentIndex = index
                            page.itemActivated(extId)
                        }
                    }

                    Keys.onReturnPressed: page.itemActivated(items.get(currentIndex).extId)
                    Keys.onEnterPressed: page.itemActivated(items.get(currentIndex).extId)
                }

                Label {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 2 * AppTheme.margin, 360)
                    visible: items.count === 0
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    color: AppTheme.textSecondary
                    text: items.searchText.length > 0
                          ? qsTr("Nothing matches “%1”.").arg(items.searchText)
                          : page.selectedCategory === CategoryListModel.FavoritesCategory
                            ? qsTr("No favorites yet. Open a title and tap the star to add it here.")
                            : page.syncing ? qsTr("Loading…") : qsTr("Nothing in this category.")
                }
            }
        }
    }
}
