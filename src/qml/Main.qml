import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import IPTVPlayer

ApplicationWindow {
    id: window

    width: 1100
    height: 700
    minimumWidth: 320
    minimumHeight: 240
    visible: true
    title: qsTr("IPTV Player")
    color: AppTheme.background

    Material.theme: Material.Dark
    Material.accent: AppTheme.accent
    Material.primary: AppTheme.surface

    readonly property bool fullScreen: visibility === Window.FullScreen
    property int visibilityBeforeFullScreen: Window.Windowed

    function setFullScreen(on) {
        if (on === fullScreen)
            return
        if (on) {
            visibilityBeforeFullScreen = visibility
            showFullScreen()
        } else if (visibilityBeforeFullScreen === Window.Maximized) {
            showMaximized()
        } else {
            showNormal()
        }
    }

    function toggleFullScreen() {
        setFullScreen(!fullScreen)
    }

    // Plays a single URL (no channel list).
    function openStream(url, title) {
        stack.push(playerPageComponent, { "streamUrl": url, "streamTitle": title || "" })
    }

    // Shows the channel browser of a source; optionally starts a channel.
    function openSource(sourceId, autoPlayExtId) {
        AppSettings.lastSourceId = sourceId
        stack.pop(null) // back to home
        stack.push(livePageComponent, { "sourceId": sourceId,
                                        "autoPlayExtId": autoPlayExtId || "" })
    }

    function playChannel(livePage, index) {
        stack.push(playerPageComponent, { "channelModel": livePage.channelModel,
                                          "channelIndex": index,
                                          "sourceId": livePage.sourceId })
    }

    // Used by the --smoke-test mode (see main.cpp).
    function smokeAddPlaylist(url) {
        SourceManager.addM3uSource("Smoke test", url, "")
    }
    function smokeShowAddPage() {
        stack.push(addSourcePageComponent)
    }
    function smokeCloseAddPage() {
        stack.pop(null)
    }
    function smokeAddXtream(server, username, password) {
        SourceManager.addXtreamSource("Smoke test", server, username, password, "ts")
    }
    function smokePlayFirst() {
        if (stack.currentItem && stack.currentItem.playIndex)
            stack.currentItem.playIndex(0)
    }

    Connections {
        target: SourceManager
        // Open a source as soon as it has been added (from the form or the smoke test).
        function onFinished(id, ok, error, isNew) {
            if (ok && isNew)
                window.openSource(id)
        }
    }

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: HomePage {
            onPlayRequested: (url, title) => window.openStream(url, title)
            onSourceSelected: (sourceId) => window.openSource(sourceId)
            onAddSourceRequested: stack.push(addSourcePageComponent)
            onResumeRequested: (sourceId, extId) => window.openSource(sourceId, extId)
        }

        Component.onCompleted: {
            if (SourceManager.sourceExists(AppSettings.lastSourceId))
                window.openSource(AppSettings.lastSourceId)
        }
    }

    Component {
        id: livePageComponent
        LivePage {
            id: livePage
            onBackRequested: stack.pop()
            onChannelActivated: (index) => window.playChannel(livePage, index)
        }
    }

    Component {
        id: addSourcePageComponent
        AddSourcePage {
            onBackRequested: stack.pop()
        }
    }

    Component {
        id: playerPageComponent
        PlayerPage {
            appWindow: window
            onCloseRequested: {
                window.setFullScreen(false)
                stack.pop()
            }
        }
    }
}
