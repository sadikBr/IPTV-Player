#pragma once

#include "SourceStore.h"

#include <QAbstractListModel>
#include <QHash>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>
#include <QQmlParserStatus>
#include <QtQml/qqmlregistration.h>

// Live channels, movies or series of one source (see `kind`), filtered by
// category and/or search text. A non-empty search looks through everything
// of that kind.
//
// Long lists are loaded in chunks: the first rows right away, the rest as a
// view scrolls (canFetchMore/fetchMore), so pages open quickly. Created from
// QML, it loads once all its properties are set.
class ChannelListModel : public QAbstractListModel, public QQmlParserStatus
{
    Q_OBJECT
    QML_ELEMENT
    Q_INTERFACES(QQmlParserStatus)

    Q_PROPERTY(int sourceId READ sourceId WRITE setSourceId NOTIFY sourceIdChanged)
    // "live" (default), "movie" or "series"
    Q_PROPERTY(QString kind READ kind WRITE setKind NOTIFY kindChanged)
    Q_PROPERTY(int categoryId READ categoryId WRITE setCategoryId NOTIFY categoryIdChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    // Live: the longest catch-up archive of the channels listed (days)
    Q_PROPERTY(int maxArchiveDays READ maxArchiveDays NOTIFY countChanged)
    // "default" (provider order) or "newest" (most recently added first)
    // "default" (provider order), "newest" (recently added), "name", "name-desc",
    // "rating", "year" (movies/series), "number" (live)
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
        ArchiveDaysRole, // live: catch-up days, 0 = none
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
    bool canFetchMore(const QModelIndex &parent) const override;
    void fetchMore(const QModelIndex &parent) override;

    void classBegin() override;
    void componentComplete() override;

    // Loads the rows not loaded yet (lookups and wrap-around zapping need them all).
    Q_INVOKABLE void loadAll();

    // {channelId, extId, name, logo, number, favorite, epgKey}; empty if out of range.
    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE QUrl streamUrl(int row) const;
    int maxArchiveDays() const { return m_maxArchiveDays; }
    // Catch-up: can the program that started at start be watched from the
    // archive, and its URL.
    Q_INVOKABLE bool canCatchup(int row, const QDateTime &start) const;
    Q_INVOKABLE QUrl catchupUrl(int row, const QDateTime &start, const QDateTime &end) const;
    // Load the remaining rows if needed.
    Q_INVOKABLE int indexOfExtId(const QString &extId);
    Q_INVOKABLE int indexOfNumber(int number);
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
        int archiveDays = 0;
        QString catchupType;
        QString catchupSource;
        QString httpUserAgent; // headers the stream needs (M3U playlists)
        QString httpReferrer;
        // Name without symbols the device can't draw, worked out when first shown.
        mutable QString displayName;
        mutable bool displayReady = false;
    };

    struct NowNext
    {
        QString nowTitle;
        double nowProgress = 0;
        QString nextTitle;
    };

    const QString &displayName(int row) const;
    static QUrl relayed(const Row &row, const QUrl &url);
    void reloadSource();
    // Keeps at least as many rows loaded as before (data changed, same list).
    void refresh();
    void load(int minRows);
    QList<Row> queryRows(int offset, int count) const; // count < 0: all
    QString whereClause() const;
    void bindFilter(class QSqlQuery &q) const;
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
    bool m_hasMore = false;
    bool m_complete = true; // false while QML sets the initial properties
    int m_maxArchiveDays = 0;
    mutable QHash<int, NowNext> m_guideCache; // by row, filled on demand
    QTimer m_guideTimer;
};
