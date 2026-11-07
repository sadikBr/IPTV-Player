import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import IPTVPlayer

// Live channel tile: logo on top, name and what's on underneath.
ItemDelegate {
    id: root

    property string name
    property string logo
    property string nowTitle
    property real nowProgress: 0
    property int number: 0
    property bool favorite: false
    property bool current: false

    readonly property bool emphasized: hovered || visualFocus

    padding: 0
    hoverEnabled: true
    focusPolicy: Qt.TabFocus

    background: Item {
        Shadow {
            target: card
            opacity: root.emphasized ? 1 : 0.5
            Behavior on opacity { NumberAnimation { duration: AppTheme.normal } }
        }
        Rectangle {
            id: card
            anchors.fill: parent
            radius: AppTheme.radius
            color: root.emphasized ? AppTheme.surfaceHover : AppTheme.surface
            border.width: root.current || root.visualFocus ? 2 : 1
            border.color: root.current || root.visualFocus ? AppTheme.accent : AppTheme.stroke
            scale: root.emphasized ? 1.03 : 1
            Behavior on scale { NumberAnimation { duration: AppTheme.normal; easing.type: Easing.OutCubic } }
            Behavior on color { ColorAnimation { duration: AppTheme.fast } }
        }
    }

    contentItem: ColumnLayout {
        spacing: 0
        scale: root.emphasized ? 1.03 : 1
        Behavior on scale { NumberAnimation { duration: AppTheme.normal; easing.type: Easing.OutCubic } }

        // Logo area
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 70

            Label {
                anchors.centerIn: parent
                visible: logoImage.status !== Image.Ready
                text: AppTheme.initials(root.name)
                font.pixelSize: 28
                font.weight: Font.Bold
                color: AppTheme.textTertiary
            }
            Image {
                id: logoImage
                anchors.fill: parent
                anchors.margins: 20
                anchors.topMargin: 24
                source: root.logo
                asynchronous: true
                fillMode: Image.PreserveAspectFit
                sourceSize: Qt.size(240, 140)
            }
            // LIVE badge
            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.margins: 10
                width: liveRow.implicitWidth + 12
                height: 18
                radius: 4
                color: "#26FF3B4E"
                Row {
                    id: liveRow
                    anchors.centerIn: parent
                    spacing: 4
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 6
                        height: 6
                        radius: 3
                        color: AppTheme.live
                    }
                    Label {
                        text: root.number > 0 ? qsTr("LIVE · %1").arg(root.number) : qsTr("LIVE")
                        color: AppTheme.live
                        font.pixelSize: 10
                        font.weight: Font.Bold
                    }
                }
            }
            IconImage {
                visible: root.favorite
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: 10
                source: "icons/star.svg"
                sourceSize: Qt.size(16, 16)
                color: AppTheme.favorite
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: 12
            Layout.topMargin: 0
            spacing: 4
            Label {
                Layout.fillWidth: true
                text: root.name
                elide: Text.ElideRight
                font.pixelSize: AppTheme.fontBody
                font.weight: Font.DemiBold
                color: AppTheme.textPrimary
            }
            Label {
                Layout.fillWidth: true
                text: root.nowTitle || qsTr("No programme information")
                elide: Text.ElideRight
                font.pixelSize: AppTheme.fontCaption
                color: root.nowTitle ? AppTheme.textSecondary : AppTheme.textTertiary
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 3
                Layout.topMargin: 4
                radius: 1.5
                color: AppTheme.surfaceHover
                opacity: root.nowTitle.length > 0 ? 1 : 0
                Rectangle {
                    width: parent.width * root.nowProgress
                    height: parent.height
                    radius: 1.5
                    color: AppTheme.accent
                }
            }
        }
    }
}
