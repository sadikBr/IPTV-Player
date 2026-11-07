import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import IPTVPlayer

// Category list on the left of wide screens.
ListView {
    id: root

    property int selectedCategory: CategoryListModel.AllCategories
    signal categorySelected(int categoryId)

    clip: true
    spacing: 2
    topMargin: 4
    bottomMargin: 12
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar {}


    delegate: ItemDelegate {
        id: item
        required property int categoryId
        required property string name
        required property int count
        required property int index
        readonly property bool selected: categoryId === root.selectedCategory

        width: root.width
        height: 40
        leftPadding: 14
        rightPadding: 12
        hoverEnabled: true
        onClicked: root.categorySelected(categoryId)

        background: Rectangle {
            radius: AppTheme.radiusSmall
            color: item.selected ? AppTheme.selection : item.hovered ? AppTheme.hover : "transparent"
            Behavior on color { ColorAnimation { duration: AppTheme.fast } }
        }
        contentItem: RowLayout {
            spacing: AppTheme.spacing
            Label {
                text: item.name
                elide: Text.ElideRight
                color: item.selected ? AppTheme.textPrimary : AppTheme.textSecondary
                font.pixelSize: AppTheme.fontSmall
                font.weight: item.selected || item.categoryId < 0 ? Font.DemiBold : Font.Normal
                Layout.fillWidth: true
            }
            Label {
                text: item.count
                color: item.selected ? AppTheme.accent : AppTheme.textTertiary
                font.pixelSize: AppTheme.fontCaption
            }
        }

        // Divider after "Favorites"
        Rectangle {
            visible: item.categoryId === CategoryListModel.FavoritesCategory
            anchors.bottom: parent.bottom
            anchors.bottomMargin: -2
            x: 10
            width: parent.width - 20
            height: 1
            color: AppTheme.stroke
        }
    }
}
