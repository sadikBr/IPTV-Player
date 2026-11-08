#pragma once

#include <QByteArray>
#include <QList>
#include <QSqlDatabase>
#include <QString>
#include <QVariantMap>

namespace iptv {

// Moving everything to another device: providers (with their logins),
// favorites, hidden and locked categories, watch progress, radio stations
// and settings, as one JSON file. Channel lists are not included; they are
// downloaded again from the provider.
namespace Backup {

QByteArray exportJson(QSqlDatabase db, const QVariantMap &settings);

struct ImportResult
{
    bool ok = false;
    QString error;
    int providersAdded = 0;
    int providersMerged = 0;   // already on this device: favorites etc. added to it
    QList<int> newSourceIds;   // need their lists downloaded
    int radioStations = 0;
    QVariantMap settings;      // to apply (the caller owns the settings store)
};

ImportResult importJson(QSqlDatabase db, const QByteArray &json);

} // namespace Backup
} // namespace iptv
