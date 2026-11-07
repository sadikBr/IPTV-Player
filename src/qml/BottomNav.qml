import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import IPTVPlayer

// Tab bar at the bottom on phones.
Rectangle {
    id: root

    implicitHeight: 64
    color: "#F20B0B0D"

    Rectangle {
        width: parent.width
        height: 1
        color: AppTheme.stroke
    }

    // Thin progress line while providers or guides update
    ProgressBar {
        anchors.top: parent.top
        width: parent.width
        height: 2
        visible: SourceManager.busy || EpgManager.busy
        indeterminate: true
    }

    RowLayout {
        anchors.fill: parent
        anchors.topMargin: 1
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
        Tab { section: "settings"; iconName: "settings"; text: qsTr("Settings") }
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

        background: Item {}
        contentItem: Item {
            Column {
                anchors.centerIn: parent
                spacing: 3
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 54
                    height: 28
                    radius: 14
                    color: tab.active ? AppTheme.surfaceHigh : "transparent"
                    FocusRing { target: tab }
                    IconImage {
                        anchors.centerIn: parent
                        source: "icons/" + tab.iconName + ".svg"
                        sourceSize: Qt.size(20, 20)
                        color: tab.active ? AppTheme.textPrimary : AppTheme.textSecondary
                    }
                }
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: tab.text
                    font.pixelSize: 11
                    font.weight: tab.active ? Font.DemiBold : Font.Medium
                    color: tab.active ? AppTheme.textPrimary : AppTheme.textSecondary
                }
            }
        }
    }
}
