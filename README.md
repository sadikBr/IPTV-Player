# IPTV Player

A cross-platform IPTV player for desktop (Windows, Linux) and Android,
built with Qt 6 and QML. Bring your own provider: Xtream Codes login or an
M3U playlist.

See [PLAN.md](PLAN.md) for the roadmap and architecture.

## Status

**Version 0.3 (milestone 3): live TV with a TV guide.**

- Add providers with an **Xtream Codes login** or an **M3U playlist** (URL or file).
  Paste a full `get.php?username=…&password=…` link and the login fills in by itself.
- Browse live channels by category, search by name (accents ignored), mark favorites
- **TV guide:** what's on now (with progress) under every channel, now/next in the player,
  and a full guide grid (3 days, tap a program for details or to watch the channel).
  Xtream guides are downloaded automatically; for M3U the playlist's guide link
  (`url-tvg`) or the one you enter is used. `.xml.gz` guides are supported.
- Channel logos are cached on disk; channel lists refresh once a day and the guide
  every 12 hours (or use the refresh button)
- Player: previous/next channel, channel list panel, type a channel number to jump,
  stream info panel, full screen, volume, reconnect
- "Continue watching" on the home page; the last provider reopens on start-up
- Xtream account status and expiry date shown on the home page
- Android: screen stays on while playing; stream stops in the background and resumes
  when you return

Next: movies and series (milestone 4), then a design pass over the whole UI.

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
| 0–9 | Type a channel number to jump to it |
| L | Channel list |
| S | Add / remove favorite |
| Space / K | Play / pause |
| + / − or mouse wheel | Volume (↑ / ↓ when playing a single URL) |
| M | Mute |
| F / F11 / double-click | Full screen |
| ← / → | Seek 10 s (movies) |
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
  models/                  list models for QML: providers, categories, channels
  player/PlayerBackend.h   player interface (allows other engines later, e.g. mpv)
  player/QtMultimediaBackend.*  Qt Multimedia implementation
  player/PlayerController.*     QML-facing player (wake lock, background handling)
  platform/ScreenWakeLock.*     keep screen on (Android)
  qml/                     UI: Main, HomePage, AddSourcePage, LivePage, GuidePage, PlayerPage, ...
tests/                     unit tests and fixtures (sample M3U, Xtream responses)
.github/workflows/build.yml  CI for Linux, Windows, Android
```
