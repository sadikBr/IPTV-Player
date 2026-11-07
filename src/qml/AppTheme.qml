pragma Singleton
import QtQuick

// Design tokens: colours, type scale, spacing and motion. Every screen uses
// these instead of hard-coded values so the app looks like one product.
QtObject {
    // --- Colour ---
    // Surfaces, darkest to lightest
    readonly property color background: "#0A0C11"
    readonly property color backgroundRaised: "#0E1118"
    readonly property color surface: "#141822"
    readonly property color surfaceHigh: "#1B2030"
    readonly property color surfaceHover: "#232A3C"
    readonly property color stroke: "#232937"
    readonly property color strokeStrong: "#333B4E"

    // Text
    readonly property color textPrimary: "#F3F5FA"
    readonly property color textSecondary: "#A6AEC0"
    readonly property color textTertiary: "#6E768A"

    // Brand: violet to cyan
    readonly property color accent: "#8B7BFF"
    readonly property color accentStrong: "#6C5CF5"
    readonly property color accent2: "#38D3F2"
    readonly property color selection: "#2B8B7BFF"
    readonly property color hover: "#12FFFFFF"

    // Meaning
    readonly property color live: "#FF4D63"
    readonly property color favorite: "#FFC857"
    readonly property color error: "#FF6B7A"
    readonly property color success: "#3DDC97"
    readonly property color overlay: "#D907080C"

    // --- Type ---
    readonly property string fontFamily: "Inter"
    readonly property int fontDisplay: 36
    readonly property int fontH1: 28
    readonly property int fontH2: 20
    readonly property int fontH3: 16
    readonly property int fontBody: 14
    readonly property int fontSmall: 13
    readonly property int fontCaption: 12

    // --- Layout ---
    readonly property int spacing: 12
    readonly property int margin: 16
    readonly property int pageMargin: 28   // left/right margin on wide screens
    readonly property int radiusSmall: 8
    readonly property int radius: 12
    readonly property int radiusLarge: 18
    readonly property int contentMaxWidth: 1400

    // Phones in portrait are narrower than this.
    readonly property int compactWidth: 600
    // From this width on, the side navigation is shown.
    readonly property int wideWidth: 900

    // --- Motion ---
    readonly property int fast: 120
    readonly property int normal: 220

    // "18:30" (24-hour, no seconds)
    function time(date) {
        return date ? date.toLocaleTimeString(Qt.locale(), "HH:mm") : ""
    }

    // Initials for avatars: "My Provider" -> "MP"
    function initials(name) {
        const words = (name || "?").trim().split(/\s+/)
        return (words[0].charAt(0) + (words.length > 1 ? words[1].charAt(0) : "")).toUpperCase()
    }
}
