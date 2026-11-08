#pragma once

#include <QString>

// Names from providers sometimes contain decorative symbols that no font on
// the device can draw (shown as empty or "NO GLYPH" boxes). visibleText()
// drops those characters, using the fonts actually installed (with fallbacks).
namespace DisplayText {
QString visibleText(const QString &text);
}
