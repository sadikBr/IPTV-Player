import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import IPTVPlayer

// TV guide grid: channels down the side, time across the top.
Page {
    id: page

    property ChannelListModel channelModel: null
    property int sourceId: -1
    property string sourceName

    signal backRequested()
    signal channelActivated(int index)

    readonly property bool narrow: width < AppTheme.compactWidth
    readonly property real pxPerMinute: narrow ? 3 : 4.5
    readonly property int rowHeight: 60
    readonly property int channelColumnWidth: narrow ? 92 : 210
    readonly property int rulerHeight: 34
    readonly property int gridHours: 72

    // The grid starts 2 hours before the hour it was opened.
    property date gridStart: {
        const d = new Date()
        d.setMinutes(0, 0, 0)
        d.setHours(d.getHours() - 2)
        return d
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

    header: ToolBar {
        Material.background: AppTheme.surface
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 4
            anchors.rightMargin: 8
            spacing: 4

            IconButton {
                iconName: "back"
                tip: qsTr("Back")
                onClicked: page.backRequested()
            }
            Label {
                text: qsTr("TV guide")
                font.pixelSize: 18
                font.bold: true
                color: AppTheme.textPrimary
            }
            Label {
                text: page.dayText(page.dateAt(grid.contentX + grid.width / 2))
                color: AppTheme.textSecondary
                Layout.leftMargin: AppTheme.spacing
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            Button {
                text: qsTr("Now")
                flat: true
                onClicked: page.scrollToNow()
            }
        }
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
        model: page.channelModel
        contentY: grid.contentY
        interactive: false
        clip: true

        delegate: ItemDelegate {
            id: channelCell
            required property int index
            required property string name
            required property string logo
            required property int number

            width: channelColumn.width
            height: page.rowHeight
            padding: 6
            onClicked: page.channelActivated(index)
            background: Rectangle {
                color: channelCell.hovered ? "#14FFFFFF" : AppTheme.surface
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
        model: page.channelModel
        contentWidth: page.gridWidth
        flickableDirection: Flickable.HorizontalAndVerticalFlick
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        cacheBuffer: page.rowHeight * 4
        ScrollBar.horizontal: ScrollBar {}
        ScrollBar.vertical: ScrollBar {}

        delegate: Item {
            id: row
            required property int index
            required property string name
            required property string epgKey

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
                    readonly property real startX: Math.max(0, page.xFor(modelData.start))

                    x: startX + 1
                    y: 3
                    width: Math.max(2, Math.min(page.gridWidth, page.xFor(modelData.stop)) - startX - 2)
                    height: page.rowHeight - 7
                    radius: 4
                    color: blockArea.containsMouse ? Qt.lighter(AppTheme.surface, 1.5)
                         : onAir ? "#2A4FC3F7" : AppTheme.surface
                    border.color: onAir ? AppTheme.accent : "transparent"
                    border.width: 1
                    clip: true

                    // Keep the text visible when the block starts left of the view.
                    Column {
                        x: Math.max(8, grid.contentX - block.x + 8)
                        width: Math.max(0, block.width - x - 6)
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2
                        Label {
                            width: parent.width
                            text: block.modelData.title
                            elide: Text.ElideRight
                            color: AppTheme.textPrimary
                            font.pixelSize: 13
                            font.bold: block.onAir
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
        color: AppTheme.error
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
            Button {
                text: qsTr("Download guide")
                visible: EpgManager.canHaveGuide(page.sourceId)
                         && !(EpgManager.busy && EpgManager.activeSourceId === page.sourceId)
                onClicked: EpgManager.refresh(page.sourceId)
            }
        }
    }

    // Program details
    Dialog {
        id: details
        property var program: null
        property int channelIndex: -1
        property string channelName

        anchors.centerIn: parent
        width: Math.min(page.width - 32, 480)
        modal: true
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
            Button {
                text: qsTr("Watch %1").arg(details.channelName)
                highlighted: true
                onClicked: {
                    details.close()
                    page.channelActivated(details.channelIndex)
                }
            }
        }
    }
}
