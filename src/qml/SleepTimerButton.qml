import QtQuick
import QtQuick.Controls
import IPTVPlayer

// Sleep timer menu: stop after 15-90 minutes, or when the current program ends.
IconButton {
    id: root

    // End of the program on now (live TV), if known
    property var programEnd: null

    iconName: "moon"
    checkable: false
    checked: SleepTimer.active
    tip: SleepTimer.active ? SleepTimer.label() : qsTr("Sleep timer")
    onClicked: menu.popup(root, 0, -menu.implicitHeight - 4)

    Menu {
        id: menu
        MenuItem {
            enabled: false
            text: SleepTimer.active ? SleepTimer.label() : qsTr("Stop playing after…")
        }
        Repeater {
            model: [15, 30, 45, 60, 90]
            delegate: MenuItem {
                required property int modelData
                text: qsTr("%1 minutes").arg(modelData)
                onTriggered: SleepTimer.start(modelData)
            }
        }
        MenuItem {
            visible: root.programEnd !== null && root.programEnd.getTime() > Date.now()
            height: visible ? implicitHeight : 0
            text: root.programEnd ? qsTr("End of this program (%1)").arg(AppTheme.time(root.programEnd)) : ""
            onTriggered: SleepTimer.startUntil(root.programEnd)
        }
        MenuItem {
            visible: SleepTimer.active
            height: visible ? implicitHeight : 0
            text: qsTr("Turn off the sleep timer")
            onTriggered: SleepTimer.stop()
        }
    }
}
