import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import IPTVPlayer

Page {
    id: page

    signal playRequested(url url, string title)

    background: Rectangle { color: AppTheme.background }

    function submit() {
        const url = AppSettings.normalizeUrl(urlField.text)
        if (url.toString() === "") {
            urlField.showError = true
            return
        }
        urlField.showError = false
        AppSettings.addRecentUrl(url.toString())
        page.playRequested(url, "")
    }

    Flickable {
        anchors.fill: parent
        contentHeight: content.implicitHeight + 2 * AppTheme.margin
        clip: true
        ScrollIndicator.vertical: ScrollIndicator {}

        ColumnLayout {
            id: content
            width: Math.min(page.width - 2 * AppTheme.margin, AppTheme.contentMaxWidth)
            x: (page.width - width) / 2
            y: AppTheme.margin
            spacing: AppTheme.spacing

            // Header
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: AppTheme.margin
                spacing: AppTheme.spacing

                Image {
                    source: "icons/tv.svg"
                    sourceSize: Qt.size(40, 40)
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                }
                ColumnLayout {
                    spacing: 0
                    Label {
                        text: qsTr("IPTV Player")
                        font.pixelSize: 26
                        font.bold: true
                        color: AppTheme.textPrimary
                    }
                    Label {
                        text: qsTr("Version %1").arg(AppSettings.appVersion)
                        color: AppTheme.textSecondary
                    }
                }
            }

            // Open a stream
            Pane {
                Layout.fillWidth: true
                Layout.topMargin: AppTheme.margin
                padding: AppTheme.margin
                background: Rectangle { color: AppTheme.surface; radius: AppTheme.radius }

                ColumnLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    spacing: AppTheme.spacing

                    Label {
                        text: qsTr("Play a stream")
                        font.pixelSize: 18
                        font.bold: true
                        color: AppTheme.textPrimary
                    }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        color: AppTheme.textSecondary
                        text: qsTr("Paste a stream URL (for example a .m3u8 or .ts link from your provider). "
                                   + "Use this to check which of your provider's streams play well.")
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: page.width < AppTheme.compactWidth ? 1 : 2
                        columnSpacing: AppTheme.spacing
                        rowSpacing: AppTheme.spacing

                        TextField {
                            id: urlField

                            property bool showError: false

                            objectName: "urlField"
                            Layout.fillWidth: true
                            placeholderText: "http://host:port/live/user/pass/12345.m3u8"
                            inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase
                                              | Qt.ImhNoPredictiveText
                            selectByMouse: true
                            onAccepted: page.submit()
                            onTextChanged: showError = false
                        }
                        Button {
                            text: qsTr("Play")
                            highlighted: true
                            enabled: urlField.text.trim().length > 0
                            Layout.fillWidth: page.width < AppTheme.compactWidth
                            onClicked: page.submit()
                        }
                    }

                    Label {
                        visible: urlField.showError
                        text: qsTr("That doesn't look like a valid URL.")
                        color: AppTheme.error
                    }
                }
            }

            // Recent streams
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: AppTheme.margin
                visible: AppSettings.recentUrls.length > 0

                Label {
                    text: qsTr("Recent")
                    font.pixelSize: 18
                    font.bold: true
                    color: AppTheme.textPrimary
                    Layout.fillWidth: true
                }
                Button {
                    text: qsTr("Clear")
                    flat: true
                    onClicked: AppSettings.clearRecentUrls()
                }
            }

            Repeater {
                model: AppSettings.recentUrls

                delegate: ItemDelegate {
                    id: recentItem

                    required property string modelData

                    Layout.fillWidth: true
                    onClicked: {
                        AppSettings.addRecentUrl(modelData)
                        page.playRequested(modelData, "")
                    }
                    background: Rectangle {
                        color: recentItem.hovered ? Qt.lighter(AppTheme.surface, 1.3) : AppTheme.surface
                        radius: AppTheme.radius
                    }
                    contentItem: RowLayout {
                        spacing: AppTheme.spacing
                        Label {
                            text: recentItem.modelData
                            elide: Text.ElideMiddle
                            color: AppTheme.textPrimary
                            Layout.fillWidth: true
                        }
                        IconButton {
                            iconName: "close"
                            tip: qsTr("Remove")
                            icon.color: AppTheme.textSecondary
                            onClicked: AppSettings.removeRecentUrl(recentItem.modelData)
                        }
                    }
                }
            }

            // Roadmap hint
            Label {
                Layout.fillWidth: true
                Layout.topMargin: 2 * AppTheme.margin
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                color: AppTheme.textSecondary
                text: qsTr("Coming next: add your provider (Xtream login or M3U playlist) "
                           + "and browse channels, movies and series.")
            }
        }
    }
}
