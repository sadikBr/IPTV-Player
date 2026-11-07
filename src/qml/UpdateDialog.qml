import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import IPTVPlayer

// "A new version is available": what's new, then download and install.
Popup {
    id: root

    readonly property string state: UpdateManager.state
    readonly property bool busy: state === "downloading" || state === "installing"

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(parent ? parent.width - 32 : 480, 480)
    modal: true
    padding: 24
    closePolicy: busy ? Popup.NoAutoClose : Popup.CloseOnEscape | Popup.CloseOnPressOutside

    Overlay.modal: Rectangle { color: "#99000000" }

    background: Rectangle {
        radius: AppTheme.radiusLarge
        color: AppTheme.surface
        border.width: 1
        border.color: AppTheme.strokeStrong
    }

    contentItem: ColumnLayout {
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            spacing: 14
            Rectangle {
                Layout.preferredWidth: 44
                Layout.preferredHeight: 44
                radius: 12
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: AppTheme.accentStrong }
                    GradientStop { position: 1.0; color: AppTheme.accent2 }
                }
                IconImage {
                    anchors.centerIn: parent
                    source: "icons/play.svg"
                    sourceSize: Qt.size(22, 22)
                    color: "white"
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Update available")
                    font.pixelSize: AppTheme.fontH2
                    font.weight: Font.Bold
                    color: AppTheme.textPrimary
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: qsTr("Version %1 · you have %2")
                            .arg(UpdateManager.latestVersion).arg(UpdateManager.currentVersion)
                    color: AppTheme.textSecondary
                }
            }
        }

        // What's new (release notes, Markdown)
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(notes.implicitHeight + 24, 240)
            visible: UpdateManager.releaseNotes.length > 0
            radius: AppTheme.radius
            color: AppTheme.backgroundRaised
            Flickable {
                id: notesFlick
                anchors.fill: parent
                anchors.margins: 12
                clip: true
                contentHeight: notes.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: AppScrollBar {}
                Label {
                    id: notes
                    width: notesFlick.width - 8
                    wrapMode: Text.Wrap
                    textFormat: Text.MarkdownText
                    linkColor: AppTheme.accent2
                    text: UpdateManager.releaseNotes
                    color: AppTheme.textSecondary
                    font.pixelSize: AppTheme.fontSmall
                    onLinkActivated: (link) => Qt.openUrlExternally(link)
                }
            }
        }

        // Download progress
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: root.busy
            Rectangle {
                Layout.fillWidth: true
                height: 6
                radius: 3
                color: AppTheme.surfaceHigh
                Rectangle {
                    width: parent.width * (root.state === "installing" ? 1 : UpdateManager.progress)
                    height: parent.height
                    radius: 3
                    color: AppTheme.accent
                }
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: AppTheme.textTertiary
                font.pixelSize: AppTheme.fontCaption
                text: root.state === "installing"
                      ? (UpdateManager.installKind === "android"
                         ? qsTr("Opening the installer…")
                         : qsTr("Installing, the app restarts by itself…"))
                      : qsTr("Downloading… %1%").arg(Math.round(UpdateManager.progress * 100))
            }
        }

        Label {
            Layout.fillWidth: true
            visible: root.state === "error"
            wrapMode: Text.Wrap
            color: AppTheme.error
            text: UpdateManager.errorString
        }

        Label {
            Layout.fillWidth: true
            visible: !UpdateManager.canInstall && !root.busy
            wrapMode: Text.Wrap
            color: AppTheme.textTertiary
            font.pixelSize: AppTheme.fontSmall
            text: UpdateManager.installKind === "source"
                  ? qsTr("You built this copy yourself: update with git pull and rebuild, "
                         + "or get the AppImage from the release page.")
                  : qsTr("This copy can't update itself (portable version). "
                         + "Download the new version from the release page.")
        }

        // Actions
        Flow {
            Layout.fillWidth: true
            Layout.topMargin: 4
            spacing: 10
            layoutDirection: Qt.RightToLeft
            AppButton {
                objectName: "updateNowButton"
                visible: !root.busy
                variant: "accent"
                text: UpdateManager.canInstall
                      ? (root.state === "error" ? qsTr("Try again") : qsTr("Update now"))
                      : qsTr("Open release page")
                onClicked: {
                    UpdateManager.install()
                    if (!UpdateManager.canInstall)
                        root.close()
                }
            }
            AppButton {
                visible: root.state === "downloading"
                variant: "secondary"
                text: qsTr("Cancel")
                onClicked: UpdateManager.cancel()
            }
            AppButton {
                visible: !root.busy
                variant: "secondary"
                text: qsTr("Later")
                onClicked: {
                    UpdateManager.later()
                    root.close()
                }
            }
            AppButton {
                visible: !root.busy
                variant: "ghost"
                text: qsTr("Skip this version")
                onClicked: {
                    UpdateManager.skipVersion()
                    root.close()
                }
            }
        }
    }
}
