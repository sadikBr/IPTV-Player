import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Controls.impl
import QtQuick.Layouts
import IPTVPlayer

// TV guide grid: channels down the side, time across the top.
Page {
    id: page

    // Channels to show: the Live TV list when opened from there, else all channels.
    property ChannelListModel channelModel: null
    readonly property ChannelListModel activeModel: channelModel ? channelModel : ownChannels
    property int sourceId: -1
    property bool showBack: false

    signal backRequested()
    signal channelActivated(int index)
    // Catch-up: play a program from the archive.
    signal catchupRequested(url url, string title)

    readonly property bool narrow: width < AppTheme.compactWidth
    readonly property real pxPerMinute: narrow ? 3 : 4.5
    readonly property int rowHeight: 60
    readonly property int channelColumnWidth: narrow ? 92 : 210
    readonly property int rulerHeight: 34
    // Back as far as the channels' catch-up archives go (else 2 hours), and 3 days ahead.
    readonly property int pastHours: activeModel && activeModel.maxArchiveDays > 0
                                     ? Math.min(activeModel.maxArchiveDays, 7) * 24 : 2
    readonly property int gridHours: pastHours + 70

    // The grid starts pastHours before the hour it was opened.
    readonly property date gridStart: {
        const d = new Date()
        d.setMinutes(0, 0, 0)
        return new Date(d.getTime() - pastHours * 3600000)
    }
    readonly property date gridEnd: new Date(gridStart.getTime() + gridHours * 3600000)
    readonly property real gridWidth: gridHours * 60 * pxPerMinute
    property date now: new Date()
    // Bumped when a new guide is imported so rows reload their programs.
    property int revision: 0
    readonly property bool hasGuide: revision >= 0 && EpgManager.hasGuide(sourceId)

    function xFor(date) {
        return (date.getTime() - gridStart.getTime()) / 60000 * pxPerMinute
    }
    function dateAt(x) {
        return new Date(gridStart.getTime() + x / pxPerMinute * 60000)
    }
    function timeText(date) {
        return AppTheme.time(date)
    }
    function dayText(date) {
        const today = new Date()
        const tomorrow = new Date(today.getTime() + 86400000)
        if (date.toDateString() === today.toDateString())
            return qsTr("Today")
        if (date.toDateString() === tomorrow.toDateString())
            return qsTr("Tomorrow")
        return date.toLocaleDateString(Qt.locale(), "ddd d MMM")
    }
    function scrollToNow() {
        grid.contentX = Math.max(0, Math.min(grid.contentWidth - grid.width,
                                             xFor(now) - grid.width * 0.2))
    }
    // Smoke test: open the first program that can be watched from the archive.
    function openFirstReplayable() {
        for (let i = 0; i < activeModel.count; ++i) {
            const ch = activeModel.get(i)
            if (!ch.archiveDays || !ch.epgKey)
                continue
            const programs = EpgManager.programs(sourceId, ch.epgKey,
                                                 new Date(now.getTime() - 6 * 3600000), now)
            for (const p of programs) {
                if (p.stop <= now && activeModel.canCatchup(i, p.start)) {
                    showDetails(p, i, ch.name)
                    return true
                }
            }
        }
        return false
    }
    function playSelectedReplay() {
        if (replayButton.visible)
            replayButton.clicked()
    }

    function showDetails(program, channelIndex, channelName) {
        details.program = program
        details.channelIndex = channelIndex
        details.channelName = channelName
        details.open()
    }

    background: Rectangle { color: AppTheme.background }

    Timer {
        interval: 30000
        running: true
        repeat: true
        onTriggered: page.now = new Date()
    }

    Connections {
        target: EpgManager
        function onFinished(id, ok, error) {
            if (id === page.sourceId && ok)
                page.revision++
        }
    }

    Component.onCompleted: Qt.callLater(scrollToNow)

    header: PageHeader {
        title: qsTr("TV guide")
        subtitle: page.dayText(page.dateAt(grid.contentX + grid.width / 2))
        showBack: page.showBack
        onBackClicked: page.backRequested()

        AppButton {
            variant: "secondary"
            text: qsTr("Now")
            iconName: "clock"
            implicitHeight: 38
            onClicked: page.scrollToNow()
        }
    }

    ChannelListModel {
        id: ownChannels
        sourceId: page.channelModel ? -1 : page.sourceId
    }

    // Time ruler
    Flickable {
        id: ruler
        x: page.channelColumnWidth
        width: parent.width - x
        height: page.rulerHeight
        contentWidth: page.gridWidth
        contentX: grid.contentX
        interactive: false
        clip: true

        Repeater {
            model: page.gridHours * 2
            delegate: Item {
                required property int index
                readonly property date time: new Date(page.gridStart.getTime() + index * 1800000)
                x: index * 30 * page.pxPerMinute
                width: 30 * page.pxPerMinute
                height: ruler.height

                Rectangle {
                    width: 1
                    height: parent.height
                    color: "#22FFFFFF"
                }
                Label {
                    x: 6
                    anchors.verticalCenter: parent.verticalCenter
                    text: parent.time.getHours() === 0 && parent.time.getMinutes() === 0
                          ? page.dayText(parent.time) : page.timeText(parent.time)
                    color: parent.time.getHours() === 0 && parent.time.getMinutes() === 0
                           ? AppTheme.accent : AppTheme.textSecondary
                    font.pixelSize: 12
                    font.bold: parent.time.getMinutes() === 0
                }
            }
        }
    }

    // Channel column (follows the grid vertically)
    ListView {
        id: channelColumn
        y: page.rulerHeight
        width: page.channelColumnWidth
        height: parent.height - y
        model: page.activeModel
        contentY: grid.contentY
        interactive: false
        clip: true

        delegate: ItemDelegate {
            id: channelCell
            required property int index
            required property string name
            required property string logo
            required property int number
            required property int archiveDays

            width: channelColumn.width
            height: page.rowHeight
            padding: 6
            focusPolicy: Qt.TabFocus
            onClicked: page.channelActivated(index)
            // Keyboard / remote: scroll the guide so the focused channel shows
            // (the channel column follows the program grid).
            onActiveFocusChanged: if (activeFocus) grid.positionViewAtIndex(index, ListView.Contain)
            background: Rectangle {
                color: channelCell.hovered ? AppTheme.surfaceHover : AppTheme.backgroundRaised
                FocusRing { target: channelCell; gap: 0; radius: 0 }
                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: AppTheme.background
                }
            }
            contentItem: RowLayout {
                spacing: 8
                Image {
                    source: channelCell.logo
                    asynchronous: true
                    fillMode: Image.PreserveAspectFit
                    sourceSize: Qt.size(72, 48)
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 26
                    visible: !page.narrow || status === Image.Ready
                }
                Label {
                    text: page.narrow ? channelCell.name : channelCell.number + "  " + channelCell.name
                    elide: Text.ElideRight
                    color: AppTheme.textPrimary
                    font.pixelSize: page.narrow ? 11 : 13
                    wrapMode: page.narrow ? Text.Wrap : Text.NoWrap
                    maximumLineCount: 2
                    Layout.fillWidth: true
                }
                // Catch-up channel
                IconImage {
                    visible: channelCell.archiveDays > 0
                    source: "icons/retry.svg"
                    sourceSize: Qt.size(14, 14)
                    color: AppTheme.accent2
                    ToolTip.visible: hoverTip.hovered
                    ToolTip.text: qsTr("Catch-up: %n day(s) of past programs", "", channelCell.archiveDays)
                    HoverHandler { id: hoverTip }
                }
            }
        }
    }

    // Program grid
    ListView {
        id: grid
        x: page.channelColumnWidth
        y: page.rulerHeight
        width: parent.width - x
        height: parent.height - y
        model: page.activeModel
        contentWidth: page.gridWidth
        flickableDirection: Flickable.HorizontalAndVerticalFlick
        boundsBehavior: Flickable.StopAtBounds
        keyNavigationEnabled: false // arrow keys move focus between programs
        clip: true
        cacheBuffer: page.rowHeight * 4
        ScrollBar.horizontal: AppScrollBar {}
        ScrollBar.vertical: AppScrollBar {}

        delegate: Item {
            id: row
            required property int index
            required property string name
            required property string epgKey
            required property int archiveDays

            readonly property var programs: {
                page.revision // reload after a guide update
                return row.epgKey.length > 0
                        ? EpgManager.programs(page.sourceId, row.epgKey, page.gridStart, page.gridEnd)
                        : []
            }

            width: page.gridWidth
            height: page.rowHeight

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: "#14FFFFFF"
            }

            Label {
                visible: row.programs.length === 0
                x: Math.max(grid.contentX, 0) + 12
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("No program information")
                color: "#66FFFFFF"
                font.pixelSize: 12
            }

            Repeater {
                model: row.programs

                delegate: Rectangle {
                    id: block
                    required property var modelData
                    readonly property bool onAir: modelData.start <= page.now && modelData.stop > page.now
                    readonly property bool past: modelData.stop <= page.now
                    // Can be watched from the provider's archive
                    readonly property bool replayable: (past || onAir) && row.archiveDays > 0
                                                       && page.activeModel.canCatchup(row.index, modelData.start)
                    readonly property real startX: Math.max(0, page.xFor(modelData.start))

                    x: startX + 1
                    y: 3
                    width: Math.max(2, Math.min(page.gridWidth, page.xFor(modelData.stop)) - startX - 2)
                    height: page.rowHeight - 7
                    radius: AppTheme.radiusSmall
                    color: blockArea.containsMouse ? AppTheme.surfaceHover
                         : onAir ? AppTheme.selection : AppTheme.surface
                    opacity: past && !replayable ? 0.55 : 1
                    border.color: onAir ? AppTheme.accent : "transparent"
                    border.width: 1
                    clip: true
                    // Keyboard / TV remote: arrows move between programs, OK opens one.
                    activeFocusOnTab: true
                    // Opened when OK is released: the release would otherwise
                    // reach the new dialog and close it again.
                    property bool okPressed: false
                    Keys.onPressed: (event) => {
                        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter
                                || event.key === Qt.Key_Select) {
                            okPressed = true
                            event.accepted = true
                        }
                    }
                    Keys.onReleased: (event) => {
                        if (okPressed && !event.isAutoRepeat) {
                            okPressed = false
                            event.accepted = true
                            page.showDetails(block.modelData, row.index, row.name)
                        }
                    }
                    FocusRing { target: block; gap: 0 }

                    // Keep the text visible when the block starts left of the view.
                    Column {
                        x: Math.max(8, grid.contentX - block.x + 8)
                        width: Math.max(0, block.width - x - 6)
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2
                        Row {
                            width: parent.width
                            spacing: 5
                            IconImage {
                                id: replayIcon
                                visible: block.replayable && block.past
                                anchors.verticalCenter: parent.verticalCenter
                                source: "icons/retry.svg"
                                sourceSize: Qt.size(13, 13)
                                color: AppTheme.accent2
                            }
                            Label {
                                width: parent.width - (replayIcon.visible ? replayIcon.width + 5 : 0)
                                text: block.modelData.title
                                elide: Text.ElideRight
                                color: AppTheme.textPrimary
                                font.pixelSize: 13
                                font.bold: block.onAir
                            }
                        }
                        Label {
                            width: parent.width
                            text: page.timeText(block.modelData.start) + " – "
                                  + page.timeText(block.modelData.stop)
                            elide: Text.ElideRight
                            color: AppTheme.textSecondary
                            font.pixelSize: 11
                        }
                    }

                    MouseArea {
                        id: blockArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: page.showDetails(block.modelData, row.index, row.name)
                    }
                }
            }
        }
    }

    // "Now" line
    Rectangle {
        readonly property real gridX: page.xFor(page.now) - grid.contentX
        visible: gridX >= 0 && gridX <= grid.width
        x: page.channelColumnWidth + gridX - 1
        y: page.rulerHeight - 6
        width: 2
        height: parent.height - y
        color: AppTheme.live
    }

    // No guide yet
    Pane {
        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * AppTheme.margin, 420)
        visible: !page.hasGuide
        padding: AppTheme.margin
        background: Rectangle { color: AppTheme.surface; radius: AppTheme.radius }

        ColumnLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: AppTheme.spacing

            Label {
                text: qsTr("No TV guide yet")
                font.pixelSize: 17
                font.bold: true
                color: AppTheme.textPrimary
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: AppTheme.textSecondary
                text: EpgManager.busy && EpgManager.activeSourceId === page.sourceId
                      ? EpgManager.statusText
                      : EpgManager.canHaveGuide(page.sourceId)
                        ? qsTr("The guide hasn't been downloaded yet.")
                        : qsTr("This playlist has no TV guide link. You can add one by "
                               + "removing and re-adding the provider with a TV guide (XMLTV) URL.")
            }
            ProgressBar {
                Layout.fillWidth: true
                visible: EpgManager.busy && EpgManager.activeSourceId === page.sourceId
                indeterminate: EpgManager.progress < 0
                value: Math.max(0, EpgManager.progress)
            }
            AppButton {
                text: qsTr("Download guide")
                visible: EpgManager.canHaveGuide(page.sourceId)
                         && !(EpgManager.busy && EpgManager.activeSourceId === page.sourceId)
                onClicked: EpgManager.refresh(page.sourceId)
            }
        }
    }

    // Program details
    AppDialog {
        id: details
        property var program: null
        property int channelIndex: -1
        property string channelName

        width: Math.min(page.width - 32, 480)
        title: program ? program.title : ""
        standardButtons: Dialog.Close

        ColumnLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: AppTheme.spacing

            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: AppTheme.textSecondary
                text: details.program
                      ? details.channelName + " · " + page.dayText(details.program.start) + " "
                        + page.timeText(details.program.start) + " – " + page.timeText(details.program.stop)
                      : ""
            }
            Label {
                Layout.fillWidth: true
                visible: text.length > 0
                wrapMode: Text.Wrap
                font.bold: true
                text: details.program ? details.program.subTitle : ""
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: details.program && details.program.description
                      ? details.program.description : qsTr("No description.")
            }
            Flow {
                Layout.fillWidth: true
                Layout.topMargin: 6
                spacing: 10
                // Catch-up: from the provider's archive
                AppButton {
                    id: replayButton
                    objectName: "replayButton"
                    readonly property bool available: details.program !== null
                            && details.program.start <= page.now
                            && page.activeModel.canCatchup(details.channelIndex, details.program.start)
                    visible: available
                    iconName: "play"
                    text: details.program && details.program.stop <= page.now
                          ? qsTr("Watch") : qsTr("Watch from the start")
                    onClicked: {
                        const p = details.program
                        const url = page.activeModel.catchupUrl(details.channelIndex, p.start, p.stop)
                        details.close()
                        page.catchupRequested(url, details.channelName + " · " + p.title)
                    }
                }
                AppButton {
                    variant: replayButton.visible ? "secondary" : "primary"
                    iconName: "live"
                    text: qsTr("Watch %1 live").arg(details.channelName)
                    onClicked: {
                        details.close()
                        page.channelActivated(details.channelIndex)
                    }
                }
            }
            Label {
                Layout.fillWidth: true
                visible: details.program !== null && details.program.stop <= page.now && !replayButton.available
                wrapMode: Text.Wrap
                color: AppTheme.textTertiary
                font.pixelSize: AppTheme.fontSmall
                text: qsTr("This channel has no catch-up, or the program is too old.")
            }
        }
    }
}
