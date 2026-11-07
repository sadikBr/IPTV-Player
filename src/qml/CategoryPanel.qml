import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import IPTVPlayer

// Category list on the left of wide screens.
ListView {
    id: root

    property int selectedCategory: CategoryListModel.AllCategories
    property int hiddenCount: 0
    signal categorySelected(int categoryId)
    signal hideRequested(int categoryId)
    signal manageRequested()

    clip: true
    spacing: 2
    // Arrow keys move focus between rows (SpatialNavigation), not the selection.
    keyNavigationEnabled: false
    topMargin: 4
    bottomMargin: 12
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: AppScrollBar {}


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
        focusPolicy: Qt.TabFocus
        onClicked: root.categorySelected(categoryId)
        onPressAndHold: categoryMenu.openFor(categoryId, name)

        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: categoryMenu.openFor(item.categoryId, item.name)
        }

        background: Rectangle {
            radius: AppTheme.radiusSmall
            color: item.selected ? AppTheme.selection : item.hovered ? AppTheme.hover : "transparent"
            Behavior on color { ColorAnimation { duration: AppTheme.fast } }
            FocusRing { target: item; gap: 0 }
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

    footer: AbstractButton {
        width: root.width
        height: 44
        hoverEnabled: true
        onClicked: root.manageRequested()
        focusPolicy: Qt.TabFocus
        contentItem: Label {
            leftPadding: 14
            verticalAlignment: Text.AlignVCenter
            text: root.hiddenCount > 0 ? qsTr("%1 hidden · Choose categories").arg(root.hiddenCount)
                                       : qsTr("Choose categories…")
            color: parent.hovered || parent.visualFocus ? AppTheme.textPrimary : AppTheme.textTertiary
            font.pixelSize: AppTheme.fontSmall
        }
        background: Item {
            FocusRing { target: parent.parent; gap: 0; radius: AppTheme.radiusSmall }
        }
    }

    CategoryMenu {
        id: categoryMenu
        onHideRequested: (id) => root.hideRequested(id)
        onManageRequested: root.manageRequested()
    }
}
