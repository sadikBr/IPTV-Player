# Changelog

The section for a version is used as its GitHub release notes and is shown in
the app's update dialog. Keep it short and written for people using the app.
Changes not released yet go under "Unreleased"; to release, rename it to the
new version (e.g. "## v1.0.0") and set the same VERSION in CMakeLists.txt.

## Unreleased

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
