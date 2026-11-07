# IPTV Player — Project Plan

A cross-platform app (Windows, macOS, Linux, Android) to connect to an IPTV
provider and watch live TV, movies (VOD) and series, with an electronic program
guide (EPG).

---

## 1. Goals

**Must have (v1.0)**
- Add one or more IPTV sources:
  - **Xtream Codes API** (server URL + username + password) — most providers offer this
  - **M3U / M3U8 playlist** (URL or local file), optionally with an **XMLTV** EPG URL
- Browse live channels by category, search, and mark favorites
- Play live streams (HLS `.m3u8`, MPEG-TS `.ts`), with channel up/down and quick zapping
- Show now/next program info and a basic EPG grid
- Browse and play VOD movies and series (seasons → episodes)
- Resume playback for VOD, remember the last watched channel
- Works on desktop (mouse/keyboard) and mobile (touch), with a responsive UI

**Nice to have (later)**
- Android TV / remote D-pad navigation
- Catch-up / timeshift (if the provider supports it)
- Picture-in-picture, Chromecast
- Multiple profiles, parental PIN
- Recording to disk (desktop only)
- Stalker/MAG portal support (MAC-address based)

**Out of scope**
- Providing any content or playlists. The app is a player only; the user brings their own provider.

---

## 2. Technology choices

| Area | Choice | Why |
|---|---|---|
| Framework | **Qt 6.8 LTS** | One codebase for desktop + mobile, long-term support |
| UI | **Qt Quick / QML** (Qt Quick Controls 2, Material style) | Touch-friendly, GPU-accelerated, adapts to phones and desktops. Qt Widgets does not work well on mobile. |
| Core logic | **C++17/20** | Networking, parsing, database, models exposed to QML |
| Video playback | **Qt Multimedia** (FFmpeg backend) behind a `PlayerBackend` interface | Ships with Qt and works on every target platform. Optional **libmpv** backend on desktop later for wider codec/stream support. |
| Networking | `QNetworkAccessManager` | Async HTTP, redirects, timeouts |
| JSON | `QJsonDocument` | Xtream API responses |
| XML (EPG) | `QXmlStreamReader` (streaming) | XMLTV files can be 50–200 MB, so they must not be loaded into memory all at once |
| Storage | **SQLite** via Qt SQL | Channels, EPG, favorites, watch history |
| Secrets | **QtKeychain** | Keep provider passwords in the OS keychain, not in plain text |
| Build | **CMake** + `qt_add_qml_module` | The standard for Qt 6, needed for Android packaging |
| CI | **GitHub Actions** + `jurplel/install-qt-action` | Build all platforms on each push |
| Tests | **Qt Test** | Parsers and API client tested with recorded fixtures |

### Why this playback approach
IPTV streams vary a lot (H.264/H.265, AAC/AC3, HLS/TS, sometimes broken
timestamps). Qt Multimedia with FFmpeg covers most of them on every platform.
Putting it behind an interface lets us add an mpv backend later where
compatibility matters most, without touching the UI.

### Licensing note
Qt is used under LGPLv3 with dynamic linking (the default on desktop and
Android), which is fine for personal use and sharing with friends. iOS is out
of scope; it would need static linking, which has extra LGPL obligations.

---

## 3. Architecture

```
┌──────────────────────── QML UI ─────────────────────────┐
│ SourcesPage  LivePage  EpgGrid  VodPage  SeriesPage      │
│ PlayerPage (overlay: channel list, now/next, controls)   │
└───────────────▲──────────────────────────▲───────────────┘
                │ Q_PROPERTY / models      │ commands
┌───────────────┴──────────────────────────┴───────────────┐
│ ViewModels / QAbstractListModels (C++)                    │
│ CategoryModel  ChannelModel  EpgModel  VodModel  ...      │
└───────────────▲──────────────────────────▲───────────────┘
┌───────────────┴───────────┐  ┌───────────┴───────────────┐
│ Repository (SQLite cache) │  │ PlayerController          │
│ sync, favorites, history  │  │  └─ PlayerBackend (iface) │
└───────────────▲───────────┘  │     ├─ QtMultimediaBackend│
┌───────────────┴───────────┐  │     └─ MpvBackend (later) │
│ Providers (iface)         │  └───────────────────────────┘
│  ├─ XtreamProvider        │
│  ├─ M3uProvider           │
│  └─ XmltvEpgLoader        │
└───────────────────────────┘
```

