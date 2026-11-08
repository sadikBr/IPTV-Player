import QtQuick
import QtQuick.Controls
import IPTVPlayer

// Dialog in the app's style: dark card, bold title, accent buttons.
Dialog {
    id: control

    // Always centered in the window, wherever the dialog is declared.
    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    padding: 24
    topPadding: 8

    Overlay.modal: Rectangle { color: "#99000000" }

    background: Rectangle {
        radius: AppTheme.radiusLarge
        color: AppTheme.surface
        border.width: 1
        border.color: AppTheme.strokeStrong
    }

    header: Label {
        text: control.title
        visible: text.length > 0
        padding: 24
        bottomPadding: 8
        wrapMode: Text.Wrap
        font.pixelSize: AppTheme.fontH2
        font.weight: Font.Bold
        color: AppTheme.textPrimary
    }

    footer: DialogButtonBox {
        padding: 16
        topPadding: 0
        background: Item {}
    }
}
