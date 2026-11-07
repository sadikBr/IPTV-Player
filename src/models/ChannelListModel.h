#pragma once

#include "SourceStore.h"

#include <QAbstractListModel>
#include <QHash>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// Live channels of one source, filtered by category and/or search text.
// A non-empty search looks through all channels of the source.
class ChannelListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int sourceId READ sourceId WRITE setSourceId NOTIFY sourceIdChanged)
    Q_PROPERTY(int categoryId READ categoryId WRITE setCategoryId NOTIFY categoryIdChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        ChannelIdRole = Qt::UserRole + 1,
        ExtIdRole,
        NameRole,
        LogoRole,
        NumberRole,
        FavoriteRole,
        EpgKeyRole,
        NowTitleRole,    // TV guide: current program ("" if unknown)
        NowProgressRole, // 0..1
        NextTitleRole,
    };

    explicit ChannelListModel(QObject *parent = nullptr);

    int sourceId() const { return m_sourceId; }
    void setSourceId(int id);
    int categoryId() const { return m_categoryId; }
    void setCategoryId(int id);
    QString searchText() const { return m_searchText; }
    void setSearchText(const QString &text);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // {channelId, extId, name, logo, number, favorite, epgKey}; empty if out of range.
    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE QUrl streamUrl(int row) const;
    Q_INVOKABLE int indexOfExtId(const QString &extId) const;
    Q_INVOKABLE int indexOfNumber(int number) const;
    Q_INVOKABLE void toggleFavorite(int row);

public slots:
    void reload();

signals:
    void sourceIdChanged();
    void categoryIdChanged();
    void searchTextChanged();
    void countChanged();

private:
    struct Row
    {
        int id;
        QString extId;
        QString name;
        QString logo;
        QString url;
        QString epgKey;
        int number;
        bool favorite;
    };

    struct NowNext
    {
        QString nowTitle;
        double nowProgress = 0;
        QString nextTitle;
    };

    void reloadSource();
    const NowNext &nowNext(int row) const;
    void refreshGuide();

    int m_sourceId = -1;
    int m_categoryId = -1; // CategoryListModel::AllCategories
    QString m_searchText;
    iptv::SourceRecord m_source;
    QList<Row> m_rows;
    mutable QHash<int, NowNext> m_guideCache; // by row, filled on demand
    QTimer m_guideTimer;
};