**Key ideas**
- **`Provider` interface**: every source type returns the same domain objects
  (`Category`, `Channel`, `VodItem`, `Series`, `Episode`, `EpgProgram`). The UI
  never knows whether the data came from Xtream or M3U.
- **Cache first**: sync from the provider into SQLite on startup or manual
  refresh; the UI always reads from the local DB. This keeps scrolling fast with
  10k+ channels, and the app works while a refresh is running.
- **Background work**: playlist/EPG parsing runs on a worker thread
  (`QtConcurrent` / `QThread`), with progress reported to the UI.

### Proposed repository layout
```
IPTV-Player/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── core/          # domain types, Provider interface
│   ├── providers/     # xtream/, m3u/, xmltv/
│   ├── data/          # Database, Repository, migrations
│   ├── models/        # QAbstractListModel subclasses for QML
│   ├── player/        # PlayerController, backends
│   └── platform/      # keychain, screen wake-lock, Android bits
├── qml/
│   ├── Main.qml
│   ├── pages/
│   └── components/
├── resources/         # icons, translations
├── android/           # AndroidManifest.xml, gradle overrides
├── ios/               # Info.plist
├── tests/
│   └── fixtures/      # sample M3U, Xtream JSON, XMLTV
└── .github/workflows/
```

---

## 4. IPTV protocol reference

### Xtream Codes API
Base: `http(s)://HOST:PORT/player_api.php?username=U&password=P`

| Action (`&action=`) | Returns |
|---|---|
| *(none)* | account + server info (status, expiry, max connections) |
| `get_live_categories` | live categories |
| `get_live_streams[&category_id=X]` | live channels |
| `get_vod_categories` / `get_vod_streams` | movie categories / movies |
| `get_vod_info&vod_id=X` | movie details |
| `get_series_categories` / `get_series` | series categories / series |
| `get_series_info&series_id=X` | seasons + episodes |
| `get_short_epg&stream_id=X&limit=N` | now/next for one channel |

Stream URLs:
- Live: `http://HOST:PORT/live/U/P/{stream_id}.{ts|m3u8}`
- Movie: `http://HOST:PORT/movie/U/P/{stream_id}.{container_extension}`
- Episode: `http://HOST:PORT/series/U/P/{episode_id}.{container_extension}`
- Full EPG (XMLTV): `http://HOST:PORT/xmltv.php?username=U&password=P`

### M3U playlist
```
#EXTM3U url-tvg="http://.../epg.xml"
#EXTINF:-1 tvg-id="bbc1.uk" tvg-name="BBC One" tvg-logo="http://..." group-title="UK",BBC One HD
http://example.com/stream/123.ts
```
The parser must handle: attributes in any order, missing attributes,
`#EXTVLCOPT` / `#KODIPROP` lines, CRLF line endings, BOM, and very large files.

### XMLTV EPG
`<channel id="...">` and `<programme start="20261107180000 +0000" stop="..." channel="...">`,
matched to channels by `tvg-id` (or Xtream `epg_channel_id`). Parse in a
streaming way, keep only a window (for example −1 day to +3 days), and
store it in SQLite with an index on `(channel_id, start)`.

---

## 5. Data model (SQLite)

```
sources(id, name, type, url, username, epg_url, last_sync)   -- password in keychain
categories(id, source_id, kind[live|vod|series], ext_id, name, sort)
channels(id, source_id, category_id, ext_id, name, logo, epg_id, stream_url, num)
vod(id, source_id, category_id, ext_id, name, poster, ext, rating, plot)
series(id, source_id, category_id, ext_id, name, poster, plot)
episodes(id, series_id, season, number, ext_id, title, ext)
epg(channel_epg_id, source_id, start, stop, title, description)
favorites(item_kind, item_id)
history(item_kind, item_id, position_ms, duration_ms, watched_at)
```

---

## 6. Milestones

Each phase ends with something that runs and can be tested on a real device.

### Phase 0 — Project setup (≈ 2–3 days)
- [x] CMake + Qt 6.8 skeleton, `Main.qml` with a window
- [ ] Builds and runs on Linux/Windows/macOS **and** an Android device (CI builds; needs a check on your devices)
- [x] GitHub Actions: desktop builds + Android APK artifact
- [x] clang-format, `.gitignore`, README with build instructions

### Phase 1 — Play a stream (≈ 3–4 days)
- [x] `PlayerBackend` interface + `QtMultimediaBackend`
- [x] Player page: paste a URL, play/pause, volume, fullscreen, buffering indicator, error message
- [ ] Test with real HLS and TS streams from your provider on desktop and phone
- [x] Keep the screen awake while playing (mobile), handle app pause/resume

