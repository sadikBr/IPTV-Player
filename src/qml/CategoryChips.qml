import QtQuick
import QtQuick.Controls
import IPTVPlayer

// Horizontally scrolling category chips (phones).
ListView {
    id: root

    property int selectedCategory: CategoryListModel.AllCategories
    signal categorySelected(int categoryId)
    signal hideRequested(int categoryId)
    signal manageRequested()

    implicitHeight: 40
    orientation: ListView.Horizontal
    keyNavigationEnabled: false // arrow keys move focus between chips
    spacing: 8
    clip: true
    leftMargin: AppTheme.margin
    rightMargin: AppTheme.margin
    boundsBehavior: Flickable.StopAtBounds

    delegate: Chip {
        required property int categoryId
        required property string name
        required property int count
        text: name
        emoji: categoryId === CategoryListModel.AllCategories ? "✨"
             : categoryId === CategoryListModel.FavoritesCategory ? "⭐"
             : AppTheme.categoryEmoji(name)
        tinted: categoryId > 0
        selected: categoryId === root.selectedCategory
        onClicked: root.categorySelected(categoryId)
        onPressAndHold: categoryMenu.openFor(categoryId, name)
    }

    CategoryMenu {
        id: categoryMenu
        onHideRequested: (id) => root.hideRequested(id)
        onManageRequested: root.manageRequested()
    }
}
