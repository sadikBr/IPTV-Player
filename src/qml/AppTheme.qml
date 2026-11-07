pragma Singleton
import QtQuick

QtObject {
    readonly property color accent: "#4FC3F7"
    readonly property color background: "#121418"
    readonly property color surface: "#1C1F26"
    readonly property color textPrimary: "#ECEFF4"
    readonly property color textSecondary: "#9AA3B2"
    readonly property color error: "#FF6B6B"
    readonly property color overlay: "#B3000000"
    readonly property color favorite: "#FFC107"
    readonly property color selection: "#264FC3F7"

    readonly property int spacing: 12
    readonly property int margin: 16
    readonly property int radius: 10
    readonly property int contentMaxWidth: 680

    // Phones in portrait are narrower than this.
    readonly property int compactWidth: 600

    // "18:30" (24-hour, no seconds)
    function time(date) {
        return date ? date.toLocaleTimeString(Qt.locale(), "HH:mm") : ""
    }
}
