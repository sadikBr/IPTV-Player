import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Dialogs
import QtQuick.Layouts
import IPTVPlayer

// Providers, network/playback options, single-URL player and about.
Page {
    id: page

    readonly property bool narrow: width < AppTheme.compactWidth
    readonly property real sideMargin: narrow ? AppTheme.margin : AppTheme.pageMargin

    signal updateRequested()

    // Parental PIN dialogs: "new" -> "confirm"; "current-change" -> "new" -> "confirm";
    // "current-remove".
    property string pinStage
    property string pinCurrent
    property string pinNew
    function pinStep(stage) {
        pinStage = stage
        if (stage === "new") {
            pinDialog.length = 4
            pinDialog.ask(qsTr("Choose a PIN"), qsTr("4 digits. You'll need it to see locked categories and Settings."))
        } else if (stage === "confirm") {
            pinDialog.ask(qsTr("Repeat the PIN"), "")
        } else {
            pinCurrent = ""
            pinNew = ""
            pinDialog.length = 4
            pinDialog.ask(qsTr("Enter your current PIN"), "")
        }
    }
    function pinEntered(pin) {
        switch (pinStage) {
        case "current-change":
        case "current-remove":
            if (!ParentalControl.checkPin(pin)) {
                pinDialog.error = qsTr("Wrong PIN, try again.")
                return
            }
            pinCurrent = pin
            if (pinStage === "current-remove") {
                ParentalControl.removePin(pin)
                pinDialog.close()
            } else {
                pinStep("new")
            }
            return
        case "new":
            pinNew = pin
            pinStep("confirm")
            return
        case "confirm":
            if (pin !== pinNew) {
                pinNew = ""
                pinStep("new")
                pinDialog.error = qsTr("The PINs didn't match. Choose a PIN again.")
                return
            }
            ParentalControl.setPin(pinCurrent, pin)
            pinDialog.close()
            pinCurrent = ""
            pinNew = ""
            // Offer to lock what looks like adult content.
            const adult = AppState.sourceId > 0 ? ParentalControl.adultCategoryCount(AppState.sourceId) : 0
            if (adult > 0) {
                adultDialog.count = adult
                adultDialog.open()
            }
            return
        }
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

            // --- Playback ---
            Section {
                title: qsTr("Playback")
                Card {
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 16
                        SwitchRow {
                            title: qsTr("Reconnect automatically")
                            description: qsTr("When a channel or movie drops, freezes or keeps buffering, "
                                              + "connect again by itself.")
                            checked: AppSettings.autoReconnect
                            onToggled: (on) => AppSettings.autoReconnect = on
                        }
                        SwitchRow {
                            visible: AppSettings.lowLatencySupported
                            title: qsTr("Start channels faster")
                            description: qsTr("Less buffering on live TV: channels start and switch "
                                              + "sooner, but may stutter on a slow connection.")
                            checked: AppSettings.lowLatency
                            onToggled: (on) => AppSettings.lowLatency = on
                        }
                        SwitchRow {
                            title: qsTr("Hardware decoding")
                            description: qsTr("Uses the graphics card to decode video. Turn it off if you "
                                              + "see a green or garbled picture. Applies after restarting the app.")
                            checked: AppSettings.hardwareDecoding
                            onToggled: (on) => AppSettings.hardwareDecoding = on
                        }
                    }
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
                                focusPolicy: Qt.TabFocus
                                onClicked: {
                                    AppSettings.addRecentUrl(modelData)
                                    AppState.playStreamRequested(modelData, "")
                                }
                                background: Rectangle {
                                    radius: AppTheme.radiusSmall
                                    color: recent.hovered ? AppTheme.hover : "transparent"
                                    FocusRing { target: recent; gap: 0 }
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

            // --- Parental control ---
            Section {
                title: qsTr("Parental control")
                description: qsTr("A PIN hides the categories you lock, and these settings, until "
                                  + "it is entered. The app locks again every time it starts.")
                Card {
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 14
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: AppTheme.spacing
                            IconImage {
                                source: ParentalControl.hasPin ? "icons/lock.svg" : "icons/lock-open.svg"
                                sourceSize: Qt.size(22, 22)
                                color: ParentalControl.hasPin ? AppTheme.accent : AppTheme.textTertiary
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Label {
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    font.weight: Font.DemiBold
                                    color: AppTheme.textPrimary
                                    text: ParentalControl.hasPin ? qsTr("PIN is set") : qsTr("No PIN")
                                }
                                Label {
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    color: AppTheme.textTertiary
                                    font.pixelSize: AppTheme.fontCaption
                                    text: {
                                        ParentalControl.hasPin // re-evaluate on changes
                                        const n = AppState.sourceId > 0
                                                ? ParentalControl.lockedCategoryCount(AppState.sourceId) : 0
                                        return ParentalControl.hasPin
                                                ? (n === 1 ? qsTr("1 locked category for this provider.")
                                                           : qsTr("%1 locked categories for this provider.").arg(n))
                                                  + " " + qsTr("Lock or unlock them in Choose categories (lock button).")
                                                : qsTr("Set a PIN to lock categories, for example for children.")
                                    }
                                }
                            }
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 10
                            AppButton {
                                visible: !ParentalControl.hasPin
                                variant: "accent"
                                iconName: "lock"
                                text: qsTr("Set a PIN")
                                onClicked: page.pinStep("new")
                            }
                            AppButton {
                                visible: ParentalControl.hasPin
                                variant: "secondary"
                                iconName: "lock"
                                text: qsTr("Lock now")
                                onClicked: {
                                    ParentalControl.lock()
                                    AppState.sectionRequested("home")
                                }
                            }
                            AppButton {
                                visible: ParentalControl.hasPin
                                variant: "secondary"
                                text: qsTr("Choose categories")
                                enabled: AppState.sourceId > 0
                                onClicked: AppState.manageCategoriesRequested(AppState.sourceId, "live")
                            }
                            AppButton {
                                visible: ParentalControl.hasPin
                                variant: "ghost"
                                text: qsTr("Change PIN")
                                onClicked: page.pinStep("current-change")
                            }
                            AppButton {
                                visible: ParentalControl.hasPin
                                variant: "ghost"
                                text: qsTr("Remove PIN")
                                onClicked: page.pinStep("current-remove")
                            }
                        }
                    }
                }
            }

            // --- Backup ---
            Section {
                title: qsTr("Backup and move to another device")
                description: qsTr("Saves your providers (with their logins), favorites, hidden and locked "
                                  + "categories, what you were watching, radio stations and settings in one "
                                  + "file. Import it on your phone, TV box or another computer. The file "
                                  + "contains your passwords: keep it private.")
                Card {
                    Flow {
                        Layout.fillWidth: true
                        spacing: 10
                        AppButton {
                            objectName: "exportButton"
                            variant: "secondary"
                            text: qsTr("Export…")
                            onClicked: exportDialog.open()
                        }
                        AppButton {
                            variant: "secondary"
                            text: qsTr("Import…")
                            onClicked: importDialog.open()
                        }
                    }
                }
            }

            // --- Updates ---
            Section {
                title: qsTr("Updates")
                Card {
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 14
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: AppTheme.spacing
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Label {
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    font.weight: Font.DemiBold
                                    color: UpdateManager.state === "available" ? AppTheme.accent2
                                                                               : AppTheme.textPrimary
                                    text: {
                                        switch (UpdateManager.state) {
                                        case "checking": return qsTr("Checking for updates…")
                                        case "available":
                                        case "downloading":
                                        case "installing":
                                            return qsTr("Version %1 is available").arg(UpdateManager.latestVersion)
                                        case "upToDate": return qsTr("You have the latest version")
                                        case "error": return qsTr("Couldn't check for updates")
                                        default: return qsTr("Version %1").arg(UpdateManager.currentVersion)
                                        }
                                    }
                                }
                                Label {
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    color: UpdateManager.state === "error" ? AppTheme.error : AppTheme.textTertiary
                                    font.pixelSize: AppTheme.fontCaption
                                    text: UpdateManager.state === "error"
                                          ? UpdateManager.errorString
                                          : UpdateManager.lastChecked.getTime && !isNaN(UpdateManager.lastChecked.getTime())
                                            ? qsTr("Last checked %1").arg(page.timeAgo(UpdateManager.lastChecked))
                                            : qsTr("New versions are published on GitHub.")
                                }
                            }
                            AppButton {
                                objectName: "checkUpdatesButton"
                                variant: UpdateManager.state === "available" ? "accent" : "secondary"
                                enabled: UpdateManager.state !== "checking"
                                text: UpdateManager.state === "available" || UpdateManager.state === "downloading"
                                      ? qsTr("Update…") : qsTr("Check now")
                                onClicked: {
                                    if (UpdateManager.state === "available" || UpdateManager.state === "downloading")
                                        page.updateRequested()
                                    else
                                        UpdateManager.check(true)
                                }
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: AppTheme.spacing
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: qsTr("Check for updates automatically")
                                color: AppTheme.textSecondary
                            }
                            AppSwitch {
                                checked: UpdateManager.autoCheck
                                onToggled: UpdateManager.autoCheck = checked
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

    AppDialog {
        id: removeDialog
        property int sourceId: -1
        property string sourceName
        width: Math.min(page.width - 48, 440)
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

    AppDialog {
        id: shiftDialog
        property int sourceId: -1
        property string sourceName
        property bool isXtream: false
        property string format: "ts"
        width: Math.min(page.width - 48, 480)
        title: qsTr("Options for “%1”").arg(sourceName)
        standardButtons: Dialog.Cancel | Dialog.Ok
        onAccepted: {
            SourceManager.setEpgShift(sourceId, shiftBox.value)
            if (isXtream)
                SourceManager.setOutputFormat(sourceId, format)
        }

        ColumnLayout {
            width: shiftDialog.availableWidth
            spacing: AppTheme.spacing

            // Stream format (Xtream live channels)
            Label {
                visible: shiftDialog.isXtream
                text: qsTr("Live stream format")
                font.weight: Font.DemiBold
                color: AppTheme.textPrimary
            }
            Label {
                visible: shiftDialog.isXtream
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: AppTheme.textSecondary
                text: qsTr("If channels don't start or keep stopping, try the other format.")
            }
            Row {
                visible: shiftDialog.isXtream
                spacing: 8
                Chip {
                    text: qsTr("MPEG-TS")
                    selected: shiftDialog.format === "ts"
                    onClicked: shiftDialog.format = "ts"
                }
                Chip {
                    text: qsTr("HLS (m3u8)")
                    selected: shiftDialog.format === "m3u8"
                    onClicked: shiftDialog.format = "m3u8"
                }
            }

            Label {
                Layout.topMargin: shiftDialog.isXtream ? 10 : 0
                text: qsTr("TV guide time shift")
                font.weight: Font.DemiBold
                color: AppTheme.textPrimary
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: AppTheme.textSecondary
                text: qsTr("If programs in the guide start too early or too late, move "
                           + "all of them by this amount. Positive values make programs later.")
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

    FileDialog {
        id: exportDialog
        title: qsTr("Save the backup")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        nameFilters: [qsTr("IPTV Player backup (*.json)")]
        onAccepted: {
            const error = BackupManager.exportTo(selectedFile)
            backupResult.title = error.length > 0 ? qsTr("Backup not saved") : qsTr("Backup saved")
            backupResult.text = error.length > 0 ? error
                    : qsTr("Copy the file to your other device and choose Import there.")
            backupResult.open()
        }
    }
    FileDialog {
        id: importDialog
        title: qsTr("Choose a backup")
        nameFilters: [qsTr("IPTV Player backup (*.json)"), qsTr("All files (*)")]
        onAccepted: {
            const r = BackupManager.importFrom(selectedFile)
            backupResult.title = r.ok ? qsTr("Backup imported") : qsTr("Couldn't import")
            if (!r.ok) {
                backupResult.text = r.error
            } else {
                const parts = []
                if (r.providersAdded > 0)
                    parts.push(r.providersAdded === 1 ? qsTr("1 provider added") : qsTr("%1 providers added").arg(r.providersAdded))
                if (r.providersMerged > 0)
                    parts.push(r.providersMerged === 1 ? qsTr("1 provider updated") : qsTr("%1 providers updated").arg(r.providersMerged))
                if (r.radioStations > 0)
                    parts.push(qsTr("%1 radio stations").arg(r.radioStations))
                backupResult.text = (parts.length > 0 ? parts.join(", ") + ". " : "")
                        + qsTr("Channel lists are downloaded again from your providers.")
                if (r.firstNewSource > 0) {
                    AppState.setSource(r.firstNewSource)
                    SourceManager.refresh(r.firstNewSource)
                }
            }
            backupResult.open()
        }
    }
    AppDialog {
        id: backupResult
        property string text
        width: Math.min(page.width - 48, 440)
        standardButtons: Dialog.Ok
        Label {
            width: backupResult.availableWidth
            wrapMode: Text.Wrap
            color: AppTheme.textSecondary
            text: backupResult.text
        }
    }

    PinDialog {
        id: pinDialog
        onEntered: (pin) => page.pinEntered(pin)
    }

    AppDialog {
        id: adultDialog
        property int count: 0
        width: Math.min(page.width - 48, 440)
        title: qsTr("Lock adult categories?")
        standardButtons: Dialog.No | Dialog.Yes
        Label {
            width: adultDialog.availableWidth
            wrapMode: Text.Wrap
            color: AppTheme.textSecondary
            text: (adultDialog.count === 1
                   ? qsTr("1 category of this provider looks like adult content.")
                   : qsTr("%1 categories of this provider look like adult content.").arg(adultDialog.count))
                  + " " + qsTr("Lock them now? You can change this in Choose categories.")
        }
        onAccepted: ParentalControl.lockAdultCategories(AppState.sourceId)
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

    // A setting with a title, an explanation and a switch. Plain anchors:
    // wrapped text inside nested layouts gets the row heights wrong.
    component SwitchRow: Item {
        id: switchRow
        property string title
        property string description
        property bool checked
        signal toggled(bool on)
        Layout.fillWidth: true
        implicitHeight: Math.max(texts.implicitHeight, toggle.implicitHeight)
        Column {
            id: texts
            anchors.left: parent.left
            anchors.right: toggle.left
            anchors.rightMargin: AppTheme.spacing
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            Label {
                width: parent.width
                wrapMode: Text.Wrap
                text: switchRow.title
                font.weight: Font.DemiBold
                color: AppTheme.textPrimary
            }
            Label {
                width: parent.width
                wrapMode: Text.Wrap
                text: switchRow.description
                color: AppTheme.textTertiary
                font.pixelSize: AppTheme.fontCaption
            }
        }
        AppSwitch {
            id: toggle
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            checked: switchRow.checked
            onToggled: switchRow.toggled(checked)
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
        required property string outputFormat
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
                                 row.epgShiftMinutes !== 0 ? qsTr("guide shifted %1").arg(page.shiftText(row.epgShiftMinutes)) : "",
                                 row.type === "xtream" && row.outputFormat === "m3u8" ? qsTr("HLS streams") : ""]
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
                    iconName: "settings"
                    tip: qsTr("Provider options: stream format, TV guide time shift")
                    onClicked: {
                        shiftDialog.sourceId = row.sourceId
                        shiftDialog.sourceName = row.name
                        shiftDialog.isXtream = row.type === "xtream"
                        shiftDialog.format = row.outputFormat || "ts"
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
