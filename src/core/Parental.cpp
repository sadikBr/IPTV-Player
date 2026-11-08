#include "Parental.h"

#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QRegularExpression>

namespace iptv::Parental {

bool isValidPin(const QString &pin)
{
    static const QRegularExpression re(QStringLiteral("^\\d{4,8}$"));
    return re.match(pin).hasMatch();
}

QByteArray hashPin(const QString &pin, const QByteArray &salt)
{
    return QCryptographicHash::hash(salt + pin.toUtf8(), QCryptographicHash::Sha256).toHex();
}

QByteArray newSalt()
{
    QByteArray salt(16, Qt::Uninitialized);
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32 *>(salt.data()), 4);
    return salt.toHex();
}

bool looksAdult(const QString &categoryName)
{
    // Whole words (or "18+" / "+18" not part of a longer number).
    static const QRegularExpression re(
            QStringLiteral("(?:^|[^\\p{L}0-9])(?:xxx|adults?|adultes?|porn\\w*|erotic\\w*|érotique|sexy?|playboy|hustler)(?:$|[^\\p{L}0-9])"
                           "|(?:^|[^0-9])(?:\\+18|18\\+)(?:$|[^0-9])"),
            QRegularExpression::CaseInsensitiveOption);
    return re.match(categoryName).hasMatch();
}

} // namespace iptv::Parental
