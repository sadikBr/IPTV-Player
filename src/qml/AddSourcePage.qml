import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts
import IPTVPlayer

// Form to add an Xtream Codes login or an M3U playlist.
Page {
    id: page

    property bool waiting: false
    property string errorText: ""
    readonly property bool xtreamTab: tabBar.currentIndex === 0
    readonly property bool freeTab: tabBar.currentIndex === 2
    // Free channels: "all", "country" or "category", and the chosen one
    property string freeMode: "all"
    property var freeChoice: null
    readonly property bool canSave: !SourceManager.busy
            && (xtreamTab ? serverField.text.trim().length > 0 && userField.text.trim().length > 0
                            && passwordField.text.length > 0
                : freeTab ? (freeMode === "all" || freeChoice !== null)
                          : m3uUrlField.text.trim().length > 0)
    // Xtream login found inside the M3U link, if any
    readonly property var m3uCredentials: SourceManager.parseCredentialsUrl(m3uUrlField.text)

    signal added(int sourceId)
    signal backRequested()

    function save() {
        if (!canSave)
            return
        errorText = ""
        waiting = true
        if (xtreamTab) {
            SourceManager.addXtreamSource(xtreamName.text, serverField.text, userField.text,
                                          passwordField.text,
                                          formatCombo.currentIndex === 1 ? "m3u8" : "ts")
        } else if (freeTab) {
            const url = freeMode === "country" ? FreeTvCatalog.countryUrl(freeChoice.code)
                      : freeMode === "category" ? FreeTvCatalog.categoryUrl(freeChoice.code)
                      : FreeTvCatalog.allChannelsUrl()
            const name = freeMode === "all" ? qsTr("Free TV")
                                            : qsTr("Free TV · %1").arg(freeChoice.name)
            SourceManager.addM3uSource(name, url.toString(), "")
        } else {
            SourceManager.addM3uSource(m3uName.text, m3uUrlField.text, epgField.text)
        }
    }

    // Smoke test: show the free channels tab, choose a country.
    function smokeFreeTab(code) {
        tabBar.currentIndex = 2
        freeMode = "country"
    }

    // Fills the Xtream form from a link like http://host/get.php?username=..&password=..
    function fillXtream(credentials) {
        serverField.text = credentials.server
        userField.text = credentials.username
        passwordField.text = credentials.password
    }

    background: Rectangle { color: AppTheme.background }

    Connections {
        target: SourceManager
        function onFinished(id, ok, error, isNew) {
            if (!page.waiting || !isNew)
                return
            page.waiting = false
            if (ok)
                page.added(id)
            else
                page.errorText = error
        }
    }

    FileDialog {
        id: fileDialog
        title: qsTr("Choose a playlist")
        nameFilters: [qsTr("Playlists (*.m3u *.m3u8 *.txt)"), qsTr("All files (*)")]
        onAccepted: m3uUrlField.text = selectedFile.toString()
    }

    header: PageHeader {
        title: qsTr("Add provider")
        subtitle: qsTr("Xtream Codes login, M3U playlist or free channels")
        showBack: true
        onBackClicked: if (!page.waiting) page.backRequested()
    }

    Flickable {
        anchors.fill: parent
        contentHeight: form.implicitHeight + 80
        clip: true
        ScrollBar.vertical: AppScrollBar {}

        // Card behind the form
        Rectangle {
            x: form.x - 24
            y: form.y - 20
            width: form.width + 48
            height: form.height + 40
            radius: AppTheme.radiusLarge
            color: AppTheme.surface
            border.color: AppTheme.stroke
            visible: page.width >= AppTheme.compactWidth
        }

        ColumnLayout {
            id: form
            width: Math.min(page.width - 2 * (page.width >= AppTheme.compactWidth ? 48 : AppTheme.margin), 560)
            x: (page.width - width) / 2
            y: 28
            spacing: AppTheme.spacing

            TabBar {
                id: tabBar
                Layout.fillWidth: true
                enabled: !page.waiting
                Material.background: "transparent"
                // Short names on phones, where the long ones get cut off
                readonly property bool shortNames: width < 460
                TabButton { text: tabBar.shortNames ? qsTr("Xtream") : qsTr("Xtream Codes login") }
                TabButton { text: tabBar.shortNames ? qsTr("M3U") : qsTr("M3U playlist") }
                TabButton { text: tabBar.shortNames ? qsTr("Free") : qsTr("Free channels") }
                onCurrentIndexChanged: {
                    page.errorText = ""
                    if (currentIndex === 2)
                        FreeTvCatalog.load()
                }
            }

            StackLayout {
                Layout.fillWidth: true
                currentIndex: tabBar.currentIndex
                enabled: !page.waiting

                // --- Xtream ---
                ColumnLayout {
                    spacing: AppTheme.spacing

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        color: AppTheme.textSecondary
                        text: qsTr("Enter the details from your provider. Tip: paste your full M3U link "
                                   + "(…/get.php?username=…&password=…) into the server field and the "
                                   + "rest fills in automatically.")
                    }
                    AppTextField {
                        id: serverField
                        objectName: "serverField"
                        Layout.fillWidth: true
                        placeholderText: qsTr("Server, e.g. http://example.com:8080")
                        inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase
                        selectByMouse: true
                        onTextChanged: {
                            const credentials = SourceManager.parseCredentialsUrl(text)
                            if (credentials.server)
                                page.fillXtream(credentials)
                        }
                    }
                    AppTextField {
                        id: userField
                        Layout.fillWidth: true
                        placeholderText: qsTr("Username")
                        inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                        selectByMouse: true
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        AppTextField {
                            id: passwordField
                            Layout.fillWidth: true
                            placeholderText: qsTr("Password")
                            echoMode: showPassword.checked ? TextInput.Normal : TextInput.Password
                            inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                            selectByMouse: true
                            onAccepted: page.save()
                        }
                        CheckBox {
                            id: showPassword
                            text: qsTr("Show")
                        }
                    }
                    AppTextField {
                        id: xtreamName
                        Layout.fillWidth: true
                        placeholderText: qsTr("Name (optional)")
                        selectByMouse: true
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            text: qsTr("Stream format")
                            color: AppTheme.textSecondary
                        }
                        ComboBox {
                            id: formatCombo
                            Layout.fillWidth: true
                            model: [qsTr("MPEG-TS (.ts) — recommended"), qsTr("HLS (.m3u8)")]
                        }
                    }
                }

                // --- M3U ---
                ColumnLayout {
                    spacing: AppTheme.spacing

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        color: AppTheme.textSecondary
                        text: qsTr("Enter the playlist URL from your provider, or choose a playlist file.")
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        AppTextField {
                            id: m3uUrlField
                            objectName: "m3uUrlField"
                            Layout.fillWidth: true
                            placeholderText: qsTr("http://example.com/playlist.m3u")
                            inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase
                            selectByMouse: true
                            onAccepted: page.save()
                        }
                        IconButton {
                            iconName: "folder"
                            tip: qsTr("Choose file")
                            iconColor: AppTheme.textPrimary
                            onClicked: fileDialog.open()
                        }
                    }

                    // The link contains an Xtream login: suggest using it.
                    Pane {
                        Layout.fillWidth: true
                        visible: page.m3uCredentials.server !== undefined
                        padding: AppTheme.spacing
                        background: Rectangle { color: AppTheme.selection; radius: AppTheme.radius }

                        RowLayout {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                color: AppTheme.textPrimary
                                text: qsTr("This link contains an Xtream login. Using it loads faster "
                                           + "and will support movies, series and the TV guide.")
                            }
                            AppButton {
                                variant: "secondary"
                                text: qsTr("Use Xtream")
                                onClicked: {
                                    page.fillXtream(page.m3uCredentials)
                                    tabBar.currentIndex = 0
                                }
                            }
                        }
                    }

                    AppTextField {
                        id: epgField
                        Layout.fillWidth: true
                        placeholderText: qsTr("TV guide (XMLTV) URL (optional)")
                        inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase
                        selectByMouse: true
                    }
                    AppTextField {
                        id: m3uName
                        Layout.fillWidth: true
                        placeholderText: qsTr("Name (optional)")
                        selectByMouse: true
                    }
                }
            
                // --- Free channels (iptv-org) ---
                ColumnLayout {
                    id: freePanel
                    spacing: AppTheme.spacing

                    readonly property var choices: page.freeMode === "country" ? FreeTvCatalog.countries
                                                 : page.freeMode === "category" ? FreeTvCatalog.categories : []
                    readonly property var filtered: {
                        const key = freeSearch.text.trim().toLowerCase()
                        const list = choices.filter(c => key.length === 0 || c.name.toLowerCase().indexOf(key) >= 0
                                                    || c.code.toLowerCase() === key)
                        // This device's country first
                        if (page.freeMode === "country") {
                            const local = list.findIndex(c => c.code === FreeTvCatalog.localCountry)
                            if (local > 0)
                                list.unshift(list.splice(local, 1)[0])
                        }
                        return list
                    }

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        color: AppTheme.textSecondary
                        text: qsTr("Thousands of channels that are free to watch, collected by the "
                                   + "open iptv-org project. No account needed. Some channels may be "
                                   + "offline at times.")
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 8
                        Chip {
                            text: qsTr("All channels")
                            selected: page.freeMode === "all"
                            onClicked: { page.freeMode = "all"; page.freeChoice = null }
                        }
                        Chip {
                            text: qsTr("A country")
                            selected: page.freeMode === "country"
                            onClicked: { page.freeMode = "country"; page.freeChoice = null }
                        }
                        Chip {
                            text: qsTr("A category")
                            selected: page.freeMode === "category"
                            onClicked: { page.freeMode = "category"; page.freeChoice = null }
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: page.freeMode === "all"
                        wrapMode: Text.Wrap
                        color: AppTheme.textTertiary
                        font.pixelSize: AppTheme.fontSmall
                        text: qsTr("Every channel (more than 10,000), grouped by category. Use Choose "
                                   + "categories afterwards to hide what you don't want.")
                    }
                    SearchField {
                        id: freeSearch
                        Layout.fillWidth: true
                        visible: page.freeMode !== "all"
                        placeholderText: page.freeMode === "country" ? qsTr("Search countries")
                                                                     : qsTr("Search categories")
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 260
                        visible: page.freeMode !== "all"
                        radius: AppTheme.radius
                        color: AppTheme.surface
                        border.width: 1
                        border.color: AppTheme.stroke

                        BusyIndicator {
                            anchors.centerIn: parent
                            running: FreeTvCatalog.loading
                            visible: running
                        }
                        Label {
                            anchors.centerIn: parent
                            width: parent.width - 32
                            visible: !FreeTvCatalog.loading && FreeTvCatalog.errorString.length > 0
                            wrapMode: Text.Wrap
                            horizontalAlignment: Text.AlignHCenter
                            color: AppTheme.error
                            text: FreeTvCatalog.errorString
                        }
                        ListView {
                            id: freeList
                            anchors.fill: parent
                            anchors.margins: 4
                            clip: true
                            model: freePanel.filtered
                            keyNavigationEnabled: false
                            ScrollBar.vertical: AppScrollBar {}
                            delegate: ItemDelegate {
                                id: choiceRow
                                required property var modelData
                                readonly property bool chosen: page.freeChoice !== null
                                                               && page.freeChoice.code === modelData.code
                                width: freeList.width
                                height: 40
                                focusPolicy: Qt.TabFocus
                                hoverEnabled: true
                                onClicked: page.freeChoice = modelData
                                background: Rectangle {
                                    radius: AppTheme.radiusSmall
                                    color: choiceRow.chosen ? AppTheme.accentSoft
                                         : choiceRow.hovered ? AppTheme.hover : "transparent"
                                    FocusRing { target: choiceRow; gap: 0 }
                                }
                                // Country code instead of a flag emoji (Windows can't show those)
                                contentItem: Row {
                                    leftPadding: 10
                                    spacing: 10
                                    Rectangle {
                                        visible: page.freeMode === "country"
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 30
                                        height: 20
                                        radius: 4
                                        color: AppTheme.surfaceHigh
                                        Label {
                                            anchors.centerIn: parent
                                            text: choiceRow.modelData.code
                                            font.pixelSize: 11
                                            font.weight: Font.Bold
                                            color: AppTheme.textSecondary
                                        }
                                    }
                                    Label {
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: choiceRow.width - 60
                                        text: choiceRow.modelData.name
                                        color: choiceRow.chosen ? AppTheme.textPrimary : AppTheme.textSecondary
                                        font.weight: choiceRow.chosen ? Font.DemiBold : Font.Normal
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                    }
                }
}

            // Progress
            Pane {
                Layout.fillWidth: true
                visible: page.waiting
                padding: AppTheme.spacing
                background: Rectangle { color: AppTheme.surface; radius: AppTheme.radius }

                ColumnLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    Label {
                        text: SourceManager.statusText
                        color: AppTheme.textPrimary
                    }
                    ProgressBar {
                        Layout.fillWidth: true
                        indeterminate: SourceManager.progress < 0
                        value: Math.max(0, SourceManager.progress)
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                visible: page.errorText.length > 0
                wrapMode: Text.Wrap
                color: AppTheme.error
                text: page.errorText
            }

            Label {
                visible: !page.freeTab
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: AppTheme.textSecondary
                font.pixelSize: 12
                text: qsTr("Your login is stored only on this device, inside the app's private data folder.")
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: AppTheme.spacing
                Item { Layout.fillWidth: true }
                AppButton {
                    text: qsTr("Cancel")
                    variant: "ghost"
                    onClicked: {
                        if (page.waiting)
                            SourceManager.cancel()
                        else
                            page.backRequested()
                    }
                }
                AppButton {
                    text: qsTr("Add provider")
                    iconName: "add"
                    enabled: page.canSave
                    onClicked: page.save()
                }
            }
        }
    }
}
