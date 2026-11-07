#pragma once

#include "SourceStore.h"

#include <QAbstractListModel>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// Episodes of one series. With `season` set, only that season is listed;
// with season 0 (the default) all episodes are, in order.
class EpisodeListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int sourceId READ sourceId WRITE setSourceId NOTIFY sourceIdChanged)
    Q_PROPERTY(QString seriesExtId READ seriesExtId WRITE setSeriesExtId NOTIFY seriesExtIdChanged)
    Q_PROPERTY(int season READ season WRITE setSeason NOTIFY seasonChanged)
    Q_PROPERTY(QVariantList seasons READ seasons NOTIFY seasonsChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        ExtIdRole = Qt::UserRole + 1,
        TitleRole,
        SeasonRole,
        EpisodeRole,
        PlotRole,
        ImageRole,
        DurationRole, // seconds
        WatchedRole,  // 0..1
    };

    explicit EpisodeListModel(QObject *parent = nullptr);

    int sourceId() const { return m_sourceId; }
    void setSourceId(int id);
    QString seriesExtId() const { return m_seriesExtId; }
    void setSeriesExtId(const QString &id);
    int season() const { return m_season; }
    void setSeason(int season);
    QVariantList seasons() const { return m_seasons; }

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // {extId, title, season, episode, plot, image, durationSecs, watched}
    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE QUrl streamUrl(int row) const;
    Q_INVOKABLE int indexOfExtId(const QString &extId) const;
    // The episode to continue with: the first unfinished after the last watched one.
    Q_INVOKABLE int resumeIndex() const;

public slots:
    void reload();

signals:
    void sourceIdChanged();
    void seriesExtIdChanged();
    void seasonChanged();
    void seasonsChanged();
    void countChanged();

private:
    struct Row
    {
        iptv::EpisodeData data;
        double watched = 0;
        bool finished = false;
        QDateTime lastWatched;
    };

    int m_sourceId = -1;
    QString m_seriesExtId;
    int m_season = 0;
    QVariantList m_seasons;
    iptv::SourceRecord m_source;
    QList<Row> m_rows;
};
