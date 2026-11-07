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

    function openStream(url, title) {
        stack.push(playerPageComponent, { "streamUrl": url, "streamTitle": title || "" })
    }

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: HomePage {
            onPlayRequested: (url, title) => window.openStream(url, title)
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
