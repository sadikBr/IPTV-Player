import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import IPTVPlayer

// Small rounded label for metadata (year, duration, rating, genre).
Rectangle {
    id: root

    property string text
    property string iconName
    property color iconColor: AppTheme.textPrimary

    visible: text.length > 0
    implicitWidth: row.implicitWidth + 18
    implicitHeight: 28
    radius: 6
    color: AppTheme.glass
    border.color: AppTheme.glassStroke

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 5
        IconImage {
            visible: root.iconName.length > 0
            anchors.verticalCenter: parent.verticalCenter
            source: root.iconName ? "icons/" + root.iconName + ".svg" : ""
            sourceSize: Qt.size(13, 13)
            color: root.iconColor
        }
        Label {
            anchors.verticalCenter: parent.verticalCenter
            text: root.text
            color: AppTheme.textPrimary
            font.pixelSize: AppTheme.fontCaption
            font.weight: Font.DemiBold
        }
    }
}
