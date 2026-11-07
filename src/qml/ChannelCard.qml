import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import IPTVPlayer

// Live channel tile: logo on top, name and what's on underneath.
ItemDelegate {
    id: root

    property string name
    property string logo
    property string nowTitle
    property real nowProgress: 0

    width: 210
    height: 170
    padding: 0
    hoverEnabled: true

    background: Rectangle {
        radius: AppTheme.radius
        color: root.hovered ? AppTheme.surfaceHover : AppTheme.surface
        border.color: root.hovered || root.visualFocus ? AppTheme.accent : AppTheme.stroke
        border.width: 1
        Behavior on color { ColorAnimation { duration: AppTheme.fast } }
    }

    contentItem: ColumnLayout {
        spacing: 0

        // Logo area
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            Layout.margins: 1
            radius: AppTheme.radius - 1
            gradient: Gradient {
                GradientStop { position: 0.0; color: AppTheme.surfaceHigh }
                GradientStop { position: 1.0; color: AppTheme.surface }
            }
            Label {
                anchors.centerIn: parent
                visible: logoImage.status !== Image.Ready
                text: AppTheme.initials(root.name)
                font.pixelSize: 26
                font.weight: Font.Bold
                color: AppTheme.textTertiary
            }
            Image {
                id: logoImage
                anchors.fill: parent
                anchors.margins: 18
                source: root.logo
                asynchronous: true
                fillMode: Image.PreserveAspectFit
                sourceSize: Qt.size(200, 120)
            }
            // LIVE badge
            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.margins: 8
                width: liveLabel.implicitWidth + 12
                height: 18
                radius: 4
                color: AppTheme.live
                Label {
                    id: liveLabel
                    anchors.centerIn: parent
                    text: qsTr("LIVE")
                    color: "white"
                    font.pixelSize: 10
                    font.weight: Font.Bold
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: 12
            Layout.topMargin: 10
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
                text: root.nowTitle || qsTr("Live channel")
                elide: Text.ElideRight
                font.pixelSize: AppTheme.fontCaption
                color: AppTheme.textSecondary
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 3
                radius: 1.5
                color: AppTheme.surfaceHigh
                visible: root.nowTitle.length > 0
                Rectangle {
                    width: parent.width * root.nowProgress
                    height: parent.height
                    radius: 1.5
                    color: AppTheme.accent
                }
            }
        }
    }

    scale: pressed ? 0.98 : 1
    Behavior on scale { NumberAnimation { duration: AppTheme.fast } }
}
