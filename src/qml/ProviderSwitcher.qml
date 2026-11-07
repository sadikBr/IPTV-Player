import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import IPTVPlayer

// Shows the current provider; click to switch, add or manage providers.
AbstractButton {
    id: control

    // Only the avatar (phone headers)
    property bool compact: false

    implicitHeight: compact ? 40 : 52
    implicitWidth: compact ? 40 : 200
    hoverEnabled: true
    focusPolicy: Qt.NoFocus
    onClicked: menu.popup(control, 0, control.height + 4)

    background: Rectangle {
        radius: control.compact ? height / 2 : AppTheme.radius
        color: control.compact ? "transparent"
             : control.hovered ? AppTheme.surfaceHover : AppTheme.surfaceHigh
        border.width: control.compact ? 0 : 1
        border.color: AppTheme.stroke
    }

    contentItem: RowLayout {
        spacing: 10

        Avatar {
            Layout.leftMargin: control.compact ? 2 : 8
            size: control.compact ? 36 : 34
            name: AppState.sourceInfo.name || "?"
        }
        ColumnLayout {
            visible: !control.compact
            spacing: 0
            Layout.fillWidth: true
            Label {
                text: qsTr("Provider")
                font.pixelSize: 11
                color: AppTheme.textTertiary
            }
            Label {
                text: AppState.sourceInfo.name || qsTr("None")
                font.pixelSize: AppTheme.fontSmall
                font.weight: Font.DemiBold
                color: AppTheme.textPrimary
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }
        IconImage {
            visible: !control.compact
            Layout.rightMargin: 8
            source: "icons/chevron-down.svg"
            sourceSize: Qt.size(18, 18)
            color: AppTheme.textSecondary
        }
    }

    SourceListModel {
        id: sources
    }

    Menu {
        id: menu
        width: 260

        Repeater {
            model: sources
            delegate: MenuItem {
                required property int sourceId
                required property string name
                text: name
                checkable: true
                checked: sourceId === AppState.sourceId
                onTriggered: AppState.setSource(sourceId)
            }
        }
        MenuSeparator { visible: sources.count > 0 }
        MenuItem {
            text: qsTr("Add provider…")
            onTriggered: AppState.addProviderRequested()
        }
        MenuItem {
            text: qsTr("Manage providers")
            onTriggered: AppState.sectionRequested("settings")
        }
    }
}
