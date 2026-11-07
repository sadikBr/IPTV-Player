#pragma once

// Keeps the screen from turning off while video plays.
// Implemented on Android (FLAG_KEEP_SCREEN_ON); a no-op elsewhere for now.
namespace ScreenWakeLock {
void setActive(bool active);
}
