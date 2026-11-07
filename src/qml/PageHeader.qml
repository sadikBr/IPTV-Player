import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import IPTVPlayer

// Title row at the top of each section: optional back button, title,
// anything placed inside (search, actions) and, on phones, the provider.
Item {
    id: root

    property string title
    property string subtitle
    property bool showBack: false
    property bool narrow: width < AppTheme.compactWidth
    default property alias actions: actionRow.data

    signal backClicked()

    implicitHeight: narrow ? 64 : 76

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: root.narrow ? 8 : AppTheme.pageMargin
        anchors.rightMargin: root.narrow ? 8 : AppTheme.pageMargin
        spacing: AppTheme.spacing

        IconButton {
            visible: root.showBack
            iconName: "back"
            tip: qsTr("Back")
            onClicked: root.backClicked()
        }

        ColumnLayout {
            spacing: 0
            Layout.leftMargin: root.showBack || !root.narrow ? 0 : 8
            Label {
                text: root.title
                font.pixelSize: root.narrow ? 22 : AppTheme.fontH1
                font.weight: Font.Bold
                color: AppTheme.textPrimary
                elide: Text.ElideRight
                Layout.maximumWidth: root.width * 0.5
            }
            Label {
                visible: text.length > 0 && !root.narrow
                text: root.subtitle
                font.pixelSize: AppTheme.fontSmall
                color: AppTheme.textTertiary
            }
        }

        Item { Layout.fillWidth: true }

        RowLayout {
            id: actionRow
            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            spacing: 6
        }

        ProviderSwitcher {
            visible: root.narrow && AppState.hasSource
            compact: true
        }
    }
}
