# Changelog

The section for a version is used as its GitHub release notes and is shown in
the app's update dialog. Keep it short and written for people using the app.
Changes not released yet go under "Unreleased"; to release, rename it to the
new version (e.g. "## v1.0.0") and set the same VERSION in CMakeLists.txt.

## v1.0.1

- **A more colourful, playful look:** every channel, movie and category gets a colour of
  its own (tinted cards, glowing initials, coloured chips with little category icons);
  what's selected glows in an orange-to-pink gradient; cards and buttons bounce when you
  tap them. On phones: a floating bottom bar whose highlight slides to the tab you pick,
  and a greeting on Home.
- **Android back gesture** goes back one screen (closes a dialog, leaves the player…) and
  only closes the app on Home.
- **Phones:** buttons no longer run off the screen. Page buttons move to a row of their
  own when they don't fit next to the title; the player's controls use two rows; the
  bottom bar has five tabs (Settings is the gear on Home and in the provider menu).
- Channel names lose decorative symbols the phone can't draw (no more "NO GLYPH" boxes).
- In the player, program times are no longer shown backwards next to Arabic titles.

**Android note:** this build is signed with a temporary key, like v1.0.0. To install it,
first export a backup (Settings → Backup), uninstall the old version, install this one
and import the backup.

## v1.0.0

The first release: watch live TV, the TV guide, movies and series from your IPTV provider
(Xtream Codes or M3U) on Windows, Linux and Android. Highlights:

- **Radio:** tens of thousands of internet radio stations from radio-browser.info: popular,
  trending, your country, genres, search, favorites and recently played. A mini player at
  the bottom keeps playing while you browse (and in the background on Android); starting a
  video stops it. Works without any IPTV provider.
- **Free channels:** no subscription needed. Add provider → Free channels gives you
  thousands of free-to-air channels from the open iptv-org project: everything (grouped by
  category), one country, or one category.
- **Home banner** shows well-rated, recent movies and series (a new selection every day)
  instead of the provider's latest uploads, without "EN|"-style prefixes in the title.
- **Backup / move to another device:** Settings → Backup exports your providers, favorites,
  hidden and locked categories, what you were watching, radio stations and settings to one
  file; import it on another device (it merges, nothing is duplicated).
- **Search everything:** one search for live channels, movies, series and radio stations
  (Search in the side bar, the search button on Home, or Ctrl+F).
- **Sleep timer:** stop TV or radio after 15–90 minutes or at the end of the program; the
  sound fades out (moon button in the player and the radio bar).
- **Faster TV guide:** the guide updates in seconds instead of minutes on large providers;
  only channels you have (and haven't hidden) are kept, with past programs only as far
  back as the channel's catch-up archive.
- **Faster navigation:** Home, Live TV, Movies, Series and Radio are kept once opened, so
  going back to them is instant and keeps your place; long lists load as you scroll.
- **Android:** pictures and https streams now load (the app ships its own OpenSSL).
- **Sorting:** sort channels, movies and series by name, rating, year, channel number or
  recently added (sort button at the top; remembered per section).
- **Missing artwork:** movies and series without a picture (or whose picture doesn't load)
  get a coloured card with their title instead of an empty tile, and the Home banner only
  shows titles whose artwork loads.
- **Parental control:** set a PIN (Settings → Parental control) and lock categories with the
  lock button in Choose categories. Locked categories, and Settings, stay hidden until the
  PIN is entered; the app locks again every time it starts. When you set the PIN it offers
  to lock categories that look like adult content.
- The subtitles button is always shown for movies and episodes, and says when a video has
  no subtitles.
- **Picture-in-picture (Android):** keep watching in a small floating window while you use
  other apps: the button in the player, or (Android 12+) just go to the home screen while
  something plays.
- **Catch-up TV:** watch programs that already aired, on channels where your provider keeps
  an archive (marked with ↺ in the TV guide). Open a past program in the guide and press
  **Watch**, or **Watch from the start** for one that is on now. It plays like a
  recording: pause and seek. Works with Xtream providers and with M3U playlists that
  describe their archive (`catchup` / `catchup-source`).
- The TV guide now keeps a week of past programs and works with the TV remote: arrows move
  between programs, OK opens one.
- **Automatic updates:** the app checks for new versions and installs them for you
  (Windows installer, Linux AppImage and Android). Settings → Updates to check now or
  turn automatic checks off.
- **Keyboard and TV remote:** the arrow keys move between everything on screen (side bar,
  categories, rows, buttons), Enter / OK opens, Escape / Back goes back.
- **Reconnects by itself** when a channel or movie drops, freezes or keeps buffering.
- **Playback settings:** turn automatic reconnecting off, start channels faster (less
  buffering), or switch hardware decoding off if the picture looks wrong.
- **Stream format per provider:** MPEG-TS or HLS, changeable any time (Settings → the
  gear button of a provider). Try the other one if channels don't start.
- **Phones in landscape** get a slim icon side bar instead of the bottom bar, so there is
  more room for the picture and lists; **tablets** show more posters per row.
- **New app icon**, and a dark launch screen on Android.
- **Windows installer** and **Linux AppImage**.
- Series: all episode cards are the same size, and episode titles leave out the
  repeated show name.

**Android note:** this build is signed with a temporary key. To install a later version you
may need to uninstall this one first (export a backup in Settings beforehand to keep your
providers and favorites).
