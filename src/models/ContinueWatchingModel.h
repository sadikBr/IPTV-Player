#pragma once

#include "SourceStore.h"

#include <QAbstractListModel>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// Unfinished movies and episodes across all sources, most recent first.
class ContinueWatchingModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        SourceIdRole = Qt::UserRole + 1,
        KindRole, // "movie" | "episode"
        ExtIdRole,
        TitleRole,
        SubtitleRole, // e.g. "S1 · E3" for episodes
        ImageRole,
        WatchedRole, // 0..1
        RemainingRole, // minutes left
        SeriesExtIdRole,
    };

    explicit ContinueWatchingModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // All roles of one row as a map (empty if out of range).
    Q_INVOKABLE QVariantMap get(int row) const;

public slots:
    void reload();

signals:
    void countChanged();

private:
    QList<iptv::WatchProgress> m_items;
};
