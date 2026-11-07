import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtMultimedia
import IPTVPlayer

Page {
    id: page

    property url streamUrl
    property string streamTitle
    property ApplicationWindow appWindow

    readonly property bool fullScreen: appWindow ? appWindow.fullScreen : false
    property bool controlsVisible: true
    property bool infoVisible: false

    signal closeRequested()

    function showControls() {
        controlsVisible = true
        hideTimer.restart()
    }

    function toggleFullScreen() {
        if (appWindow)
            appWindow.toggleFullScreen()
        showControls()
    }

    function goBack() {
        if (fullScreen)
            appWindow.setFullScreen(false)
        else
            page.closeRequested()
    }

    function changeVolume(delta) {
        player.muted = false
        player.volume = Math.max(0, Math.min(1, player.volume + delta))
        showControls()
    }

    focus: true
    background: Rectangle { color: "black" }

    PlayerController {
        id: player
        onVolumeChanged: AppSettings.volume = volume
    }

    Component.onCompleted: {
        player.videoOutput = videoOutput
        player.volume = AppSettings.volume
        player.open(streamUrl)
        forceActiveFocus()
        showControls()
    }

    Timer {
        id: hideTimer
        interval: 3500
        onTriggered: {
            if (player.playing && !controls.interacting)
                page.controlsVisible = false
        }
    }

    VideoOutput {
        id: videoOutput
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
    }

    // Mouse / touch: move or tap shows controls; tap again hides; double-click toggles fullscreen.
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: page.controlsVisible ? Qt.ArrowCursor : Qt.BlankCursor
        onPositionChanged: if (containsMouse && !pressed) page.showControls()
        onClicked: {
            if (page.controlsVisible && player.playing) {
                page.controlsVisible = false
                hideTimer.stop()
            } else {
                page.showControls()
            }
        }
        onDoubleClicked: page.toggleFullScreen()
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: player.busy
        visible: running
    }

    // Error state
    Pane {
        anchors.centerIn: parent
        width: Math.min(page.width - 2 * AppTheme.margin, 460)
        visible: player.failed
        padding: AppTheme.margin
        background: Rectangle { color: AppTheme.surface; radius: AppTheme.radius }

        ColumnLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: AppTheme.spacing

            Label {
                text: qsTr("Can't play this stream")
                font.pixelSize: 18
                font.bold: true
                color: AppTheme.textPrimary
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: AppTheme.textSecondary
                text: player.errorString
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                Button {
                    text: qsTr("Back")
                    flat: true
                    onClicked: page.closeRequested()
                }
                Button {
                    text: qsTr("Retry")
                    highlighted: true
                    onClicked: player.retry()
                }
            }
        }
    }

    // Technical stream details, toggled with the (i) button or the I key.
    Pane {
        id: infoPanel
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 72
        anchors.rightMargin: AppTheme.margin
        width: Math.min(page.width - 2 * AppTheme.margin, 380)
        visible: page.infoVisible
        padding: AppTheme.spacing
        background: Rectangle { color: AppTheme.overlay; radius: AppTheme.radius }

        Label {
            width: parent.width
            wrapMode: Text.Wrap
            color: AppTheme.textPrimary
            font.family: "monospace"
            text: player.streamInfo.length > 0 ? player.streamInfo : qsTr("Waiting for stream…")
        }
    }

    PlayerControls {
        id: controls
        anchors.fill: parent
        player: player
        title: page.streamTitle.length > 0 ? page.streamTitle : page.streamUrl.toString()
        fullScreen: page.fullScreen
        infoVisible: page.infoVisible
        opacity: page.controlsVisible || !player.playing ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 200 } }

        onBackClicked: page.goBack()
        onFullScreenClicked: page.toggleFullScreen()
        onInfoClicked: page.infoVisible = !page.infoVisible
        onActivity: page.showControls()
    }

    Keys.onPressed: (event) => {
        switch (event.key) {
        case Qt.Key_Space:
        case Qt.Key_K:
        case Qt.Key_MediaTogglePlayPause:
            player.togglePause(); page.showControls(); break
        case Qt.Key_F:
        case Qt.Key_F11:
            page.toggleFullScreen(); break
        case Qt.Key_M:
            player.muted = !player.muted; page.showControls(); break
        case Qt.Key_I:
            page.infoVisible = !page.infoVisible; break
        case Qt.Key_Up:
        case Qt.Key_VolumeUp:
            page.changeVolume(0.05); break
        case Qt.Key_Down:
        case Qt.Key_VolumeDown:
            page.changeVolume(-0.05); break
        case Qt.Key_Left:
            player.seekRelative(-10000); page.showControls(); break
        case Qt.Key_Right:
            player.seekRelative(10000); page.showControls(); break
        case Qt.Key_Escape:
        case Qt.Key_Back:
        case Qt.Key_Backspace:
            page.goBack(); break
        default:
            return
        }
        event.accepted = true
    }
}
