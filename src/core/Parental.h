#pragma once

#include <QByteArray>
#include <QString>

namespace iptv::Parental {

// A PIN is 4 to 8 digits.
bool isValidPin(const QString &pin);

// Salted SHA-256 of the PIN (hex). The PIN itself is never stored.
QByteArray hashPin(const QString &pin, const QByteArray &salt);
QByteArray newSalt();

// Category names that look like adult content ("XXX", "Adult", "18+", ...),
// suggested for locking when a PIN is set.
bool looksAdult(const QString &categoryName);

} // namespace iptv::Parental
