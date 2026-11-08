pragma Singleton
import QtQuick

// Design tokens: colours, type scale, spacing and motion. Every screen uses
// these instead of hard-coded values so the app looks like one product.
//
// Direction: vibrant and playful on a dark base. Content brings its own
// colour (every channel / title gets a hue of its own), active things glow
// in an orange-to-pink gradient, and presses bounce.
QtObject {
    // --- Colour ---
    // Surfaces, darkest to lightest (neutral, no tint)
    readonly property color background: "#0B0B0D"
    readonly property color backgroundRaised: "#0F0F12"
    readonly property color surface: "#16161A"
    readonly property color surfaceHigh: "#1E1E23"
    readonly property color surfaceHover: "#26262C"
    readonly property color stroke: "#232328"
    readonly property color strokeStrong: "#34343B"

    // Glass: translucent panels over artwork
    readonly property color glass: "#33FFFFFF"
    readonly property color glassHover: "#4DFFFFFF"
    readonly property color glassStroke: "#29FFFFFF"

    // Text
    readonly property color textPrimary: "#F5F5F7"
    readonly property color textSecondary: "#A3A3AB"
    readonly property color textTertiary: "#6B6B74"

    // The one accent: warm orange-red
    readonly property color accent: "#FF5A36"
    readonly property color accentStrong: "#E8462A"
    readonly property color accentSoft: "#26FF5A36"
    readonly property color accent2: "#FF8A5B"   // lighter tint for small highlights
    readonly property color accentPink: "#FF3D8B" // other end of the accent gradient
    readonly property color selection: "#1FFFFFFF"
    readonly property color hover: "#0FFFFFFF"

    // Meaning
    readonly property color live: "#FF3B4E"
    readonly property color favorite: "#FFC24B"
    readonly property color error: "#FF6B6B"
    readonly property color success: "#34D399"
    readonly property color overlay: "#E6000000"

    // --- Type ---
    readonly property string fontFamily: "Inter"
    readonly property int fontHero: 48
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
    readonly property int pageMargin: 40   // left/right margin on wide screens
    readonly property int radiusSmall: 8
    readonly property int radius: 14
    readonly property int radiusLarge: 22
    readonly property int contentMaxWidth: 1400

    // Phones in portrait are narrower than this.
    readonly property int compactWidth: 600
    // From this width on, the side navigation is shown.
    readonly property int wideWidth: 900

    // --- Motion ---
    readonly property int fast: 120
    readonly property int normal: 220
    readonly property int slow: 600

    // "18:30" (24-hour, no seconds)
    function time(date) {
        return date ? date.toLocaleTimeString(Qt.locale(), "HH:mm") : ""
    }

    // Initials for avatars: "My Provider" -> "MP"
    function initials(name) {
        // Skip prefixes like "[EN]" or "UK |" and anything that isn't a letter or digit.
        const clean = (name || "").replace(/^\s*[\[(][^\])]*[\])]\s*/, "")
                                  .replace(/^\s*[A-Za-z]{2,4}\s*[|:]\s*/, "")
        const words = clean.split(/[\s|:\-_.,\/]+/).filter(w => w.length > 0)
        if (words.length === 0)
            return "?"
        return (words[0].charAt(0) + (words.length > 1 ? words[1].charAt(0) : "")).toUpperCase()
    }

    // Whether an Image shows real artwork: loaded, and not a tiny image.
    function hasArtwork(image) {
        return image !== null && image.status === Image.Ready && image.implicitWidth > 4
    }

    // A deep colour picked from a title, for placeholders: each title gets
    // its own, and it stays the same.
    function artColor(title, lightness) {
        let hash = 0
        const text = title || ""
        for (let i = 0; i < text.length; ++i)
            hash = (Math.imul(hash, 31) + text.charCodeAt(i)) | 0
        // Mix the bits so similar titles ("Movie 3", "Movie 6") get different colours.
        hash = Math.imul(hash ^ (hash >>> 16), 0x45d9f3b)
        hash = Math.imul(hash ^ (hash >>> 16), 0x45d9f3b)
        hash = hash ^ (hash >>> 16)
        const hue = ((hash >>> 0) % 360) / 360
        return Qt.hsla(hue, 0.45, lightness === undefined ? 0.22 : lightness, 1)
    }

    // A bright colour of a title's own (same hue as artColor), for tints and glows.
    function vivid(title, alpha) {
        const c = artColor(title, 0.5)
        return Qt.hsla(c.hslHue, 0.85, 0.6, alpha === undefined ? 1 : alpha)
    }

    // "Good morning" / "Good afternoon" / "Good evening" / "Good night"
    function greeting() {
        const h = new Date().getHours()
        return h < 5 ? qsTr("Good night") : h < 12 ? qsTr("Good morning")
             : h < 18 ? qsTr("Good afternoon") : qsTr("Good evening")
    }

    // A small picture for a category, from words in its name ("" if none fits).
    function categoryEmoji(name) {
        const n = (name || "").toLowerCase()
        const table = [
            [/kid|child|cartoon|enfant|nino|kinder|أطفال|anime/, "🧸"],
            [/sport|football|soccer|foot|bein|nba|nfl|f1|racing|tennis|golf|ufc|boxing|رياض/, "⚽"],
            [/news|info|nachrichten|noticias|أخبار/, "📰"],
            [/music|musique|musik|mtv|radio|أغاني/, "🎵"],
            [/doc|nature|discovery|history|science|geo/, "🌍"],
            [/movie|film|cinema|ciné|vod|box ?office|أفلام/, "🎬"],
            [/series|série|serie|show|drama|مسلسل/, "🎭"],
            [/relig|islam|quran|قرآن|church|christ|gospel/, "🕌"],
            [/comed|funny|humou?r/, "😂"],
            [/food|cook|cuisine/, "🍳"],
            [/4k|uhd|hd/, "✨"],
            [/adult|xxx|18\+/, "🔞"],
        ]
        for (const [re, emoji] of table) {
            if (re.test(n))
                return emoji
        }
        return ""
    }

    // "1 h 45 min" / "45 min"
    function duration(secs) {
        if (!secs)
            return ""
        const h = Math.floor(secs / 3600)
        const m = Math.round((secs % 3600) / 60)
        return h > 0 ? (m > 0 ? h + " h " + m + " min" : h + " h") : m + " min"
    }
}
