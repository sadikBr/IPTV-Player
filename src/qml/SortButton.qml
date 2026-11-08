import QtQuick
import QtQuick.Controls
import IPTVPlayer

// Sort menu for a list of channels, movies or series. The choice is
// remembered per section.
IconButton {
    id: root

    property ChannelListModel model
    property string kind: "live"

    readonly property var options: kind === "live"
            ? [{ order: "default", label: qsTr("Provider order") },
               { order: "number", label: qsTr("Channel number") },
               { order: "name", label: qsTr("Name (A–Z)") },
               { order: "name-desc", label: qsTr("Name (Z–A)") }]
            : [{ order: "default", label: qsTr("Provider order") },
               { order: "newest", label: qsTr("Recently added") },
               { order: "name", label: qsTr("Name (A–Z)") },
               { order: "name-desc", label: qsTr("Name (Z–A)") },
               { order: "rating", label: qsTr("Rating") },
               { order: "year", label: qsTr("Year (newest first)") }]

    iconName: "sort"
    tip: qsTr("Sort")
    checked: model && model.sortOrder !== "default"
    checkable: false
    onClicked: menu.popup(root, 0, root.height + 4)

    Component.onCompleted: {
        if (model)
            model.sortOrder = AppSettings.sortOrder(kind)
    }

    Menu {
        id: menu
        Repeater {
            model: root.options
            delegate: MenuItem {
                required property var modelData
                text: modelData.label
                checkable: true
                checked: root.model && root.model.sortOrder === modelData.order
                onTriggered: {
                    root.model.sortOrder = modelData.order
                    AppSettings.setSortOrder(root.kind, modelData.order)
                }
            }
        }
    }
}
