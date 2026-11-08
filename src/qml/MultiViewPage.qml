import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import IPTVPlayer

// Watch 2 or 4 live channels at once. Tap a screen to hear it, double-tap
// (or the full-screen button) to watch it on its own.
Page {
    id: page

    property int sourceId: -1
    property string firstExtId: ""     // channel to start with
    // Hides the app navigation, like the player.
    readonly property bool immersive: true
    readonly property bool wide: width > height

    property int screens: width >= AppTheme.compactWidth ? 4 : 2
    property var slots: [-1, -1, -1, -1] // channel row per screen
    property int selectedSlot: 0
    property int pickingSlot: -1

    signal watchRequested(string extId) // full screen

    function setSlot(slot, row) {
        const copy = slots.slice()
        copy[slot] = row
        slots = copy
        if (row >= 0)
            selectedSlot = slot
    }

    background: Rectangle { color: "black" }

    // Every live channel (screens refer to rows in it).
    ChannelListModel {
        id: channels
        sourceId: page.sourceId
        kind: "live"
    }

    Component.onCompleted: {
        if (firstExtId.length > 0) {
            const row = channels.indexOfExtId(firstExtId)
            if (row >= 0)
                setSlot(0, row)
        }
    }

    header: Rectangle {
        color: "black"
        implicitHeight: headerRow.implicitHeight + 16
        RowLayout {
            id: headerRow
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 12
            spacing: 8
            IconButton {
                iconName: "back"
                tip: qsTr("Back")
                onClicked: StackView.view.pop()
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                Label {
                    text: qsTr("Multi-view")
                    font.pixelSize: AppTheme.fontH2
                    font.weight: Font.Bold
                    color: AppTheme.textPrimary
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Tap a screen to hear it, double-tap to watch it full screen")
                    elide: Text.ElideRight
                    font.pixelSize: AppTheme.fontCaption
                    color: AppTheme.textTertiary
                }
            }
            Chip {
                text: qsTr("2 screens")
                tinted: false
                selected: page.screens === 2
                onClicked: page.screens = 2
            }
            Chip {
                text: qsTr("4 screens")
                tinted: false
                selected: page.screens === 4
                onClicked: page.screens = 4
            }
        }
    }

    // The screens: 2 side by side (stacked on a phone held upright) or 2 × 2.
    Grid {
        id: grid
        anchors.fill: parent
        anchors.margins: 4
        columns: page.screens === 2 && !page.wide ? 1 : 2
        readonly property int rowCount: Math.ceil(page.screens / columns)

        Repeater {
            model: page.screens
            delegate: MultiViewTile {
                required property int index
                width: grid.width / grid.columns
                height: grid.height / grid.rowCount
                channelModel: channels
                sourceId: page.sourceId
                channelIndex: page.slots[index]
                selected: page.selectedSlot === index
                onSelectRequested: page.selectedSlot = index
                onPickRequested: {
                    page.pickingSlot = index
                    picker.open()
                }
                onClearRequested: page.setSlot(index, -1)
                onFullScreenRequested: page.watchRequested(channels.get(channelIndex).extId)
            }
        }
    }

    // Choose a channel for a screen
    AppDialog {
        id: picker
        title: qsTr("Choose a channel")
        width: Math.min(page.width - 32, 460)
        height: Math.min(page.height - 64, 620)
        standardButtons: Dialog.Cancel
        onOpened: {
            search.text = ""
            search.forceActiveFocus()
        }

        ChannelListModel {
            id: pickerModel
            sourceId: page.sourceId
            kind: "live"
            searchText: search.text
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 8
            SearchField {
                id: search
                Layout.fillWidth: true
                placeholderText: qsTr("Search channels")
            }
            ListView {
                id: pickerList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: pickerModel
                spacing: 2
                ScrollBar.vertical: AppScrollBar {}
                delegate: ChannelDelegate {
                    width: pickerList.width
                    onClicked: {
                        const row = channels.indexOfExtId(pickerModel.get(index).extId)
                        if (row >= 0)
                            page.setSlot(page.pickingSlot, row)
                        picker.close()
                    }
                }
            }
        }
    }
}
