#!/usr/bin/env bash
# Adds IPTV Player to your app launcher (current user only, no sudo).
#
#   tools/install-desktop-entry.sh            # uses build/bin/IPTVPlayer
#   tools/install-desktop-entry.sh /path/to/IPTVPlayer
#   tools/install-desktop-entry.sh --remove   # takes it out again
set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
id="io.github.sadikbr.iptvplayer"
data="${XDG_DATA_HOME:-$HOME/.local/share}"
desktop="$data/applications/$id.desktop"

refresh() {
    command -v update-desktop-database >/dev/null && update-desktop-database -q "$data/applications" || true
    command -v gtk-update-icon-cache >/dev/null && gtk-update-icon-cache -qtf "$data/icons/hicolor" || true
}

if [ "${1:-}" = "--remove" ]; then
    rm -f "$desktop"
    find "$data/icons/hicolor" -name "$id.png" -delete 2>/dev/null || true
    refresh
    echo "Removed IPTV Player from the launcher."
    exit 0
fi

bin="${1:-$repo/build/bin/IPTVPlayer}"
if [ ! -x "$bin" ]; then
    echo "Can't find the app at $bin" >&2
    echo "Build it first (cmake --build build) or pass the path to IPTVPlayer." >&2
    exit 1
fi
bin="$(realpath "$bin")"

# Icons in every size
for size in 16 32 48 64 128 256 512; do
    install -Dm644 "$repo/assets/icon/hicolor/${size}x${size}/apps/$id.png" \
        "$data/icons/hicolor/${size}x${size}/apps/$id.png"
done

# Launcher entry pointing at the full path of the app (launchers often don't
# have ~/.local/bin in their PATH).
mkdir -p "$data/applications"
sed "s|^Exec=.*|Exec=\"$bin\"|" "$repo/assets/$id.desktop" > "$desktop"
chmod 644 "$desktop"

refresh
echo "Added IPTV Player to the launcher:"
echo "  $desktop"
echo "  -> $bin"
