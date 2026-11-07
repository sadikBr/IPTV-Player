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
    readonly property bool canSave: !SourceManager.busy
            && (xtreamTab ? serverField.text.trim().length > 0 && userField.text.trim().length > 0
                            && passwordField.text.length > 0
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
        } else {
            SourceManager.addM3uSource(m3uName.text, m3uUrlField.text, epgField.text)
        }
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
        subtitle: qsTr("Xtream Codes login or M3U playlist")
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
                TabButton { text: qsTr("Xtream Codes login") }
                TabButton { text: qsTr("M3U playlist") }
                onCurrentIndexChanged: page.errorText = ""
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
