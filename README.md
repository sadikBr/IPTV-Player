# IPTV Player

A cross-platform IPTV player for desktop (Windows, Linux) and Android,
built with Qt 6 and QML. Bring your own provider: Xtream Codes login or an
M3U playlist.

See [PLAN.md](PLAN.md) for the roadmap and architecture.

## Status

**Version 0.1 (milestones 0–1):** paste a stream URL and play it.

- Plays HLS (`.m3u8`) and MPEG-TS (`.ts`) streams, plus anything else FFmpeg can open
- Play/pause, reconnect, volume, mute, full screen, auto-hiding controls
- Stream info panel (codec, resolution, frame rate) — press **I** or tap ⓘ
- Recent URLs remembered
- Android: screen stays on while playing; stream stops in the background and resumes when you return

Adding a provider and browsing channels comes next (milestone 2).

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

## Keyboard shortcuts (player)

| Key | Action |
|---|---|
| Space / K | Play / pause |
| F / F11 / double-click | Full screen |
| M | Mute |
| ↑ / ↓ | Volume |
| ← / → | Seek 10 s (movies) |
| I | Stream info |
| Esc / Backspace | Exit full screen / back |

## Building from source

Requirements: **Qt 6.8 or newer** (CI uses 6.10) with Qt Quick, Quick Controls 2, Multimedia and SVG; CMake ≥ 3.21,
Ninja and a C++17 compiler. Qt 6.4+ also builds, but older Qt Multimedia versions are much
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
ctest --test-dir build --output-on-failure   # off-screen UI smoke test
```

Or open `CMakeLists.txt` in Qt Creator and press Run. For Android, pick an Android kit in
Qt Creator (it needs JDK 17, the Android SDK and NDK r27c).

## Project layout

```
src/
  main.cpp                 app entry point (+ --smoke-test mode)
  app/AppSettings.*        persisted settings, exposed to QML
  player/PlayerBackend.h   player interface (allows other engines later, e.g. mpv)
  player/QtMultimediaBackend.*  Qt Multimedia implementation
  player/PlayerController.*     QML-facing player (wake lock, background handling)
  platform/ScreenWakeLock.*     keep screen on (Android)
  qml/                     UI: Main, HomePage, PlayerPage, PlayerControls, ...
.github/workflows/build.yml  CI for Linux, Windows, Android
```
