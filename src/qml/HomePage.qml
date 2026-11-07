import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import IPTVPlayer

// Start page: your providers, "continue watching", and playing a single URL.
Page {
    id: page

    signal playRequested(url url, string title)
    signal sourceSelected(int sourceId)
    signal addSourceRequested()
    signal resumeRequested(int sourceId, string extId)
    signal resumeVodRequested(int sourceId, string kind, string extId, string seriesExtId)

    readonly property bool resumeAvailable: AppSettings.lastChannelExtId.length > 0
            && sources.count > 0 && SourceManager.sourceExists(AppSettings.lastChannelSourceId)

    function submitUrl() {
        const url = AppSettings.normalizeUrl(urlField.text)
        if (url.toString() === "") {
            urlField.showError = true
            return
        }
        urlField.showError = false
        AppSettings.addRecentUrl(url.toString())
        page.playRequested(url, "")
    }

    function timeAgo(date) {
        if (!date || isNaN(date.getTime()))
            return qsTr("never")
        const minutes = Math.floor((Date.now() - date.getTime()) / 60000)
        if (minutes < 1) return qsTr("just now")
        if (minutes < 60) return qsTr("%1 min ago").arg(minutes)
        const hours = Math.floor(minutes / 60)
        if (hours < 24) return qsTr("%1 h ago").arg(hours)
        return qsTr("%1 days ago").arg(Math.floor(hours / 24))
    }

    function expiryText(date) {
        if (!date || isNaN(date.getTime()))
            return ""
        const days = Math.floor((date.getTime() - Date.now()) / 86400000)
        if (days < 0)
            return qsTr("Expired on %1").arg(date.toLocaleDateString(Qt.locale(), Locale.ShortFormat))
        return qsTr("Expires %1").arg(date.toLocaleDateString(Qt.locale(), Locale.ShortFormat))
               + (days === 0 ? qsTr(" (today)") : days === 1 ? qsTr(" (tomorrow)")
                  : days <= 7 ? qsTr(" (in %1 days)").arg(days) : "")
    }

    function channelCount(count) {
        return count === 1 ? qsTr("1 channel")
                           : qsTr("%1 channels").arg(Number(count).toLocaleString(Qt.locale(), "f", 0))
    }

    function expiryWarning(date) {
        return date && !isNaN(date.getTime()) && date.getTime() - Date.now() < 7 * 86400000
    }

    background: Rectangle { color: AppTheme.background }

    SourceListModel {
        id: sources
    }

    ContinueWatchingModel {
        id: continueWatching
    }

    Dialog {
        id: removeDialog
        property int sourceId: -1
        property string sourceName
        anchors.centerIn: parent
        modal: true
        title: qsTr("Remove provider?")
        standardButtons: Dialog.Cancel | Dialog.Ok
        Label {
            width: Math.min(page.width - 96, 360)
            wrapMode: Text.Wrap
            text: qsTr("“%1” and its favorites will be removed from this device.")
                    .arg(removeDialog.sourceName)
        }
        onAccepted: SourceManager.removeSource(sourceId)
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
                    Layout.fillWidth: true
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

            // Continue watching
            ItemDelegate {
                Layout.fillWidth: true
                Layout.topMargin: AppTheme.margin
                visible: page.resumeAvailable
                onClicked: page.resumeRequested(AppSettings.lastChannelSourceId,
                                                AppSettings.lastChannelExtId)
                background: Rectangle {
                    color: parent.hovered ? Qt.lighter(AppTheme.surface, 1.3) : AppTheme.surface
                    radius: AppTheme.radius
                    border.color: AppTheme.accent
                    border.width: 1
                }
                contentItem: RowLayout {
                    spacing: AppTheme.spacing
                    IconButton {
                        iconName: "play"
                        icon.color: AppTheme.accent
                        onClicked: page.resumeRequested(AppSettings.lastChannelSourceId,
                                                        AppSettings.lastChannelExtId)
                    }
                    ColumnLayout {
                        spacing: 0
                        Layout.fillWidth: true
                        Label {
                            text: qsTr("Last channel")
                            color: AppTheme.textSecondary
                            font.pixelSize: 12
                        }
                        Label {
                            text: AppSettings.lastChannelName
                            color: AppTheme.textPrimary
                            font.pixelSize: 17
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                }
            }

            // Unfinished movies and episodes
            Label {
                Layout.topMargin: AppTheme.margin
                visible: continueWatching.count > 0
                text: qsTr("Continue watching")
                font.pixelSize: 18
                font.bold: true
                color: AppTheme.textPrimary
            }
            ListView {
                id: continueList
                Layout.fillWidth: true
                Layout.preferredHeight: 280
                visible: continueWatching.count > 0
                orientation: ListView.Horizontal
                spacing: AppTheme.spacing
                clip: true
                model: continueWatching
                ScrollIndicator.horizontal: ScrollIndicator {}

                delegate: PosterCard {
                    required property int sourceId
                    required property string kind
                    required property string extId
                    required property string seriesExtId
                    required property string image
                    required property var model // for the "subtitle" role (name clash)
                    required property int remainingMinutes
                    required watched
                    required title

                    width: 130
                    height: continueList.height
                    poster: image
                    subtitle: [model.subtitle, qsTr("%1 min left").arg(remainingMinutes)]
                              .filter(t => t).join(" · ")
                    onClicked: page.resumeVodRequested(sourceId, kind, extId, seriesExtId)
                }
            }

            // Providers
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: AppTheme.margin
                Label {
                    text: qsTr("Your providers")
                    font.pixelSize: 18
                    font.bold: true
                    color: AppTheme.textPrimary
                    Layout.fillWidth: true
                }
                Button {
                    text: qsTr("Add")
                    icon.source: "icons/add.svg"
                    visible: sources.count > 0
                    onClicked: page.addSourceRequested()
                }
            }

            // No providers yet
            Pane {
                Layout.fillWidth: true
                visible: sources.count === 0
                padding: AppTheme.margin * 1.5
                background: Rectangle { color: AppTheme.surface; radius: AppTheme.radius }

                ColumnLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    spacing: AppTheme.spacing
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        horizontalAlignment: Text.AlignHCenter
                        color: AppTheme.textSecondary
                        text: qsTr("Add your IPTV provider to browse and watch channels. "
                                   + "You need either an Xtream Codes login (server, username, "
                                   + "password) or an M3U playlist link.")
                    }
                    Button {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Add provider")
                        highlighted: true
                        onClicked: page.addSourceRequested()
                    }
                }
            }

            Repeater {
                model: sources

                delegate: ItemDelegate {
                    id: sourceCard

                    required property int sourceId
                    required property string name
                    required property string type
                    required property int liveCount
                    required property var lastSync
                    required property var expiresAt
                    required property string status
                    required property int epgCount
                    required property var epgLastSync
                    readonly property bool syncing: SourceManager.busy
                                                    && SourceManager.activeSourceId === sourceId

                    Layout.fillWidth: true
                    padding: AppTheme.margin
                    rightPadding: 4
                    onClicked: page.sourceSelected(sourceId)

                    background: Rectangle {
                        color: sourceCard.hovered ? Qt.lighter(AppTheme.surface, 1.3) : AppTheme.surface
                        radius: AppTheme.radius
                    }

                    contentItem: RowLayout {
                        spacing: AppTheme.spacing

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            RowLayout {
                                spacing: 8
                                Layout.fillWidth: true
                                Label {
                                    text: sourceCard.name
                                    font.pixelSize: 17
                                    font.bold: true
                                    color: AppTheme.textPrimary
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Rectangle {
                                    radius: 4
                                    color: "#22FFFFFF"
                                    implicitWidth: typeLabel.implicitWidth + 10
                                    implicitHeight: typeLabel.implicitHeight + 4
                                    Label {
                                        id: typeLabel
                                        anchors.centerIn: parent
                                        text: sourceCard.type === "xtream" ? "Xtream" : "M3U"
                                        color: AppTheme.textSecondary
                                        font.pixelSize: 11
                                    }
                                }
                            }
                            Label {
                                text: sourceCard.syncing
                                      ? SourceManager.statusText
                                      : page.channelCount(sourceCard.liveCount) + " · "
                                        + qsTr("updated %1").arg(page.timeAgo(sourceCard.lastSync))
                                        + (sourceCard.epgCount > 0
                                           ? " · " + qsTr("TV guide %1").arg(page.timeAgo(sourceCard.epgLastSync))
                                           : "")
                                color: AppTheme.textSecondary
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Label {
                                visible: text.length > 0
                                text: {
                                    const parts = []
                                    if (sourceCard.status && sourceCard.status !== "Active")
                                        parts.push(sourceCard.status)
                                    const expiry = page.expiryText(sourceCard.expiresAt)
                                    if (expiry)
                                        parts.push(expiry)
                                    return parts.join(" · ")
                                }
                                color: page.expiryWarning(sourceCard.expiresAt)
                                       || (sourceCard.status && sourceCard.status !== "Active")
                                       ? AppTheme.error : AppTheme.textSecondary
                                font.pixelSize: 12
                            }
                            ProgressBar {
                                visible: sourceCard.syncing
                                Layout.fillWidth: true
                                indeterminate: SourceManager.progress < 0
                                value: Math.max(0, SourceManager.progress)
                            }
                        }

                        IconButton {
                            iconName: "more"
                            tip: qsTr("More")
                            icon.color: AppTheme.textSecondary
                            onClicked: sourceMenu.popup()

                            Menu {
                                id: sourceMenu
                                MenuItem {
                                    text: qsTr("Update channel list")
                                    enabled: !SourceManager.busy
                                    onTriggered: SourceManager.refresh(sourceCard.sourceId)
                                }
                                MenuItem {
                                    text: qsTr("Update TV guide")
                                    enabled: EpgManager.canHaveGuide(sourceCard.sourceId)
                                    onTriggered: EpgManager.refresh(sourceCard.sourceId)
                                }
                                MenuItem {
                                    text: qsTr("Remove")
                                    onTriggered: {
                                        removeDialog.sourceId = sourceCard.sourceId
                                        removeDialog.sourceName = sourceCard.name
                                        removeDialog.open()
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Play a single URL
            Pane {
                Layout.fillWidth: true
                Layout.topMargin: 2 * AppTheme.margin
                padding: AppTheme.margin
                background: Rectangle { color: AppTheme.surface; radius: AppTheme.radius }

                ColumnLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    spacing: AppTheme.spacing

                    Label {
                        text: qsTr("Play a stream URL")
                        font.pixelSize: 16
                        font.bold: true
                        color: AppTheme.textPrimary
                    }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        color: AppTheme.textSecondary
                        text: qsTr("Paste a single .m3u8 or .ts link to test a stream.")
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
                            onAccepted: page.submitUrl()
                            onTextChanged: showError = false
                        }
                        Button {
                            text: qsTr("Play")
                            enabled: urlField.text.trim().length > 0
                            Layout.fillWidth: page.width < AppTheme.compactWidth
                            onClicked: page.submitUrl()
                        }
                    }

                    Label {
                        visible: urlField.showError
                        text: qsTr("That doesn't look like a valid URL.")
                        color: AppTheme.error
                    }

                    // Recent URLs
                    Repeater {
                        model: AppSettings.recentUrls

                        delegate: ItemDelegate {
                            id: recentItem

                            required property string modelData

                            Layout.fillWidth: true
                            padding: 4
                            onClicked: {
                                AppSettings.addRecentUrl(modelData)
                                page.playRequested(modelData, "")
                            }
                            contentItem: RowLayout {
                                spacing: AppTheme.spacing
                                Label {
                                    text: recentItem.modelData
                                    elide: Text.ElideMiddle
                                    color: AppTheme.textSecondary
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
                }
            }
        }
    }
}
