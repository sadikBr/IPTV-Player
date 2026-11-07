# IPTV Player

A cross-platform IPTV player for desktop (Windows, Linux) and Android,
built with Qt 6 and QML. Bring your own provider: Xtream Codes login or an
M3U playlist.

See [PLAN.md](PLAN.md) for the roadmap and architecture.

## Status

**Version 0.5 (design pass): live TV, TV guide, movies and series in a new look.**

- **New design:** dark cinematic theme with a violet-to-cyan accent and the Inter typeface;
  side navigation on desktop (Home, Live TV, Movies, Series, TV guide, Settings) and a
  bottom tab bar on phones; switch providers from the navigation
- **Home dashboard:** a hero for what to watch next, then rows for continue watching,
  favorite channels, favorite movies and recently added movies and series
- Add providers with an **Xtream Codes login** or an **M3U playlist** (URL or file).
  Paste a full `get.php?username=…&password=…` link and the login fills in by itself.
- **Live TV:** categories (side panel or chips), search (accents ignored), favorites,
  channel logos and what's on now
- **TV guide:** now/next under every channel and in the player, and a 3-day guide grid.
  Xtream guides download automatically; for M3U the playlist's guide link or the one you
  enter is used (`.xml.gz` supported). **Time shift per provider** in Settings for guides
  with wrong time zones
- **Movies and series:** poster grids with ratings, details pages, seasons and episodes,
  resume where you stopped, next episode after a 10 s countdown, favorites
- **Settings:** manage providers (use, update, guide time shift, remove), User-Agent,
  play a single stream URL
- Player: seek bar, audio and subtitle tracks, previous/next channel or episode, channel
  list, channel number entry, stream info, full screen
- Lists refresh once a day and the guide every 12 hours (or use the refresh buttons)
- Android: screen stays on while playing; back button goes back one screen

### Troubleshooting

- **"The server closed the connection" / "Connection reset by peer":** many providers only
  answer clients that look like a known player. The app identifies itself as VLC
  (`VLC/3.0.20 LibVLC/3.0.20`). To use a different one, go to **Settings → Network**.
- **Guide times are off by a fixed amount:** Settings → Providers → clock button, and
  shift that provider's guide by the difference.
- **"Network unreachable":** the network or DNS hands out an address that can't be
  reached (for example a `64:ff9b::` IPv6 address on a network without IPv6). Check the
  connection, VPN and DNS settings.
- **Test whether your provider answers:**
  `curl -sS -A "VLC/3.0.20 LibVLC/3.0.20" -o /dev/null -w "%{http_code}\n" "http://SERVER:PORT/player_api.php"`
  should print `200`.

### Where your data is stored

Providers, channel lists and favorites are kept in a SQLite database in the app's
private data folder (Linux: `~/.local/share/IPTVPlayer/IPTV Player/`, Windows:
`%APPDATA%\IPTVPlayer\IPTV Player\`, Android: the app's private storage). Your
provider password is stored there as well; it is not sent anywhere except to your
provider.

## Download

Every push to `main` builds the app on GitHub Actions. Open the
[Actions tab](../../actions/workflows/build.yml), pick the latest successful
run and download from **Artifacts**:

| Artifact | Platform |
|---|---|
| `IPTVPlayer-windows-x64` | Windows 10/11 — unzip and run `IPTVPlayer.exe` |
| `IPTVPlayer-android` | Android 9+ (arm64 and 32-bit ARM) — install the APK ("allow unknown sources") |

Pushing a tag like `v0.1.0` also publishes these files as a GitHub Release,
which is the easiest link to share with others.

### Android signing key (recommended)

Without a fixed signing key, each Android build is signed with a new temporary
key, so you must uninstall the old version before installing a new one. To keep
updates working, create a key once and add it as repository secrets:

```bash
keytool -genkeypair -v -keystore release.keystore -alias iptvplayer \
  -keyalg RSA -keysize 2048 -validity 10000
base64 -w0 release.keystore > release.keystore.b64
```

Then in **GitHub → Settings → Secrets and variables → Actions**, add:

| Secret | Value |
|---|---|
| `ANDROID_KEYSTORE_BASE64` | contents of `release.keystore.b64` |
| `ANDROID_KEYSTORE_PASSWORD` | the password you chose |
| `ANDROID_KEY_ALIAS` | `iptvplayer` |

Keep `release.keystore` safe and out of the repository.

## Keyboard shortcuts

**Player**

| Key | Action |
|---|---|
| ↑ / ↓, Page Up / Page Down | Previous / next channel |
| N | Next episode |
| 0–9 | Type a channel number to jump to it |
| L | Channel list |
| S | Add / remove favorite |
| Space / K | Play / pause |
| + / − or mouse wheel | Volume (↑ / ↓ when playing a single URL) |
| M | Mute |
| F / F11 / double-click | Full screen |
| ← / → | Seek 10 s (movies and episodes) |
| I | Stream info |
| Esc / Backspace | Close panel, exit full screen, back |

**Channel list:** ↑ / ↓ to move, Enter to play, S to favorite, Ctrl+F to search.

## Building from source

Requirements: **Qt 6.8 or newer** (CI uses 6.10) with Qt Quick, Quick Controls 2, Multimedia and SVG; CMake ≥ 3.21,
Ninja, a C++17 compiler and zlib (downloaded automatically if missing, e.g. on Windows). Qt 6.4+ also builds, but older Qt Multimedia versions are much
less reliable for streaming.

### Arch Linux

```bash
sudo pacman -S --needed base-devel cmake ninja \
  qt6-base qt6-declarative qt6-multimedia qt6-multimedia-ffmpeg qt6-svg qt6-wayland
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bin/IPTVPlayer
```

`qt6-multimedia-ffmpeg` is required: it is the video engine. For hardware decoding,
install `intel-media-driver` (Intel) or `libva-mesa-driver` (AMD).

### Other systems (Qt from the online installer)

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/Qt/6.10.1/gcc_64
cmake --build build
./build/bin/IPTVPlayer
ctest --test-dir build --output-on-failure   # unit tests + off-screen UI smoke test
```

Or open `CMakeLists.txt` in Qt Creator and press Run. For Android, pick an Android kit in
Qt Creator (it needs JDK 17, the Android SDK and NDK r27c).

## Project layout

```
src/
  main.cpp                 app entry point (+ --smoke-test mode)
  core/                    no UI: M3U/XMLTV parsers, Xtream API, SQLite storage (unit tested)
  app/AppSettings.*        persisted settings, exposed to QML
  app/SourceManager.*      add/refresh/remove providers (download, parse, save)
  app/EpgManager.*         download and import TV guides; now/next queries
  app/VodManager.*         movie/series details and episodes, watch progress
  models/                  list models for QML: providers, categories, channels
  player/PlayerBackend.h   player interface (allows other engines later, e.g. mpv)
  player/QtMultimediaBackend.*  Qt Multimedia implementation
  player/PlayerController.*     QML-facing player (wake lock, background handling)
  platform/ScreenWakeLock.*     keep screen on (Android)
  qml/                     UI. Main.qml is the app shell (navigation); pages: HomePage,
                           LivePage, GuidePage, VodPage, MovieDetailsPage, SeriesDetailsPage,
                           SettingsPage, AddSourcePage, PlayerPage. AppTheme.qml holds the
                           design tokens; AppButton, Chip, PosterCard, ... are the components
  qml/fonts/               Inter typeface (SIL Open Font License, see Inter-LICENSE.txt)
tests/                     unit tests and fixtures (sample M3U, Xtream responses)
.github/workflows/build.yml  CI for Linux, Windows, Android
```
