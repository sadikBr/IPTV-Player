#pragma once

#include "SourceStore.h"

#include <QAbstractListModel>
#include <QHash>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// Live channels, movies or series of one source (see `kind`), filtered by
// category and/or search text. A non-empty search looks through everything
// of that kind.
class ChannelListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int sourceId READ sourceId WRITE setSourceId NOTIFY sourceIdChanged)
    // "live" (default), "movie" or "series"
    Q_PROPERTY(QString kind READ kind WRITE setKind NOTIFY kindChanged)
    Q_PROPERTY(int categoryId READ categoryId WRITE setCategoryId NOTIFY categoryIdChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    // "default" (provider order) or "newest" (most recently added first)
    Q_PROPERTY(QString sortOrder READ sortOrder WRITE setSortOrder NOTIFY sortOrderChanged)
    // Maximum number of rows, 0 = all (home page rows)
    Q_PROPERTY(int limit READ limit WRITE setLimit NOTIFY limitChanged)

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
        YearRole,
        RatingRole,
        WatchedRole, // movies: how much has been watched, 0..1
    };

    explicit ChannelListModel(QObject *parent = nullptr);

    int sourceId() const { return m_sourceId; }
    void setSourceId(int id);
    QString kind() const { return m_kind; }
    void setKind(const QString &kind);
    int categoryId() const { return m_categoryId; }
    void setCategoryId(int id);
    QString searchText() const { return m_searchText; }
    QString sortOrder() const { return m_sortOrder; }
    void setSortOrder(const QString &order);
    int limit() const { return m_limit; }
    void setLimit(int limit);
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
    void kindChanged();
    void categoryIdChanged();
    void searchTextChanged();
    void sortOrderChanged();
    void limitChanged();
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
        QString extension;
        QString year;
        QString rating;
        double watched;
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
    QString m_kind = QStringLiteral("live");
    int m_categoryId = -1; // CategoryListModel::AllCategories
    QString m_searchText;
    QString m_sortOrder = QStringLiteral("default");
    int m_limit = 0;
    iptv::SourceRecord m_source;
    QList<Row> m_rows;
    mutable QHash<int, NowNext> m_guideCache; // by row, filled on demand
    QTimer m_guideTimer;
};
