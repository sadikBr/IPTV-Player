import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import IPTVPlayer

// Stands in for a poster or still that is missing or doesn't load: a deep
// colour gradient picked from the title, an icon and the title.
Rectangle {
    id: root

    property string title
    property string iconName: "film"
    property bool showTitle: true
    property int fontSize: AppTheme.fontBody

    gradient: Gradient {
        GradientStop { position: 0.0; color: AppTheme.artColor(root.title, 0.30) }
        GradientStop { position: 1.0; color: AppTheme.artColor(root.title, 0.12) }
    }

    Column {
        anchors.centerIn: parent
        width: parent.width - 24
        spacing: 10
        IconImage {
            anchors.horizontalCenter: parent.horizontalCenter
            source: "icons/" + root.iconName + ".svg"
            sourceSize: Qt.size(Math.round(Math.min(root.width, root.height) * 0.18),
                                Math.round(Math.min(root.width, root.height) * 0.18))
            color: "#59FFFFFF"
        }
        Label {
            visible: root.showTitle && root.title.length > 0
            width: parent.width
            text: root.title
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            maximumLineCount: 4
            elide: Text.ElideRight
            color: "#E6FFFFFF"
            font.pixelSize: root.fontSize
            font.weight: Font.DemiBold
        }
    }
}
