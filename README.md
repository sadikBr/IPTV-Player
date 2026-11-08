# IPTV Player

A cross-platform IPTV player for desktop (Windows, Linux) and Android,
built with Qt 6 and QML. Bring your own provider: Xtream Codes login or an
M3U playlist.

See [PLAN.md](PLAN.md) for the roadmap and architecture.

## Features (version 1.0)

**Watching**
- **Free channels:** no subscription needed: thousands of free-to-air channels from the open
  [iptv-org](https://github.com/iptv-org/iptv) project, all of them or by country or category
  (Add provider → Free channels)
- **Providers:** Xtream Codes login or M3U playlist (URL or file); paste a full
  `get.php?username=…&password=…` link and the login fills in by itself. Several providers,
  switch from the side bar
- **Live TV:** channel tiles or a compact list with what's on now; categories, search
  (accents ignored), favorites, channel numbers
- **TV guide:** now/next everywhere and a guide grid; Xtream guides download by themselves,
  M3U uses the playlist's guide link or yours (`.xml.gz` too); time shift per provider for
  guides in the wrong time zone
- **Catch-up TV:** on channels with an archive (↺ in the guide) watch past programs, or the
  current one from its start. Xtream (`tv_archive`) and M3U (`catchup`, `catchup-days`,
  `catchup-source`; default, append, shift, flussonic, xc)
- **Movies and series:** poster grids, details pages, seasons and episodes, resume where you
  stopped, next episode after a countdown, favorites
- **Sorting:** by name, rating, year, channel number or recently added (remembered per
  section); titles without artwork get a coloured card with their name
- **Home:** a hero with backdrops (what you're watching, what's new), then rows for continue
  watching, your channels, your movies, new movies and series

**Player**
- Seek bar, audio tracks and the subtitles that come with a movie or episode, previous/next
  channel or episode, channel list, channel number entry, stream info, full screen
- **Reconnects by itself** when a stream drops, freezes or keeps buffering (up to 5 tries);
  live TV picks up at "now", movies where they were
- **Picture-in-picture** on Android (button, or Android 12+ when you leave the app)

**Your way**
- **Choose categories:** hide what you don't watch, whole languages or regions at once
  (`[EN]`, `FR |`, ...) or one by one
- **Parental control:** a PIN hides the categories you lock, and Settings, until it is
  entered; locked again at every start
- **Playback settings:** automatic reconnect, start channels faster, hardware decoding; live
  stream format per provider (MPEG-TS or HLS)
- **Keyboard and TV remote:** arrows move between everything on screen, Enter/OK opens,
  Escape/Back goes back
- Cinematic dark design that adapts to desktops, tablets, and phones in portrait and
  landscape

**Keeping up to date**
- **Automatic updates** from GitHub Releases, checked against the release's SHA-256: the
  Windows installer updates in place, the AppImage replaces itself, Android opens the
  system installer. Settings → Updates
- Lists refresh once a day and the guide every 12 hours (or use the refresh buttons)

## Troubleshooting

- **"The server closed the connection" / "Connection reset by peer":** many providers only
  answer clients that look like a known player. The app identifies itself as VLC
  (`VLC/3.0.20 LibVLC/3.0.20`). To use a different one, go to **Settings → Network**.
- **Guide times are off by a fixed amount:** Settings → Providers → gear button, and
  shift that provider's guide by the difference.
- **Channels don't start or keep stopping:** Settings → Providers → gear button, switch
  the live stream format (MPEG-TS ↔ HLS).
- **Green or garbled picture:** Settings → Playback → turn off Hardware decoding and
  restart the app.
- **"Network unreachable":** the network or DNS hands out an address that can't be
  reached (for example a `64:ff9b::` IPv6 address on a network without IPv6). Check the
  connection, VPN and DNS settings.
- **Test whether your provider answers:**
  `curl -sS -A "VLC/3.0.20 LibVLC/3.0.20" -o /dev/null -w "%{http_code}\n" "http://SERVER:PORT/player_api.php"`
  should print `200`.

## Where your data is stored

Providers, channel lists and favorites are kept in a SQLite database in the app's
private data folder (Linux: `~/.local/share/IPTVPlayer/IPTV Player/`, Windows:
`%APPDATA%\IPTVPlayer\IPTV Player\`, Android: the app's private storage). Your
provider password is stored there as well; it is not sent anywhere except to your
provider.

## Download

Get the files from the **[latest release](../../releases/latest)**. Once installed, the app
updates itself.

| File | Platform |
|---|---|
| `IPTVPlayer-setup-x64.exe` | Windows 10/11 — installs it (Start menu, uninstaller, updates in place; no admin rights needed) |
| `IPTVPlayer-windows-x64.zip` | Windows, portable — unzip and run `IPTVPlayer.exe` (no automatic updates) |
| `IPTVPlayer-x86_64.AppImage` | Any Linux distribution — `chmod +x IPTVPlayer-x86_64.AppImage` and run it |
| `IPTVPlayer-android.apk` | Android 9+ (phones, tablets, TV boxes; arm64 and 32-bit ARM) — install it ("allow unknown sources") |

Test builds of every push to `main` are on the [Actions tab](../../actions/workflows/build.yml)
(open a successful run, **Artifacts**).

Windows may show "Windows protected your PC" because the installer isn't signed with a
paid certificate: click **More info → Run anyway**.

### Releases and updates

Releases are what the app's automatic updates install. To publish one:

1. Set `VERSION` in `CMakeLists.txt` (e.g. `1.1.0`).
2. In `CHANGELOG.md`, rename `## Unreleased` to `## v1.1.0`: it becomes the release notes
   and the "What's new" text in the update dialog.
3. Commit and push to `main`.

When CI passes, it creates the tag and publishes the GitHub Release with the Windows
installer and zip, the AppImage and the APK (once per version; no tag to push). Within 12 hours every installed copy offers the update. The
[latest release](../../releases/latest) is also the easiest link to share.

### Android signing key (recommended)

Every build has a higher version number, so a new APK installs over the old one,
but only when both are signed with the same key. **Automatic updates on Android need this
key:** without it, Android refuses to install the update ("App not installed"). Without a fixed signing key, each
Android build is signed with a new temporary key, so you must uninstall the old version
before installing a new one. To keep
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

**Everywhere (keyboard and TV remote):** the arrow keys move between the side bar,
categories, rows, cards and buttons (the focused one has an orange outline); Enter / OK
opens or plays; Escape / Back goes back one screen.

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

To add it to your app launcher (with its icon), run once:

```bash
tools/install-desktop-entry.sh             # --remove takes it out again
```

The entry points at `build/bin/IPTVPlayer`, so after pulling and rebuilding the launcher
starts the new version.

If the build stops with `qmlcachegen` crashing (a bug in some Qt versions), configure
with `-DIPTV_QML_AOT=OFF`; QML is then compiled when the app starts instead.

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
assets/                    app icon (all sizes), Linux .desktop file, Windows resource
android/                   Android manifest, launcher icons, launch screen
installer/windows/         Windows installer (Inno Setup)
tools/make_icons.py        draws the icon and writes every size (needs Pillow)
tools/install-desktop-entry.sh  adds the app to the Linux app launcher
.github/workflows/build.yml  CI for Linux (+ AppImage), Windows (+ installer), Android
```
