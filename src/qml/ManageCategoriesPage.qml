import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import IPTVPlayer

// Choose which categories of a provider are shown. Hidden categories are left
// out of the lists, "All", search and Home. Favorites stay visible.
Page {
    id: page

    property int sourceId: -1
    property string kind: "live"

    readonly property var sourceInfo: SourceManager.source(sourceId)
    readonly property bool narrow: width < AppTheme.compactWidth
    readonly property real sideMargin: narrow ? AppTheme.margin : AppTheme.pageMargin

    signal backRequested()

    function goBack() { backRequested() }
    function hideGroup(group) { manager.setGroupHidden(group, true) }

    background: Rectangle { color: AppTheme.background }

    CategoryManagerModel {
        id: manager
        sourceId: page.sourceId
        kind: page.kind
        filterText: search.text
    }

    header: PageHeader {
        title: qsTr("Choose categories")
        subtitle: page.sourceInfo.name || ""
        showBack: true
        onBackClicked: page.backRequested()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: page.sideMargin
        anchors.rightMargin: page.sideMargin
        spacing: 14

        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: AppTheme.textSecondary
            text: qsTr("Switch off what you don't watch. Hidden categories disappear from the lists, "
                       + "search and Home; your favorites stay.")
        }

        // Content type
        Flow {
            Layout.fillWidth: true
            spacing: 8
            Repeater {
                model: [
                    { kind: "live", label: qsTr("Live TV"), count: page.sourceInfo.liveCount || 0 },
                    { kind: "movie", label: qsTr("Movies"), count: page.sourceInfo.movieCount || 0 },
                    { kind: "series", label: qsTr("Series"), count: page.sourceInfo.seriesCount || 0 },
                ]
                delegate: Chip {
                    required property var modelData
                    visible: modelData.count > 0
                    text: modelData.label
                    selected: page.kind === modelData.kind
                    onClicked: page.kind = modelData.kind
                }
            }
        }

        // Language / region groups
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: manager.groups.length > 0
            Label {
                text: qsTr("Languages and regions")
                font.pixelSize: AppTheme.fontH3
                font.weight: Font.DemiBold
                color: AppTheme.textPrimary
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: AppTheme.textTertiary
                font.pixelSize: AppTheme.fontSmall
                text: qsTr("Tap a group to hide or show all of its categories at once.")
            }
            Flow {
                Layout.fillWidth: true
                spacing: 8
                Repeater {
                    model: manager.groups
                    delegate: GroupChip {
                        required property var modelData
                        group: modelData
                        onClicked: manager.setGroupHidden(modelData.name, modelData.hidden < modelData.total)
                    }
                }
            }
        }

        // Search and bulk actions (stacked on phones)
        GridLayout {
            Layout.fillWidth: true
            Layout.topMargin: 6
            columns: page.narrow ? 1 : 2
            columnSpacing: 10
            rowSpacing: 10
            SearchField {
                id: search
                Layout.fillWidth: true
                Layout.maximumWidth: page.narrow ? -1 : 420
                placeholderText: qsTr("Search categories")
            }
            RowLayout {
                Layout.alignment: page.narrow ? Qt.AlignLeft : Qt.AlignRight
                spacing: 10
                AppButton {
                    variant: "secondary"
                    implicitHeight: 38
                    text: search.text.length > 0 ? qsTr("Show these") : qsTr("Show all")
                    onClicked: manager.setListedHidden(false)
                }
                AppButton {
                    variant: "secondary"
                    implicitHeight: 38
                    text: search.text.length > 0 ? qsTr("Hide these") : qsTr("Hide all")
                    onClicked: manager.setListedHidden(true)
                }
            }
        }

        Label {
            color: AppTheme.textTertiary
            font.pixelSize: AppTheme.fontSmall
            text: qsTr("%1 of %2 categories shown")
                    .arg(manager.totalCount - manager.hiddenCount).arg(manager.totalCount)
        }

        // Categories
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: manager
            clip: true
            spacing: 2
            keyNavigationEnabled: false // arrow keys move focus between rows
            bottomMargin: 16
            ScrollBar.vertical: AppScrollBar {}

            delegate: ItemDelegate {
                id: row
                required property int index
                required property string name
                required property int count
                required property bool hidden
                required property string group

                width: list.width
                height: 52
                hoverEnabled: true
                focusPolicy: Qt.TabFocus
                leftPadding: 14
                rightPadding: 6
                onClicked: manager.setHidden(index, !hidden)

                background: Rectangle {
                    radius: AppTheme.radius
                    color: row.hovered ? AppTheme.surface : "transparent"
                    FocusRing { target: row; gap: 0 }
                }
                contentItem: RowLayout {
                    spacing: 12
                    Label {
                        Layout.fillWidth: true
                        text: row.name
                        elide: Text.ElideRight
                        color: row.hidden ? AppTheme.textTertiary : AppTheme.textPrimary
                        font.strikeout: row.hidden
                    }
                    Label {
                        text: row.count
                        color: AppTheme.textTertiary
                        font.pixelSize: AppTheme.fontCaption
                    }
                    AppSwitch {
                        checked: !row.hidden
                        onToggled: manager.setHidden(row.index, !checked)
                    }
                }
            }

            EmptyState {
                anchors.centerIn: parent
                visible: list.count === 0
                iconName: "filter"
                title: search.text.length > 0 ? qsTr("No results") : qsTr("No categories")
                text: search.text.length > 0 ? qsTr("No category matches “%1”.").arg(search.text) : ""
            }
        }
    }

    // A language/region group: shows how many of its categories are visible.
    component GroupChip: AbstractButton {
        id: chip
        property var group: ({})
        readonly property int visibleCount: group.total - group.hidden
        readonly property bool allShown: group.hidden === 0
        readonly property bool allHidden: visibleCount === 0

        implicitHeight: 36
        implicitWidth: chipRow.implicitWidth + 28
        hoverEnabled: true
        focusPolicy: Qt.TabFocus

        background: Rectangle {
            radius: height / 2
            color: chip.allShown ? AppTheme.accent
                 : chip.allHidden ? (chip.hovered ? AppTheme.surfaceHover : AppTheme.surface)
                                  : AppTheme.accentSoft
            border.width: chip.allShown ? 0 : 1
            border.color: chip.allHidden ? AppTheme.stroke : AppTheme.accent
            Behavior on color { ColorAnimation { duration: AppTheme.fast } }
            FocusRing { target: chip }
        }
        contentItem: Item {
            Row {
                id: chipRow
                anchors.centerIn: parent
                spacing: 8
                IconImage {
                    anchors.verticalCenter: parent.verticalCenter
                    source: chip.allHidden ? "icons/eye-off.svg" : "icons/check.svg"
                    sourceSize: Qt.size(14, 14)
                    color: chip.allShown ? "white" : chip.allHidden ? AppTheme.textTertiary : AppTheme.accent
                }
                Label {
                    text: chip.group.name
                    font.weight: Font.Bold
                    font.pixelSize: AppTheme.fontSmall
                    color: chip.allShown ? "white" : chip.allHidden ? AppTheme.textTertiary : AppTheme.textPrimary
                }
                Label {
                    text: chip.allShown || chip.allHidden ? chip.group.total
                                                          : chip.visibleCount + "/" + chip.group.total
                    font.pixelSize: AppTheme.fontCaption
                    color: chip.allShown ? "#DDFFFFFF" : AppTheme.textTertiary
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }
}
