pragma Singleton
import QtQuick
import IPTVPlayer

// App-wide navigation state: the current provider and section. Components
// call the request functions; Main.qml performs the navigation.
QtObject {
    id: state

    // The provider whose content is shown (-1: none yet)
    property int sourceId: -1
    // SourceManager.source(sourceId), refreshed when providers change
    property var sourceInfo: ({})
    // "home", "live", "movie", "series", "guide" or "settings"
    property string section: "home"

    readonly property bool hasSource: sourceId > 0

    signal sectionRequested(string section)
    signal addProviderRequested()
    signal playStreamRequested(url url, string title)

    function refreshSource() {
        sourceInfo = sourceId > 0 ? SourceManager.source(sourceId) : ({})
    }

    function setSource(id) {
        sourceId = id
        AppSettings.lastSourceId = id
        refreshSource()
    }

    // Picks a valid provider: the last used one, else the first, else none.
    function ensureSource(firstSourceId) {
        if (SourceManager.sourceExists(sourceId))
            return refreshSource()
        if (SourceManager.sourceExists(AppSettings.lastSourceId))
            return setSource(AppSettings.lastSourceId)
        setSource(firstSourceId > 0 ? firstSourceId : -1)
    }

    property Connections sourceWatcher: Connections {
        target: SourceManager
        function onSourcesChanged() { state.refreshSource() }
    }
}
