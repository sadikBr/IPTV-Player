#pragma once

#include <QDateTime>
#include <QObject>
#include <QSet>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class QNetworkAccessManager;

// Movies and series for QML: details pages, episodes and watch progress.
// Xtream details and episodes are fetched when a title is opened and cached.
class VodManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit VodManager(QObject *parent = nullptr);

    // Everything known about a movie or series:
    // {name, poster, backdrop, year, rating, genre, plot, cast, director,
    //  releaseDate, durationSecs, favorite}
    Q_INVOKABLE QVariantMap movie(int sourceId, const QString &extId) const;
    Q_INVOKABLE QVariantMap series(int sourceId, const QString &extId) const;

    // Loads details from the provider if not cached yet. Emits detailsLoaded.
    Q_INVOKABLE void fetchMovie(int sourceId, const QString &extId);
    // Loads the episode list (and details). Emits seriesLoaded.
    Q_INVOKABLE void fetchSeries(int sourceId, const QString &extId, bool force = false);
    Q_INVOKABLE bool isLoading(int sourceId, const QString &extId) const;

    Q_INVOKABLE QUrl movieUrl(int sourceId, const QString &extId) const;

    // Watch progress. kind: "movie" or "episode".
    Q_INVOKABLE double resumePosition(int sourceId, const QString &kind, const QString &extId) const;
    Q_INVOKABLE void saveProgress(int sourceId, const QString &kind, const QString &extId,
                                  double positionMs, double durationMs, const QVariantMap &info);
    Q_INVOKABLE void clearProgress(int sourceId, const QString &kind, const QString &extId);

    Q_INVOKABLE bool isFavorite(int sourceId, const QString &kind, const QString &extId) const;
    Q_INVOKABLE void setFavorite(int sourceId, const QString &kind, const QString &extId, bool favorite);

signals:
    void detailsLoaded(int sourceId, const QString &extId, bool ok, const QString &error);
    void seriesLoaded(int sourceId, const QString &extId, bool ok, const QString &error);
    void loadingChanged();

private:
    QVariantMap describe(int sourceId, int kind, const QString &extId) const;

    QNetworkAccessManager *m_nam = nullptr;
    QSet<QString> m_loading; // "sourceId|extId"
};
