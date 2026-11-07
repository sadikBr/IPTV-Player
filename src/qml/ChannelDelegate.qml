import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import IPTVPlayer

// One channel in a list: number, logo, name, what's on now, favorite.
ItemDelegate {
    id: root

    required property int index
    required property string name
    required property string logo
    required property int number
    required property bool favorite
    required property string nowTitle
    required property real nowProgress

    property bool current: false

    signal favoriteClicked()

    height: 68
    padding: 0
    leftPadding: 12
    rightPadding: 6
    hoverEnabled: true

    background: Rectangle {
        radius: AppTheme.radius
        color: root.current ? AppTheme.selection
             : root.hovered || root.visualFocus ? AppTheme.surface : "transparent"
        border.color: root.visualFocus ? AppTheme.accent : "transparent"
        Behavior on color { ColorAnimation { duration: AppTheme.fast } }
    }

    contentItem: RowLayout {
        spacing: 14

        Label {
            text: root.number > 0 ? root.number : ""
            color: root.current ? AppTheme.accent : AppTheme.textTertiary
            horizontalAlignment: Text.AlignRight
            Layout.preferredWidth: 32
            font.pixelSize: AppTheme.fontCaption
            font.weight: Font.DemiBold
        }

        // Logo tile
        Rectangle {
            Layout.preferredWidth: 64
            Layout.preferredHeight: 44
            radius: AppTheme.radiusSmall
            color: AppTheme.surfaceHigh
            border.color: AppTheme.stroke

            Label {
                anchors.centerIn: parent
                visible: logoImage.status !== Image.Ready
                text: AppTheme.initials(root.name)
                color: AppTheme.textTertiary
                font.pixelSize: AppTheme.fontSmall
                font.weight: Font.Bold
            }
            Image {
                id: logoImage
                anchors.fill: parent
                anchors.margins: 6
                source: root.logo
                asynchronous: true
                cache: true
                fillMode: Image.PreserveAspectFit
                sourceSize: Qt.size(128, 88)
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Label {
                Layout.fillWidth: true
                text: root.name
                elide: Text.ElideRight
                color: root.current ? AppTheme.textPrimary : AppTheme.textPrimary
                font.pixelSize: AppTheme.fontBody
                font.weight: root.current ? Font.Bold : Font.DemiBold
            }

            // What's on now (TV guide)
            RowLayout {
                visible: root.nowTitle.length > 0
                Layout.fillWidth: true
                spacing: 10
                Label {
                    Layout.fillWidth: true
                    text: root.nowTitle
                    elide: Text.ElideRight
                    color: AppTheme.textSecondary
                    font.pixelSize: AppTheme.fontCaption
                }
                Rectangle {
                    Layout.preferredWidth: 56
                    Layout.preferredHeight: 3
                    radius: 1.5
                    color: AppTheme.surfaceHigh
                    Rectangle {
                        width: parent.width * root.nowProgress
                        height: parent.height
                        radius: 1.5
                        color: AppTheme.accent
                    }
                }
            }
        }

        IconButton {
            iconName: root.favorite ? "star" : "star-outline"
            tip: root.favorite ? qsTr("Remove from favorites") : qsTr("Add to favorites")
            iconColor: root.favorite ? AppTheme.favorite : AppTheme.textTertiary
            size: 36
            opacity: root.favorite || root.hovered || root.current ? 1 : 0.5
            onClicked: root.favoriteClicked()
        }
    }
}
