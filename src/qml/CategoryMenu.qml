import QtQuick
import QtQuick.Controls
import IPTVPlayer

// Right-click / long-press menu on a category.
Menu {
    id: menu

    property int categoryId: -1
    property string categoryName

    signal hideRequested(int categoryId)
    signal manageRequested()

    function openFor(id, name) {
        categoryId = id
        categoryName = name
        popup()
    }

    MenuItem {
        text: qsTr("Hide “%1”").arg(menu.categoryName)
        enabled: menu.categoryId > 0 // not "All" or "Favorites"
        onTriggered: menu.hideRequested(menu.categoryId)
    }
    MenuItem {
        text: qsTr("Choose categories…")
        onTriggered: menu.manageRequested()
    }
}
