import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import IPTVPlayer

// Live TV / Movies / Series switcher for one source. Sections without
// content are hidden; the bar disappears when there is only live TV.
TabBar {
    id: root

    property var sourceInfo: ({})
    property string current: "live"

    signal sectionSelected(string kind)

    readonly property var sections: {
        const all = [
            { kind: "live", label: qsTr("Live TV"), count: sourceInfo.liveCount || 0 },
            { kind: "movie", label: qsTr("Movies"), count: sourceInfo.movieCount || 0 },
            { kind: "series", label: qsTr("Series"), count: sourceInfo.seriesCount || 0 },
        ]
        return all.filter(s => s.count > 0 || s.kind === current)
    }

    visible: sections.length > 1
    Material.background: AppTheme.surface
    currentIndex: sections.findIndex(s => s.kind === current)

    Repeater {
        model: root.sections
        delegate: TabButton {
            required property var modelData
            text: modelData.label
            width: Math.max(implicitWidth, 120)
            onClicked: {
                if (modelData.kind !== root.current)
                    root.sectionSelected(modelData.kind)
            }
        }
    }
}
