#include "DisplayText.h"

#include <QFont>
#include <QGlyphRun>
#include <QGuiApplication>
#include <QHash>
#include <QTextLayout>

namespace {

// Ordinary text needs no check: letters, digits, punctuation and spaces of
// the common scripts are covered by the system fonts.
bool obviouslyFine(char32_t c)
{
    if (c < 0x2000)
        return true;
    const auto category = QChar::category(c);
    switch (category) {
    case QChar::Letter_Uppercase:
    case QChar::Letter_Lowercase:
    case QChar::Letter_Titlecase:
    case QChar::Letter_Other:
    case QChar::Number_DecimalDigit:
        return true;
    default:
        return false;
    }
}

// Joiners and variation selectors have no glyph of their own: keep them.
bool invisibleHelper(char32_t c)
{
    return c == 0x200C || c == 0x200D || (c >= 0xFE00 && c <= 0xFE0F)
            || (c >= 0xE0100 && c <= 0xE01EF);
}

bool drawable(char32_t c)
{
    static QHash<char32_t, bool> cache;
    const auto it = cache.constFind(c);
    if (it != cache.constEnd())
        return *it;

    bool ok = false;
    const QChar::Category category = QChar::category(c);
    if (category != QChar::Other_PrivateUse && category != QChar::Other_NotAssigned
        && category != QChar::Other_Surrogate && category != QChar::Other_Control) {
        // Lay the character out like the UI would (font fallback included) and
        // see whether a real glyph came out (index 0 is the "missing" glyph).
        QTextLayout layout(QString::fromUcs4(&c, 1), QGuiApplication::font());
        layout.beginLayout();
        QTextLine line = layout.createLine();
        if (line.isValid())
            line.setLineWidth(1000);
        layout.endLayout();
        const auto runs = layout.glyphRuns();
        for (const QGlyphRun &run : runs) {
            for (quint32 glyph : run.glyphIndexes()) {
                if (glyph != 0)
                    ok = true;
            }
        }
    }
    cache.insert(c, ok);
    return ok;
}

} // namespace

QString DisplayText::visibleText(const QString &text)
{
    bool needsCheck = false;
    for (const char32_t c : text.toUcs4()) {
        if (!obviouslyFine(c)) {
            needsCheck = true;
            break;
        }
    }
    if (!needsCheck || !qobject_cast<QGuiApplication *>(QCoreApplication::instance()))
        return text;

    QString result;
    result.reserve(text.size());
    bool dropped = false;
    for (const char32_t c : text.toUcs4()) {
        if (obviouslyFine(c) || invisibleHelper(c) || drawable(c))
            result += QString::fromUcs4(&c, 1);
        else
            dropped = true;
    }
    return dropped ? result.simplified() : text;
}
