import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import IPTVPlayer

// A titled, horizontally scrolling row of cards (home page).
ColumnLayout {
    id: root

    property string title
    property string seeAllText: qsTr("See all")
    property bool showSeeAll: false
    property alias model: list.model
    property alias delegate: list.delegate
    property alias count: list.count
    property int rowHeight: 260
    property real sideMargin: AppTheme.pageMargin

    signal seeAllClicked()

    spacing: 10
    visible: list.count > 0

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: root.sideMargin
        Layout.rightMargin: root.sideMargin
        Label {
            text: root.title
            font.pixelSize: AppTheme.fontH2
            font.weight: Font.Bold
            color: AppTheme.textPrimary
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
        AbstractButton {
            visible: root.showSeeAll
            hoverEnabled: true
            onClicked: root.seeAllClicked()
            contentItem: RowLayout {
                spacing: 2
                Label {
                    text: root.seeAllText
                    color: parent.parent.hovered ? AppTheme.textPrimary : AppTheme.accent
                    font.pixelSize: AppTheme.fontSmall
                    font.weight: Font.DemiBold
                }
                IconImage {
                    source: "icons/chevron-right.svg"
                    sourceSize: Qt.size(16, 16)
                    color: AppTheme.accent
                }
            }
            background: Item {}
        }
    }

    ListView {
        id: list
        Layout.fillWidth: true
        Layout.preferredHeight: root.rowHeight
        orientation: ListView.Horizontal
        spacing: 14
        clip: true
        leftMargin: root.sideMargin
        rightMargin: root.sideMargin
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.horizontal: AppScrollBar {}
    }
}
