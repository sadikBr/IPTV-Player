import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import IPTVPlayer

// Poster, details and play/resume buttons for one movie.
Page {
    id: page

    property int sourceId: -1
    property string extId

    property var info: VodManager.movie(sourceId, extId)
    property double resumeMs: VodManager.resumePosition(sourceId, "movie", extId)
    property string errorText: ""
    readonly property bool narrow: width < AppTheme.compactWidth
    readonly property bool loading: VodManager.isLoading(sourceId, extId) && loadingTick >= 0
    property int loadingTick: 0

    signal backRequested()
    // fromStart: ignore the saved position
    signal playRequested(bool fromStart)

    function refresh() {
        info = VodManager.movie(sourceId, extId)
        resumeMs = VodManager.resumePosition(sourceId, "movie", extId)
    }

    function formatDuration(secs) {
        if (!secs) return ""
        const h = Math.floor(secs / 3600)
        const m = Math.round((secs % 3600) / 60)
        return h > 0 ? qsTr("%1 h %2 min").arg(h).arg(m) : qsTr("%1 min").arg(m)
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

    // Backdrop
    Image {
        id: backdrop
        anchors.left: parent.left
        anchors.right: parent.right
        height: Math.min(parent.height * 0.6, width * 0.56)
        source: page.info.backdrop || ""
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        opacity: 0.35
        visible: status === Image.Ready
    }
    Rectangle {
        anchors.fill: backdrop
        visible: backdrop.visible
        gradient: Gradient {
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 1.0; color: AppTheme.background }
        }
    }

    IconButton {
        z: 2
        x: 4
        y: 4
        iconName: "back"
        tip: qsTr("Back")
        onClicked: page.backRequested()
    }

    Flickable {
        anchors.fill: parent
        contentHeight: content.implicitHeight + 2 * AppTheme.margin
        clip: true
        ScrollIndicator.vertical: ScrollIndicator {}

        GridLayout {
            id: content
            width: Math.min(page.width - 2 * AppTheme.margin, 1000)
            x: (page.width - width) / 2
            y: page.narrow ? 56 : 72
            columns: page.narrow ? 1 : 2
            columnSpacing: AppTheme.margin * 1.5
            rowSpacing: AppTheme.margin

            // Poster
            Rectangle {
                Layout.alignment: Qt.AlignTop | (page.narrow ? Qt.AlignHCenter : Qt.AlignLeft)
                Layout.preferredWidth: page.narrow ? 160 : 260
                Layout.preferredHeight: Layout.preferredWidth * 1.5
                radius: 8
                color: AppTheme.surface
                clip: true
                Image {
                    anchors.fill: parent
                    source: page.info.poster || ""
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    sourceSize.width: 520
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                spacing: AppTheme.spacing

                Label {
                    Layout.fillWidth: true
                    text: page.info.name || ""
                    wrapMode: Text.Wrap
                    font.pixelSize: page.narrow ? 24 : 32
                    font.bold: true
                    color: AppTheme.textPrimary
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    color: AppTheme.textSecondary
                    text: [page.info.year, page.formatDuration(page.info.durationSecs),
                           page.info.rating ? "★ " + page.info.rating : "", page.info.genre]
                          .filter(t => t).join("  ·  ")
                }

                RowLayout {
                    Layout.topMargin: AppTheme.spacing
                    spacing: AppTheme.spacing
                    Button {
                        objectName: "playButton"
                        highlighted: true
                        icon.source: "icons/play.svg"
                        text: page.resumeMs > 0
                              ? qsTr("Resume from %1").arg(page.formatDuration(Math.round(page.resumeMs / 1000)))
                              : qsTr("Play")
                        onClicked: page.playRequested(false)
                    }
                    Button {
                        visible: page.resumeMs > 0
                        flat: true
                        text: qsTr("From the start")
                        onClicked: page.playRequested(true)
                    }
                    IconButton {
                        iconName: page.info.favorite ? "star" : "star-outline"
                        tip: page.info.favorite ? qsTr("Remove from favorites") : qsTr("Add to favorites")
                        icon.color: page.info.favorite ? AppTheme.favorite : AppTheme.textPrimary
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
                    Layout.topMargin: AppTheme.spacing
                    visible: text.length > 0
                    text: page.info.plot || ""
                    wrapMode: Text.Wrap
                    color: AppTheme.textPrimary
                    font.pixelSize: 15
                    lineHeight: 1.2
                }
                Label {
                    Layout.fillWidth: true
                    visible: !!page.info.director
                    wrapMode: Text.Wrap
                    color: AppTheme.textSecondary
                    text: qsTr("Director: %1").arg(page.info.director || "")
                }
                Label {
                    Layout.fillWidth: true
                    visible: !!page.info.cast
                    wrapMode: Text.Wrap
                    color: AppTheme.textSecondary
                    text: qsTr("Cast: %1").arg(page.info.cast || "")
                }
            }
        }
    }

    Keys.onEscapePressed: page.backRequested()
    Keys.onBackPressed: page.backRequested()
}
