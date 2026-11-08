#pragma once

// Android's back gesture / button (Android 13+): connects the activity's back
// callback (android/src/org/iptvplayer/IptvActivity.java) to the app's own
// back navigation. Does nothing on other platforms.
namespace AndroidBack {
void install();
}
