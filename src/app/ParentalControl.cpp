#include "ParentalControl.h"

#include "Database.h"
#include "Parental.h"
#include "SourceStore.h"

#include <QSqlQuery>

using namespace iptv;

namespace {
const auto kHashKey = QStringLiteral("parental/pinHash");
const auto kSaltKey = QStringLiteral("parental/salt");
}

ParentalControl::ParentalControl(QObject *parent) : QObject(parent)
{
    // Every start begins locked.
    setLocked(hasPin());
}

bool ParentalControl::hasPin() const
{
    return !m_settings.value(kHashKey).toByteArray().isEmpty();
}

bool ParentalControl::isValidPin(const QString &pin) const
{
    return Parental::isValidPin(pin);
}

bool ParentalControl::checkPin(const QString &pin) const
{
    if (!hasPin())
        return true;
    return Parental::hashPin(pin, m_settings.value(kSaltKey).toByteArray())
            == m_settings.value(kHashKey).toByteArray();
}

bool ParentalControl::setPin(const QString &currentPin, const QString &newPin)
{
    if (!Parental::isValidPin(newPin) || (hasPin() && !checkPin(currentPin)))
        return false;
    const QByteArray salt = Parental::newSalt();
    m_settings.setValue(kSaltKey, salt);
    m_settings.setValue(kHashKey, Parental::hashPin(newPin, salt));
    setLocked(false);
    emit changed();
    return true;
}

bool ParentalControl::removePin(const QString &currentPin)
{
    if (!hasPin() || !checkPin(currentPin))
        return false;
    m_settings.remove(kHashKey);
    m_settings.remove(kSaltKey);
    setLocked(false);
    emit changed();
    return true;
}

bool ParentalControl::unlock(const QString &pin)
{
    if (!checkPin(pin))
        return false;
    setLocked(false);
    return true;
}

void ParentalControl::lock()
{
    if (hasPin())
        setLocked(true);
}

void ParentalControl::setLocked(bool locked)
{
    SourceStore::setParentalLocked(locked);
    if (locked == m_locked)
        return;
    m_locked = locked;
    emit changed();
    notifyContent();
}

void ParentalControl::notifyContent(int sourceId)
{
    // Lists, Home rows and "continue watching" reload with the new filter.
    auto *notifier = DataNotifier::instance();
    const QList<SourceRecord> sources = SourceStore(Database::connection()).sources();
    for (const SourceRecord &s : sources) {
        if (sourceId >= 0 && s.id != sourceId)
            continue;
        emit notifier->contentChanged(s.id);
        emit notifier->favoritesChanged(s.id);
        emit notifier->progressChanged(s.id);
    }
}

void ParentalControl::setCategoryLocked(int sourceId, const QString &kind, const QString &extId,
                                        bool locked)
{
    SourceStore(Database::connection()).setCategoryLocked(sourceId, kindFromString(kind), extId,
                                                          locked);
    emit changed();
    notifyContent(sourceId);
}

int ParentalControl::lockedCategoryCount(int sourceId) const
{
    return SourceStore(Database::connection()).lockedCategoryCount(sourceId);
}

QList<QPair<QString, QString>> ParentalControl::adultCategories(int sourceId) const
{
    QList<QPair<QString, QString>> result;
    QSqlQuery q(Database::connection());
    q.prepare(QStringLiteral(
            "SELECT cat.kind, cat.ext_id, cat.name FROM categories cat WHERE cat.source_id = ? "
            "AND NOT EXISTS (SELECT 1 FROM locked_categories lk WHERE lk.source_id = cat.source_id "
            "AND lk.kind = cat.kind AND lk.ext_id = cat.ext_id)"));
    q.addBindValue(sourceId);
    if (q.exec()) {
        while (q.next()) {
            if (Parental::looksAdult(q.value(2).toString()))
                result.append({ q.value(0).toString(), q.value(1).toString() });
        }
    }
    return result;
}

int ParentalControl::adultCategoryCount(int sourceId) const
{
    return int(adultCategories(sourceId).size());
}

int ParentalControl::lockAdultCategories(int sourceId)
{
    const auto categories = adultCategories(sourceId);
    SourceStore store(Database::connection());
    for (const auto &c : categories)
        store.setCategoryLocked(sourceId, kindFromString(c.first), c.second, true);
    emit changed();
    notifyContent(sourceId);
    return int(categories.size());
}
