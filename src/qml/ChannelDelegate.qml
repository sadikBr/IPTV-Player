import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import IPTVPlayer

// One row in a channel list: number, logo, name and a favorite star.
ItemDelegate {
    id: root

    required property int index
    required property string name
    required property string logo
    required property int number
    required property bool favorite

    property bool current: false

    signal favoriteClicked()

    height: 56
    padding: 0
    leftPadding: AppTheme.spacing
    rightPadding: 4

    background: Rectangle {
        color: root.current ? AppTheme.selection
             : root.hovered || root.visualFocus ? "#14FFFFFF" : "transparent"
        Rectangle {
            visible: root.current
            width: 3
            height: parent.height
            color: AppTheme.accent
        }
    }

    contentItem: RowLayout {
        spacing: AppTheme.spacing

        Label {
            text: root.number > 0 ? root.number : ""
            color: AppTheme.textSecondary
            horizontalAlignment: Text.AlignRight
            Layout.preferredWidth: 36
            font.pixelSize: 13
        }

        // Logo, or the first letter of the name while it loads / if missing
        Rectangle {
            Layout.preferredWidth: 44
            Layout.preferredHeight: 32
            radius: 4
            color: logoImage.status === Image.Ready ? "transparent" : AppTheme.surface

            Label {
                anchors.centerIn: parent
                visible: logoImage.status !== Image.Ready
                text: root.name.length > 0 ? root.name.charAt(0).toUpperCase() : "?"
                color: AppTheme.textSecondary
                font.bold: true
            }
            Image {
                id: logoImage
                anchors.fill: parent
                source: root.logo
                asynchronous: true
                cache: true
                fillMode: Image.PreserveAspectFit
                sourceSize: Qt.size(88, 64)
            }
        }

        Label {
            text: root.name
            elide: Text.ElideRight
            color: root.current ? AppTheme.accent : AppTheme.textPrimary
            Layout.fillWidth: true
            font.pixelSize: 15
        }

        IconButton {
            iconName: root.favorite ? "star" : "star-outline"
            tip: root.favorite ? qsTr("Remove from favorites") : qsTr("Add to favorites")
            icon.color: root.favorite ? AppTheme.favorite : AppTheme.textSecondary
            onClicked: root.favoriteClicked()
        }
    }
}
