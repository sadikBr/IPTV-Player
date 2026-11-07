import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import IPTVPlayer

// Providers, network/playback options, single-URL player and about.
Page {
    id: page

    readonly property bool narrow: width < AppTheme.compactWidth
    readonly property real sideMargin: narrow ? AppTheme.margin : AppTheme.pageMargin

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

    function shiftText(minutes) {
        if (!minutes)
            return qsTr("none")
        const sign = minutes > 0 ? "+" : "−"
        const abs = Math.abs(minutes)
        const h = Math.floor(abs / 60)
        const m = abs % 60
        return sign + (h > 0 ? qsTr("%1 h").arg(h) : "") + (h > 0 && m > 0 ? " " : "")
                + (m > 0 ? qsTr("%1 min").arg(m) : "")
    }

    function count(n, one, many) {
        return n === 1 ? one : many.arg(Number(n).toLocaleString(Qt.locale(), "f", 0))
    }

    function submitUrl() {
        const url = AppSettings.normalizeUrl(urlField.text)
        if (url.toString() === "") {
            urlError.visible = true
            return
        }
        urlError.visible = false
        AppSettings.addRecentUrl(url.toString())
        AppState.playStreamRequested(url, "")
    }

    background: Rectangle { color: AppTheme.background }

    header: PageHeader {
        title: qsTr("Settings")
    }

    SourceListModel {
        id: sources
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentHeight: content.implicitHeight + 40
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: AppScrollBar {}

        ColumnLayout {
            id: content
            x: page.sideMargin
            width: Math.min(flick.width - 2 * page.sideMargin, 860)
            spacing: 28

            // --- Providers ---
            Section {
                title: qsTr("Providers")
                description: qsTr("Your IPTV subscriptions. Lists update once a day and the TV guide every 12 hours.")

                Repeater {
                    model: sources
                    delegate: ProviderRow {}
                }

                AppButton {
                    variant: sources.count > 0 ? "secondary" : "primary"
                    text: qsTr("Add provider")
                    iconName: "add"
                    onClicked: AppState.addProviderRequested()
                }
            }

            // --- Network ---
            Section {
                title: qsTr("Network")
                description: qsTr("Some providers only answer known players. The app identifies "
                                   + "itself as VLC unless you enter something else here.")

                Card {
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: AppTheme.spacing
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Label {
                                text: qsTr("User-Agent")
                                font.weight: Font.DemiBold
                                color: AppTheme.textPrimary
                            }
                            AppTextField {
                                id: userAgentField
                                Layout.fillWidth: true
                                text: AppSettings.userAgent
                                placeholderText: AppSettings.defaultUserAgent
                                selectByMouse: true
                                onEditingFinished: AppSettings.userAgent = text
                            }
                        }
                        AppButton {
                            variant: "ghost"
                            text: qsTr("Default")
                            enabled: AppSettings.userAgent.length > 0
                            onClicked: {
                                userAgentField.text = ""
                                AppSettings.userAgent = ""
                            }
                        }
                    }
                }
            }

            // --- Single URL ---
            Section {
                title: qsTr("Play a stream URL")
                description: qsTr("Test a single .m3u8 or .ts link without adding a provider.")

                Card {
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: AppTheme.spacing

                        GridLayout {
                            Layout.fillWidth: true
                            columns: page.narrow ? 1 : 2
                            columnSpacing: AppTheme.spacing
                            rowSpacing: AppTheme.spacing
                            AppTextField {
                                id: urlField
                                objectName: "urlField"
                                Layout.fillWidth: true
                                placeholderText: "http://host:port/live/user/pass/12345.m3u8"
                                inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase
                                                  | Qt.ImhNoPredictiveText
                                selectByMouse: true
                                onAccepted: page.submitUrl()
                                onTextChanged: urlError.visible = false
                            }
                            AppButton {
                                text: qsTr("Play")
                                iconName: "play"
                                enabled: urlField.text.trim().length > 0
                                onClicked: page.submitUrl()
                            }
                        }
                        Label {
                            id: urlError
                            visible: false
                            text: qsTr("That doesn't look like a valid URL.")
                            color: AppTheme.error
                        }
                        Repeater {
                            model: AppSettings.recentUrls
                            delegate: ItemDelegate {
                                id: recent
                                required property string modelData
                                Layout.fillWidth: true
                                padding: 6
                                hoverEnabled: true
                                onClicked: {
                                    AppSettings.addRecentUrl(modelData)
                                    AppState.playStreamRequested(modelData, "")
                                }
                                background: Rectangle {
                                    radius: AppTheme.radiusSmall
                                    color: recent.hovered ? AppTheme.hover : "transparent"
                                }
                                contentItem: RowLayout {
                                    IconImage {
                                        source: "icons/link.svg"
                                        sourceSize: Qt.size(16, 16)
                                        color: AppTheme.textTertiary
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: recent.modelData
                                        elide: Text.ElideMiddle
                                        color: AppTheme.textSecondary
                                        font.pixelSize: AppTheme.fontSmall
                                    }
                                    IconButton {
                                        size: 30
                                        iconName: "close"
                                        iconColor: AppTheme.textTertiary
                                        tip: qsTr("Remove")
                                        onClicked: AppSettings.removeRecentUrl(recent.modelData)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // --- About ---
            Section {
                title: qsTr("About")
                Card {
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
                                text: qsTr("IPTV Player %1").arg(AppSettings.appVersion)
                                font.weight: Font.DemiBold
                                color: AppTheme.textPrimary
                            }
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: qsTr("Built with Qt. Uses the Inter typeface (SIL Open Font License). "
                                           + "Your logins and lists are stored only on this device.")
                                color: AppTheme.textTertiary
                                font.pixelSize: AppTheme.fontCaption
                            }
                        }
                    }
                }
            }
        }
    }

    // --- Dialogs ---

    Dialog {
        id: removeDialog
        property int sourceId: -1
        property string sourceName
        anchors.centerIn: parent
        width: Math.min(page.width - 48, 440)
        modal: true
        title: qsTr("Remove provider?")
        standardButtons: Dialog.Cancel | Dialog.Ok
        Label {
            width: removeDialog.availableWidth
            wrapMode: Text.Wrap
            text: qsTr("“%1” will be removed from this device, together with its favorites "
                       + "and watch progress.").arg(removeDialog.sourceName)
        }
        onAccepted: SourceManager.removeSource(sourceId)
    }

    Dialog {
        id: shiftDialog
        property int sourceId: -1
        property string sourceName
        anchors.centerIn: parent
        width: Math.min(page.width - 48, 460)
        modal: true
        title: qsTr("TV guide time shift")
        standardButtons: Dialog.Cancel | Dialog.Ok
        onAccepted: SourceManager.setEpgShift(sourceId, shiftBox.value)

        ColumnLayout {
            width: shiftDialog.availableWidth
            spacing: AppTheme.spacing
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: AppTheme.textSecondary
                text: qsTr("If programs in the guide for “%1” start too early or too late, move "
                           + "all of them by this amount. Positive values make programs later.")
                        .arg(shiftDialog.sourceName)
            }
            SpinBox {
                id: shiftBox
                Layout.alignment: Qt.AlignHCenter
                from: -720
                to: 720
                stepSize: 30
                editable: false
                textFromValue: (value) => page.shiftText(value)
            }
        }
    }

    // --- Pieces ---

    component Section: ColumnLayout {
        id: section
        property string title
        property string description
        Layout.fillWidth: true
        spacing: AppTheme.spacing

        // A plain Column so the wrapped description gets its real height
        // (layouts don't re-measure wrapped text when their width changes).
        Column {
            Layout.fillWidth: true
            spacing: 6
            Label {
                width: parent.width
                text: section.title
                font.pixelSize: AppTheme.fontH2
                font.weight: Font.Bold
                color: AppTheme.textPrimary
            }
            Label {
                width: parent.width
                visible: text.length > 0
                text: section.description
                wrapMode: Text.Wrap
                color: AppTheme.textTertiary
                font.pixelSize: AppTheme.fontSmall
                bottomPadding: 2
            }
        }
    }

    component Card: Rectangle {
        default property alias content: inner.data
        Layout.fillWidth: true
        implicitHeight: inner.implicitHeight + 36
        radius: AppTheme.radius
        color: AppTheme.surface
        border.color: AppTheme.stroke
        ColumnLayout {
            id: inner
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 18
            spacing: AppTheme.spacing
        }
    }

    component ProviderRow: Card {
        id: row
        required property int sourceId
        required property string name
        required property string type
        required property int liveCount
        required property int movieCount
        required property int seriesCount
        required property var lastSync
        required property var expiresAt
        required property string status
        required property int epgCount
        required property var epgLastSync
        required property int epgShiftMinutes
        readonly property bool syncing: (SourceManager.busy && SourceManager.activeSourceId === sourceId)
                                        || (EpgManager.busy && EpgManager.activeSourceId === sourceId)
        readonly property bool expiresSoon: expiresAt && !isNaN(expiresAt.getTime())
                                            && expiresAt.getTime() - Date.now() < 7 * 86400000

        GridLayout {
            Layout.fillWidth: true
            columns: page.narrow ? 1 : 2
            columnSpacing: AppTheme.spacing
            rowSpacing: AppTheme.spacing

            RowLayout {
                Layout.fillWidth: true
                spacing: 14
                Avatar {
                    Layout.alignment: Qt.AlignTop
                    name: row.name
                    size: 44
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3
                    RowLayout {
                        spacing: 8
                        Label {
                            text: row.name
                            font.pixelSize: AppTheme.fontH3
                            font.weight: Font.Bold
                            color: AppTheme.textPrimary
                            elide: Text.ElideRight
                            Layout.maximumWidth: 260
                        }
                        Rectangle {
                            radius: 4
                            color: AppTheme.surfaceHigh
                            implicitWidth: typeLabel.implicitWidth + 10
                            implicitHeight: typeLabel.implicitHeight + 4
                            Label {
                                id: typeLabel
                                anchors.centerIn: parent
                                text: row.type === "xtream" ? "Xtream" : "M3U"
                                color: AppTheme.textSecondary
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                            }
                        }
                        Rectangle {
                            visible: row.sourceId === AppState.sourceId
                            radius: 4
                            color: AppTheme.selection
                            implicitWidth: activeLabel.implicitWidth + 10
                            implicitHeight: activeLabel.implicitHeight + 4
                            Label {
                                id: activeLabel
                                anchors.centerIn: parent
                                text: qsTr("In use")
                                color: AppTheme.accent
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                            }
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        color: AppTheme.textSecondary
                        font.pixelSize: AppTheme.fontSmall
                        text: [page.count(row.liveCount, qsTr("1 channel"), qsTr("%1 channels")),
                               row.movieCount > 0 ? page.count(row.movieCount, qsTr("1 movie"), qsTr("%1 movies")) : "",
                               row.seriesCount > 0 ? page.count(row.seriesCount, qsTr("1 series"), qsTr("%1 series")) : ""]
                              .filter(t => t).join("  ·  ")
                    }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        color: AppTheme.textTertiary
                        font.pixelSize: AppTheme.fontCaption
                        text: row.syncing
                              ? (SourceManager.busy ? SourceManager.statusText : EpgManager.statusText)
                              : [qsTr("Updated %1").arg(page.timeAgo(row.lastSync)),
                                 row.epgCount > 0 ? qsTr("guide %1").arg(page.timeAgo(row.epgLastSync)) : "",
                                 row.epgShiftMinutes !== 0 ? qsTr("guide shifted %1").arg(page.shiftText(row.epgShiftMinutes)) : ""]
                                .filter(t => t).join("  ·  ")
                    }
                    Label {
                        visible: text.length > 0
                        color: row.expiresSoon || (row.status && row.status !== "Active")
                               ? AppTheme.error : AppTheme.textTertiary
                        font.pixelSize: AppTheme.fontCaption
                        text: {
                            const parts = []
                            if (row.status && row.status !== "Active")
                                parts.push(row.status)
                            if (row.expiresAt && !isNaN(row.expiresAt.getTime()))
                                parts.push(qsTr("Expires %1").arg(
                                    row.expiresAt.toLocaleDateString(Qt.locale(), Locale.ShortFormat)))
                            return parts.join("  ·  ")
                        }
                    }
                    ProgressBar {
                        Layout.fillWidth: true
                        visible: row.syncing
                        indeterminate: true
                    }
                }
            }

            RowLayout {
                Layout.alignment: page.narrow ? Qt.AlignLeft : Qt.AlignRight | Qt.AlignTop
                spacing: 4
                AppButton {
                    variant: "secondary"
                    implicitHeight: 36
                    visible: row.sourceId !== AppState.sourceId
                    text: qsTr("Use")
                    onClicked: AppState.setSource(row.sourceId)
                }
                IconButton {
                    iconName: "retry"
                    tip: qsTr("Update lists and TV guide")
                    enabled: !SourceManager.busy
                    onClicked: {
                        SourceManager.refresh(row.sourceId)
                        EpgManager.refresh(row.sourceId)
                    }
                }
                IconButton {
                    iconName: "filter"
                    tip: qsTr("Choose categories")
                    onClicked: AppState.manageCategoriesRequested(row.sourceId, "live")
                }
                IconButton {
                    iconName: "clock"
                    tip: qsTr("TV guide time shift")
                    onClicked: {
                        shiftDialog.sourceId = row.sourceId
                        shiftDialog.sourceName = row.name
                        shiftBox.value = row.epgShiftMinutes
                        shiftDialog.open()
                    }
                }
                IconButton {
                    iconName: "delete"
                    iconColor: AppTheme.error
                    tip: qsTr("Remove")
                    onClicked: {
                        removeDialog.sourceId = row.sourceId
                        removeDialog.sourceName = row.name
                        removeDialog.open()
                    }
                }
            }
        }
    }
}
