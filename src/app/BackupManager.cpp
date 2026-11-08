#include "BackupManager.h"

#include "Backup.h"
#include "Database.h"

#include <QDate>
#include <QFile>
#include <QSettings>

using namespace iptv;

namespace {

// Settings that belong to this device, not to the person.
bool deviceOnly(const QString &key)
{
    return key.startsWith(QLatin1String("updates/")) || key.startsWith(QLatin1String("subtitleFiles/"))
        || key == QLatin1String("playback/hardwareDecoding") || key.startsWith(QLatin1String("window/"));
}

QString localPath(const QUrl &file)
{
    // Local files, and Android documents (content://) which QFile opens too.
    return file.isLocalFile() ? file.toLocalFile() : file.toString();
}

} // namespace

QString BackupManager::suggestedFileName() const
{
    return QStringLiteral("iptv-player-backup-%1.json").arg(QDate::currentDate().toString(Qt::ISODate));
}

QString BackupManager::exportTo(const QUrl &file)
{
    QSettings settings;
    QVariantMap values;
    for (const QString &key : settings.allKeys()) {
        if (!deviceOnly(key))
            values.insert(key, settings.value(key));
    }
    const QByteArray json = Backup::exportJson(Database::connection(), values);
    QFile out(localPath(file));
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate) || out.write(json) != json.size())
        return tr("Couldn't save the backup: %1").arg(out.errorString());
    return {};
}

QVariantMap BackupManager::importFrom(const QUrl &file)
{
    QFile in(localPath(file));
    if (!in.open(QIODevice::ReadOnly))
        return { { QStringLiteral("ok"), false },
                 { QStringLiteral("error"), tr("Couldn't open the file: %1").arg(in.errorString()) } };
    const Backup::ImportResult result = Backup::importJson(Database::connection(), in.read(20 * 1024 * 1024));
    if (!result.ok)
        return { { QStringLiteral("ok"), false }, { QStringLiteral("error"), result.error } };

    QSettings settings;
    for (auto it = result.settings.cbegin(); it != result.settings.cend(); ++it) {
        if (!deviceOnly(it.key()))
            settings.setValue(it.key(), it.value());
    }

    // Lists, favorites, Home and the radio reload. The page downloads the
    // lists of the first new provider (firstNewSource); the others load
    // when they are opened.
    emit DataNotifier::instance()->sourcesChanged();
    return { { QStringLiteral("ok"), true },
             { QStringLiteral("firstNewSource"), result.newSourceIds.isEmpty() ? -1 : result.newSourceIds.first() },
             { QStringLiteral("providersAdded"), result.providersAdded },
             { QStringLiteral("providersMerged"), result.providersMerged },
             { QStringLiteral("radioStations"), result.radioStations } };
}
