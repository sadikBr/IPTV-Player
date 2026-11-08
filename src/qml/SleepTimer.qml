pragma Singleton
import QtQuick

// Sleep timer: playback (TV or radio) fades out and stops when it runs out.
QtObject {
    id: root

    property date endsAt: new Date(0)
    readonly property bool active: endsAt.getTime() > 0
    // Whole minutes left (rounded up), for display
    property int minutesLeft: 0
    // 1 normally, falling to 0 during the last 10 seconds (fade out)
    property real fade: 1

    // The player pages stop playback when this is emitted.
    signal expired()

    function start(minutes) {
        endsAt = new Date(Date.now() + minutes * 60000)
        update()
    }
    function startUntil(date) {
        endsAt = date
        update()
    }
    function stop() {
        endsAt = new Date(0)
        fade = 1
        minutesLeft = 0
    }
    function update() {
        if (!active)
            return
        const left = endsAt.getTime() - Date.now()
        minutesLeft = Math.max(0, Math.ceil(left / 60000))
        fade = Math.max(0, Math.min(1, left / 10000))
        if (left <= 0) {
            stop()
            expired()
        }
    }
    // "Stops in 25 min" / "Stops at 23:30"
    function label() {
        return minutesLeft > 1 ? qsTr("Stops in %1 min").arg(minutesLeft) : qsTr("Stops in 1 min")
    }

    property Timer ticker: Timer {
        interval: 1000
        repeat: true
        running: root.active
        onTriggered: root.update()
    }
}
