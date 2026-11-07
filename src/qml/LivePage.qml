import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import IPTVPlayer

// Live TV browser for one source: categories on the left (or in a drop-down
// on narrow screens), channels on the right, search on top.
Page {
    id: page

    property int sourceId: -1
    // Plays this channel (by ext id) as soon as the list is loaded.
    property string autoPlayExtId: ""

    property var sourceInfo: SourceManager.source(sourceId)
    property int selectedCategory: CategoryListModel.AllCategories
    property string errorText: ""
    readonly property bool wide: width >= 760
    readonly property bool syncing: SourceManager.busy && SourceManager.activeSourceId === sourceId
    readonly property alias channelModel: channels

    signal backRequested()
    signal channelActivated(int index)
    signal guideRequested()
    signal sectionRequested(string kind)

    function playIndex(index) {
        if (index >= 0 && index < channels.count)
            page.channelActivated(index)
    }

    function selectCategory(categoryId) {
        selectedCategory = categoryId
        searchField.text = ""
        channelList.positionViewAtBeginning()
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
            if (id !== page.sourceId)
                return
            page.sourceInfo = SourceManager.source(page.sourceId)
            page.errorText = ok ? "" : error
        }
    }

    Component.onCompleted: {
        if (autoPlayExtId.length > 0) {
            const index = channels.indexOfExtId(autoPlayExtId)
            autoPlayExtId = ""
            if (index >= 0) {
                channelList.currentIndex = index
                Qt.callLater(page.playIndex, index)
            }
        } else if (AppSettings.lastChannelSourceId === sourceId) {
            const last = channels.indexOfExtId(AppSettings.lastChannelExtId)
            if (last >= 0) {
                channelList.currentIndex = last
                channelList.positionViewAtIndex(last, ListView.Center)
            }
        }
        SourceManager.refreshIfStale(sourceId, 24)
        EpgManager.refreshIfStale(sourceId, 12)
        channelList.forceActiveFocus()
    }

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
                objectName: "searchField"
                visible: page.wide || searchButton.checked
                placeholderText: qsTr("Search channels")
                selectByMouse: true
                Layout.fillWidth: true
                Layout.maximumWidth: 480
                Layout.alignment: Qt.AlignVCenter
                onTextChanged: searchTimer.restart()
                Keys.onDownPressed: channelList.forceActiveFocus()
                Keys.onEscapePressed: {
                    text = ""
                    channelList.forceActiveFocus()
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
                iconName: "guide"
                tip: qsTr("TV guide")
                onClicked: page.guideRequested()
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
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        SectionTabs {
            Layout.fillWidth: true
            sourceInfo: page.sourceInfo
            current: "live"
            onSectionSelected: (kind) => page.sectionRequested(kind)
        }

        // Sync progress
        Pane {
            Layout.fillWidth: true
            visible: page.syncing
            padding: AppTheme.spacing
            background: Rectangle { color: AppTheme.surface }

            RowLayout {
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: AppTheme.spacing

                ColumnLayout {
                    Layout.fillWidth: true
                    Label {
                        text: SourceManager.statusText
                        color: AppTheme.textPrimary
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    ProgressBar {
                        Layout.fillWidth: true
                        indeterminate: SourceManager.progress < 0
                        value: Math.max(0, SourceManager.progress)
                    }
                }
                Button {
                    text: qsTr("Cancel")
                    flat: true
                    visible: SourceManager.canCancel
                    onClicked: SourceManager.cancel()
                }
            }
        }

        // TV guide download (slim, it runs in the background)
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: AppTheme.spacing
            Layout.rightMargin: AppTheme.spacing
            visible: EpgManager.busy && EpgManager.activeSourceId === page.sourceId && !page.syncing
            Label {
                text: EpgManager.statusText
                color: AppTheme.textSecondary
                font.pixelSize: 12
            }
            ProgressBar {
                Layout.fillWidth: true
                indeterminate: EpgManager.progress < 0
                value: Math.max(0, EpgManager.progress)
            }
        }

        // Last refresh failed
        Pane {
            Layout.fillWidth: true
            visible: page.errorText.length > 0
            padding: AppTheme.spacing
            background: Rectangle { color: "#33FF6B6B" }

            RowLayout {
                anchors.left: parent.left
                anchors.right: parent.right
                Label {
                    text: qsTr("Update failed: %1").arg(page.errorText)
                    color: AppTheme.textPrimary
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                IconButton {
                    iconName: "close"
                    tip: qsTr("Dismiss")
                    onClicked: page.errorText = ""
                }
            }
        }

        // Category drop-down (narrow screens)
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
            displayText: categories.count > 0 ? categories.nameOf(page.selectedCategory)
                                              : qsTr("All channels")
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

            // Category list (wide screens)
            ListView {
                id: categoryList
                visible: page.wide
                Layout.preferredWidth: 280
                Layout.fillHeight: true
                model: categories
                clip: true
                currentIndex: categories.indexOf(page.selectedCategory)
                ScrollIndicator.vertical: ScrollIndicator {}

                delegate: ItemDelegate {
                    id: categoryDelegate
                    required property int index
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

            // Channels
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ListView {
                    id: channelList
                    objectName: "channelList"
                    anchors.fill: parent
                    model: channels
                    clip: true
                    keyNavigationEnabled: true
                    highlightMoveDuration: 0
                    ScrollBar.vertical: ScrollBar {}

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
                            searchButton.checked = true
                            searchField.forceActiveFocus()
                            event.accepted = true
                        } else if (event.key === Qt.Key_S) {
                            channels.toggleFavorite(currentIndex)
                            event.accepted = true
                        }
                    }
                }

                // Empty states
                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 2 * AppTheme.margin, 360)
                    visible: channels.count === 0
                    spacing: AppTheme.spacing

                    BusyIndicator {
                        Layout.alignment: Qt.AlignHCenter
                        visible: page.syncing && (page.sourceInfo.liveCount || 0) === 0
                        running: visible
                    }
                    Label {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        color: AppTheme.textSecondary
                        text: channels.searchText.length > 0
                              ? qsTr("No channels match “%1”.").arg(channels.searchText)
                              : page.selectedCategory === CategoryListModel.FavoritesCategory
                                ? qsTr("No favorites yet. Tap the star next to a channel to add it here.")
                                : page.syncing ? qsTr("Loading channels…")
                                               : qsTr("No channels in this category.")
                    }
                }
            }
        }
    }
}
