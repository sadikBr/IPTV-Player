import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import IPTVPlayer

// Tab bar at the bottom on phones: a floating pill with a glowing highlight
// that slides (with a little bounce) to the tab you pick.
Item {
    id: root

    implicitHeight: 76

    // Fades the content into the bar's background
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.45; color: Qt.rgba(AppTheme.background.r, AppTheme.background.g,
                                                          AppTheme.background.b, 0.92) }
        }
    }

    Shadow { target: pill; opacity: 0.9 }

    Rectangle {
        id: pill
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.topMargin: 6
        anchors.bottomMargin: 10
        radius: height / 2
        color: AppTheme.surfaceHigh
        border.width: 1
        border.color: AppTheme.strokeStrong

        // Thin progress line while providers or guides update
        ProgressBar {
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width - 2 * parent.radius
            height: 2
            visible: SourceManager.busy || EpgManager.busy
            indeterminate: true
        }

        // Highlight behind the active tab
        Rectangle {
            id: highlight
            readonly property Item tab: {
                for (let i = 0; i < tabs.children.length; ++i) {
                    const t = tabs.children[i]
                    if (t.active && t.visible)
                        return t
                }
                return null
            }
            visible: tab !== null
            x: tab ? tabs.x + tab.x + 4 : 0
            y: 5
            width: tab ? tab.width - 8 : 0
            height: parent.height - 10
            radius: height / 2
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: AppTheme.accent }
                GradientStop { position: 1.0; color: AppTheme.accentPink }
            }
            Behavior on x { NumberAnimation { duration: 420; easing.type: Easing.OutBack; easing.overshoot: 1.4 } }
            Behavior on width { NumberAnimation { duration: 420; easing.type: Easing.OutBack } }
        }

        RowLayout {
            id: tabs
            anchors.fill: parent
            anchors.leftMargin: 4
            anchors.rightMargin: 4
            spacing: 0

            Tab { section: "home"; iconName: "home"; text: qsTr("Home") }
            Tab { section: "live"; iconName: "live"; text: qsTr("Live"); visible: AppState.hasSource }
            Tab {
                section: "movie"; iconName: "film"; text: qsTr("Movies")
                visible: (AppState.sourceInfo.movieCount || 0) > 0
            }
            Tab {
                section: "series"; iconName: "series"; text: qsTr("Series")
                visible: (AppState.sourceInfo.seriesCount || 0) > 0
            }
            Tab { section: "radio"; iconName: "radio"; text: qsTr("Radio") }
            // With a provider, five tabs are enough on a phone: Settings is the gear on
            // Home and in the provider menu. Without one it stays here (restore a backup).
            Tab {
                section: "settings"; text: qsTr("Settings")
                iconName: ParentalControl.locked ? "lock" : "settings"
                visible: !AppState.hasSource
            }
        }
    }

    component Tab: AbstractButton {
        id: tab
        property string section
        property string iconName
        readonly property bool active: AppState.section === section
                                       || (section === "live" && AppState.section === "guide")

        Layout.fillWidth: true
        Layout.fillHeight: true
        focusPolicy: Qt.TabFocus
        onClicked: AppState.sectionRequested(section)

        background: Item {
            FocusRing { target: tab; radius: height / 2 }
        }
        contentItem: Item {
            Column {
                anchors.centerIn: parent
                spacing: 2
                // A little hop when pressed or chosen
                scale: tab.pressed ? 0.85 : 1
                Behavior on scale { NumberAnimation { duration: 260; easing.type: Easing.OutBack; easing.overshoot: 3 } }
                IconImage {
                    anchors.horizontalCenter: parent.horizontalCenter
                    source: "icons/" + tab.iconName + ".svg"
                    sourceSize: Qt.size(21, 21)
                    color: tab.active ? "white" : AppTheme.textSecondary
                    Behavior on color { ColorAnimation { duration: AppTheme.normal } }
                }
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: tab.text
                    font.pixelSize: 10
                    font.weight: tab.active ? Font.Bold : Font.Medium
                    color: tab.active ? "white" : AppTheme.textSecondary
                }
            }
        }
    }
}
