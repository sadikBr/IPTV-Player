#pragma once

#include <QObject>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// Export / import of everything (see iptv::Backup) to move to another device.
class BackupManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    using QObject::QObject;

    // Writes the backup file; returns an error message, empty on success.
    Q_INVOKABLE QString exportTo(const QUrl &file);
    // {ok, error, providersAdded, providersMerged, radioStations, firstNewSource}
    Q_INVOKABLE QVariantMap importFrom(const QUrl &file);
    Q_INVOKABLE QString suggestedFileName() const;
};
