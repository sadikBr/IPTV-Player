#pragma once

#include "SourceStore.h"

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

// All configured sources, for the home page.
class SourceListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        SourceIdRole = Qt::UserRole + 1,
        NameRole,
        TypeRole,
        UrlRole,
        UsernameRole,
        LiveCountRole,
        LastSyncRole,
        ExpiresAtRole,
        StatusRole,
        MaxConnectionsRole,
        EpgLastSyncRole,
        EpgCountRole,
        EpgShiftRole,
        MovieCountRole,
        SeriesCountRole,
    };

    explicit SourceListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Id of the provider in a row (-1 if out of range).
    Q_INVOKABLE int sourceIdAt(int row) const;

public slots:
    void reload();

signals:
    void countChanged();

private:
    QList<iptv::SourceRecord> m_sources;
};
