import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import IPTVPlayer

// Live channel tile: logo on top, name and what's on underneath. Each
// channel glows in a colour of its own; pressing it gives a little bounce.
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
    readonly property color tint: AppTheme.vivid(name)

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
            border.color: root.current || root.visualFocus ? AppTheme.accent
                                                           : Qt.rgba(root.tint.r, root.tint.g, root.tint.b, 0.22)
            scale: root.pressed ? 0.94 : root.emphasized ? 1.03 : 1
            Behavior on scale { NumberAnimation { duration: 300; easing.type: Easing.OutBack; easing.overshoot: 2.5 } }
            Behavior on color { ColorAnimation { duration: AppTheme.fast } }

            // The channel's own colour, glowing from the top corner
            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                gradient: Gradient {
                    orientation: Gradient.Vertical
                    GradientStop { position: 0.0; color: Qt.rgba(root.tint.r, root.tint.g, root.tint.b, 0.26) }
                    GradientStop { position: 0.65; color: Qt.rgba(root.tint.r, root.tint.g, root.tint.b, 0.04) }
                    GradientStop { position: 1.0; color: "transparent" }
                }
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: 0
        scale: card.scale

        // Logo area
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 70

            // Soft halo behind the logo
            Rectangle {
                anchors.centerIn: parent
                anchors.verticalCenterOffset: 6
                width: Math.min(parent.width, parent.height) * 0.9
                height: width
                radius: width / 2
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Qt.rgba(root.tint.r, root.tint.g, root.tint.b, 0.20) }
                    GradientStop { position: 1.0; color: "transparent" }
                }
            }
            Label {
                anchors.centerIn: parent
                anchors.verticalCenterOffset: 6
                visible: logoImage.status !== Image.Ready
                text: AppTheme.initials(root.name)
                font.pixelSize: 30
                font.weight: Font.Black
                color: root.tint
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
                radius: 9
                color: "#33FF3B4E"
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
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: AppTheme.accent }
                        GradientStop { position: 1.0; color: AppTheme.accentPink }
                    }
                }
            }
        }
    }
}
