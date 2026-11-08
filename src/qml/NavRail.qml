import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import IPTVPlayer

// Side navigation for wide screens. compact: icons only (phones in
// landscape, where a bottom bar would take too much of the height).
Rectangle {
    id: root

    property bool compact: false

    color: AppTheme.background

    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: AppTheme.stroke
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.compact ? 10 : 16
        anchors.rightMargin: root.compact ? 11 : 17
        spacing: root.compact ? 4 : 6

        // Brand
        RowLayout {
            visible: !root.compact
            Layout.bottomMargin: 18
            Layout.leftMargin: 6
            spacing: 10
            Rectangle {
                Layout.preferredWidth: 34
                Layout.preferredHeight: 34
                radius: 10
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: AppTheme.accentStrong }
                    GradientStop { position: 1.0; color: AppTheme.accent2 }
                }
                IconImage {
                    anchors.centerIn: parent
                    source: "icons/play.svg"
                    sourceSize: Qt.size(18, 18)
                    color: "white"
                }
            }
            Label {
                text: "IPTV Player"
                font.pixelSize: 18
                font.weight: Font.Bold
                color: AppTheme.textPrimary
            }
        }

        ProviderSwitcher {
            Layout.fillWidth: !root.compact
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: root.compact ? 6 : 14
            compact: root.compact
            visible: AppState.hasSource
        }

        NavItem { section: "home"; iconName: "home"; text: qsTr("Home") }
        NavItem {
            section: "live"; iconName: "live"; text: qsTr("Live TV")
            visible: AppState.hasSource
        }
        NavItem {
            section: "movie"; iconName: "film"; text: qsTr("Movies")
            visible: (AppState.sourceInfo.movieCount || 0) > 0
        }
        NavItem {
            section: "series"; iconName: "series"; text: qsTr("Series")
            visible: (AppState.sourceInfo.seriesCount || 0) > 0
        }
        NavItem {
            section: "guide"; iconName: "guide"; text: qsTr("TV guide")
            visible: AppState.hasSource
        }

        Item { Layout.fillHeight: true }

        // Background work
        ColumnLayout {
            Layout.fillWidth: true
            Layout.bottomMargin: 8
            spacing: 6
            visible: SourceManager.busy || EpgManager.busy
            Label {
                Layout.fillWidth: true
                visible: !root.compact
                text: SourceManager.busy ? SourceManager.statusText : EpgManager.statusText
                color: AppTheme.textTertiary
                font.pixelSize: AppTheme.fontCaption
                elide: Text.ElideRight
            }
            ProgressBar {
                Layout.fillWidth: true
                indeterminate: (SourceManager.busy ? SourceManager.progress : EpgManager.progress) < 0
                value: Math.max(0, SourceManager.busy ? SourceManager.progress : EpgManager.progress)
            }
        }

        NavItem { section: "settings"; iconName: "settings"; text: qsTr("Settings") }
    }

    component NavItem: AbstractButton {
        id: item
        property string section
        property string iconName
        readonly property bool active: AppState.section === section

        Layout.fillWidth: true
        implicitHeight: root.compact ? 40 : 44
        hoverEnabled: true
        focusPolicy: Qt.TabFocus
        onClicked: AppState.sectionRequested(section)
        Accessible.name: text
        ToolTip.visible: root.compact && hovered
        ToolTip.text: text
        ToolTip.delay: 500

        background: Rectangle {
            radius: AppTheme.radius
            color: item.active ? AppTheme.surfaceHigh : item.hovered ? AppTheme.hover : "transparent"
            Behavior on color { ColorAnimation { duration: AppTheme.fast } }
            Rectangle {
                visible: item.active
                x: 0
                anchors.verticalCenter: parent.verticalCenter
                width: 3
                height: 20
                radius: 1.5
                color: AppTheme.accent
            }
            FocusRing { target: item; gap: 1 }
        }
        contentItem: RowLayout {
            spacing: 14
            IconImage {
                Layout.leftMargin: root.compact ? 0 : 14
                Layout.alignment: root.compact ? Qt.AlignCenter : Qt.AlignLeft | Qt.AlignVCenter
                source: "icons/" + item.iconName + ".svg"
                sourceSize: Qt.size(20, 20)
                color: item.active ? AppTheme.textPrimary : AppTheme.textSecondary
            }
            Label {
                Layout.fillWidth: true
                visible: !root.compact
                text: item.text
                font.pixelSize: AppTheme.fontBody
                font.weight: item.active ? Font.DemiBold : Font.Medium
                color: item.active ? AppTheme.textPrimary : AppTheme.textSecondary
            }
        }
    }
}