### Phase 2 — Sources & live TV (≈ 1–2 weeks)
- [ ] `M3uProvider` (parser + tests with fixtures)
- [ ] `XtreamProvider` (API client + account info/expiry + tests)
- [ ] "Add source" screen; password stored with QtKeychain
- [ ] SQLite database + migrations + Repository; sync with a progress bar
- [ ] Live page: categories → channels (with logos, lazy-loaded and cached), search
- [ ] Tap a channel to play; channel up/down and a channel list overlay inside the player
- [ ] Favorites and last-watched channel

**→ First usable version: you can watch live TV.**

### Phase 3 — EPG (≈ 1 week)
- [ ] Streaming XMLTV loader on a background thread, with a time window
- [ ] Now/next under each channel and in the player overlay
- [ ] EPG grid (channels × time) with virtualization so it stays smooth
- [ ] Xtream `get_short_epg` fallback when there is no XMLTV

### Phase 4 — VOD & series (≈ 1 week)
- [ ] Movies: categories → poster grid → detail page → play
- [ ] Series: series → seasons → episodes → play, plus "next episode"
- [ ] Seek bar, audio/subtitle track selection
- [ ] Resume position / "Continue watching" row

### Phase 5 — Polish & platform work (≈ 1–2 weeks)
- [ ] Responsive layouts: phone portrait, phone landscape, tablet, desktop
- [ ] Keyboard shortcuts (desktop) and D-pad focus navigation (Android TV)
- [ ] Settings: preferred stream format (TS/HLS), buffer size, user agent, theme, language
- [ ] Network robustness: timeouts, retries, automatic reconnect on stream stall
- [ ] App icons, splash screen, translations
- [ ] Packaging: Windows installer (NSIS or Qt IFW), macOS `.dmg`, Linux AppImage/Flatpak, Android signed APK/AAB

### Phase 6 — Optional extras
- [ ] `MpvBackend` on desktop for better codec/stream support
- [ ] Catch-up/timeshift (Xtream `tv_archive` channels)
- [ ] Picture-in-picture (Android), Chromecast
- [ ] Multiple profiles, parental PIN, Stalker portal

**Rough total for v1.0 (Phases 0–5):** 6–9 weeks part-time.

---

## 7. Risks and how we handle them

| Risk | Mitigation |
|---|---|
| Some streams don't play in Qt Multimedia (codec, broken TS) | Backend interface; mpv backend; try `.m3u8` vs `.ts` as a setting; custom User-Agent |
| Huge playlists/EPG are slow or use too much memory | Streaming parsers, background threads, SQLite cache, model virtualization in QML |
| Providers block unknown clients or limit connections | Configurable User-Agent; always stop the previous stream before starting a new one |
| Android build setup is complicated | Set up the Android build in Phase 0, not at the end |
| Plain-HTTP providers blocked on mobile | Not an issue for Qt: its networking and FFmpeg use native sockets, which Android’s cleartext policy does not cover. Revisit if a Java-based component is added |

---

## 8. Testing strategy
- **Unit tests** (Qt Test): M3U parser, Xtream JSON mapping, XMLTV parser, repository queries — using files in `tests/fixtures/` (sanitized, no real credentials).
- **Mock server**: a small local HTTP server that returns fixture JSON, for testing the Xtream client without a real account.
- **Manual device matrix** before each milestone: one desktop OS, one Android phone, optionally one Android TV box.

---

## 9. Decisions

| Question | Answer | Consequence |
|---|---|---|
| Source type | Xtream login **and** M3U | Both providers are in scope for Phase 2 |
| Target devices | **Desktop + Android** | iOS dropped. Android builds for arm64 and 32-bit ARM so cheap TV boxes work too |
| Distribution | Personal, shared with friends | No store publishing: GitHub Releases with a Windows zip, macOS dmg and a signed Android APK |
| Stream formats | Unknown | The player has a stream info panel (codec, resolution) to find out; testing in Phase 1 tells whether an mpv backend is needed |

---

## 10. Progress

- **Phase 0:** done — CMake/QML skeleton, CI for Linux/Windows/macOS/Android, release on `v*` tags
- **Phase 1:** done — URL player with controls, stream info, wake lock, background handling.
  **To do (you):** test real streams from your provider on PC and phone and note what fails
- **Next:** Phase 2 — sources and live TV
