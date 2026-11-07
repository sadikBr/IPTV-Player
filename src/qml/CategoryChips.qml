import QtQuick
import QtQuick.Controls
import IPTVPlayer

// Horizontally scrolling category chips (phones).
ListView {
    id: root

    property int selectedCategory: CategoryListModel.AllCategories
    signal categorySelected(int categoryId)

    implicitHeight: 34
    orientation: ListView.Horizontal
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
        selected: categoryId === root.selectedCategory
        onClicked: root.categorySelected(categoryId)
    }
}
