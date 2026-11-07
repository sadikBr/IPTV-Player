import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import IPTVPlayer

// Cinematic movie page: full-width backdrop, poster, details, play/resume.
Page {
    id: page

    // Keyboard / TV remote focus starts here.
    readonly property Item initialFocusItem: playButton

    property int sourceId: -1
    property string extId

    property var info: VodManager.movie(sourceId, extId)
    property double resumeMs: VodManager.resumePosition(sourceId, "movie", extId)
    property string errorText: ""
    readonly property bool narrow: width < AppTheme.compactWidth
    readonly property real sideMargin: narrow ? AppTheme.margin : AppTheme.pageMargin
    readonly property bool loading: VodManager.isLoading(sourceId, extId) && loadingTick >= 0
    property int loadingTick: 0

    signal backRequested()
    // fromStart: ignore the saved position
    signal playRequested(bool fromStart)

    function refresh() {
        info = VodManager.movie(sourceId, extId)
        resumeMs = VodManager.resumePosition(sourceId, "movie", extId)
    }

    background: Rectangle { color: AppTheme.background }

    Component.onCompleted: VodManager.fetchMovie(sourceId, extId)
    StackView.onActivated: refresh() // back from the player: update "Resume"

    Connections {
        target: VodManager
        function onDetailsLoaded(id, ext, ok, error) {
            if (id === page.sourceId && ext === page.extId) {
                page.refresh()
                page.errorText = ok ? "" : error
            }
        }
        function onLoadingChanged() { page.loadingTick++ }
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentHeight: content.y + content.implicitHeight + 48
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: AppScrollBar {}

        DetailsBackdrop {
            id: backdrop
            width: flick.width
            height: page.narrow ? 300 : Math.max(420, Math.min(page.height * 0.78, flick.width * 0.5))
            backdrop: page.info.backdrop || ""
            poster: page.info.poster || ""
        }

        GridLayout {
            id: content
            x: page.sideMargin
            y: page.narrow ? 180 : Math.max(120, backdrop.height - 340)
            width: Math.min(flick.width - 2 * page.sideMargin, 1180)
            columns: page.narrow ? 1 : 2
            columnSpacing: 40
            rowSpacing: AppTheme.margin

            // Poster
            Item {
                Layout.alignment: Qt.AlignTop | (page.narrow ? Qt.AlignHCenter : Qt.AlignLeft)
                Layout.preferredWidth: page.narrow ? 150 : 250
                Layout.preferredHeight: Layout.preferredWidth * 1.5
                Shadow { target: posterFrame; spread: 30 }
                Rectangle {
                    id: posterFrame
                    anchors.fill: parent
                    radius: AppTheme.radius
                    color: AppTheme.surfaceHigh
                    clip: true
                    Image {
                        anchors.fill: parent
                        source: page.info.poster || ""
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        sourceSize.width: 500
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                Layout.topMargin: page.narrow ? 0 : 40
                spacing: 14

                Label {
                    text: qsTr("MOVIE")
                    color: AppTheme.accent
                    font.pixelSize: AppTheme.fontCaption
                    font.weight: Font.Bold
                    font.letterSpacing: 2
                }
                Label {
                    Layout.fillWidth: true
                    text: page.info.name || ""
                    wrapMode: Text.Wrap
                    font.pixelSize: page.narrow ? 28 : AppTheme.fontHero
                    font.weight: Font.Bold
                    font.letterSpacing: -0.5
                    lineHeight: 1.05
                    color: "white"
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    MetaPill { text: page.info.year || "" }
                    MetaPill { text: AppTheme.duration(page.info.durationSecs); iconName: "clock" }
                    MetaPill {
                        text: page.info.rating || ""
                        iconName: "star"
                        iconColor: AppTheme.favorite
                    }
                    MetaPill { text: page.info.genre || "" }
                }

                RowLayout {
                    Layout.topMargin: 8
                    spacing: 12
                    AppButton {
                        id: playButton
                        objectName: "playButton"
                        iconName: "play"
                        text: page.resumeMs > 0
                              ? qsTr("Resume from %1").arg(AppTheme.duration(Math.round(page.resumeMs / 1000)))
                              : qsTr("Play")
                        onClicked: page.playRequested(false)
                    }
                    AppButton {
                        visible: page.resumeMs > 0
                        variant: "secondary"
                        iconName: "retry"
                        text: qsTr("From the start")
                        onClicked: page.playRequested(true)
                    }
                    IconButton {
                        size: 44
                        filled: true
                        iconName: page.info.favorite ? "heart" : "heart-outline"
                        iconColor: page.info.favorite ? AppTheme.accent : AppTheme.textPrimary
                        tip: page.info.favorite ? qsTr("Remove from My movies") : qsTr("Add to My movies")
                        onClicked: {
                            VodManager.setFavorite(page.sourceId, "movie", page.extId, !page.info.favorite)
                            page.refresh()
                        }
                    }
                }

                BusyIndicator {
                    visible: page.loading && !page.info.plot
                    running: visible
                }
                Label {
                    Layout.fillWidth: true
                    visible: page.errorText.length > 0
                    wrapMode: Text.Wrap
                    color: AppTheme.error
                    text: qsTr("Couldn't load details: %1").arg(page.errorText)
                }
                Label {
                    Layout.fillWidth: true
                    Layout.maximumWidth: 760
                    Layout.topMargin: 6
                    visible: text.length > 0
                    text: page.info.plot || ""
                    wrapMode: Text.Wrap
                    color: "#D0D0D6"
                    font.pixelSize: AppTheme.fontH3
                    lineHeight: 1.35
                }

                GridLayout {
                    Layout.topMargin: 6
                    Layout.maximumWidth: 760
                    columns: 2
                    columnSpacing: 16
                    rowSpacing: 6
                    visible: !!page.info.director || !!page.info.cast
                    Label {
                        visible: !!page.info.director
                        text: qsTr("Director")
                        color: AppTheme.textTertiary
                        Layout.alignment: Qt.AlignTop
                    }
                    Label {
                        visible: !!page.info.director
                        Layout.fillWidth: true
                        text: page.info.director || ""
                        wrapMode: Text.Wrap
                        color: AppTheme.textSecondary
                    }
                    Label {
                        visible: !!page.info.cast
                        text: qsTr("Cast")
                        color: AppTheme.textTertiary
                        Layout.alignment: Qt.AlignTop
                    }
                    Label {
                        visible: !!page.info.cast
                        Layout.fillWidth: true
                        text: page.info.cast || ""
                        wrapMode: Text.Wrap
                        color: AppTheme.textSecondary
                    }
                }
            }
        }
    }

    IconButton {
        x: 16
        y: 16
        filled: true
        iconName: "back"
        tip: qsTr("Back")
        onClicked: page.backRequested()
    }

    Keys.onEscapePressed: page.backRequested()
    Keys.onBackPressed: page.backRequested()
}
