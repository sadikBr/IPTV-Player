#pragma once

#include "SourceStore.h"
#include "XtreamApi.h"

#include <QFutureWatcher>
#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#include <functional>

class QNetworkAccessManager;

// Adds, refreshes and removes IPTV sources (Xtream logins and M3U playlists).
// Downloads happen on the main thread (asynchronously); parsing and saving
// run on a worker thread. One operation runs at a time.
class SourceManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(int activeSourceId READ activeSourceId NOTIFY busyChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(qreal progress READ progress NOTIFY statusChanged) // -1 = unknown
    Q_PROPERTY(bool canCancel READ canCancel NOTIFY statusChanged)
    Q_PROPERTY(bool hasSources READ hasSources NOTIFY sourcesChanged)

public:
    explicit SourceManager(QObject *parent = nullptr);
    ~SourceManager() override;

    bool isBusy() const { return m_busy; }
    int activeSourceId() const { return m_activeSourceId; }
    QString statusText() const { return m_statusText; }
    qreal progress() const { return m_progress; }
    bool canCancel() const { return m_reply != nullptr; }
    bool hasSources() const;

    Q_INVOKABLE void addXtreamSource(const QString &name, const QString &server,
                                     const QString &username, const QString &password,
                                     const QString &format);
    Q_INVOKABLE void addM3uSource(const QString &name, const QString &url, const QString &epgUrl);
    Q_INVOKABLE void refresh(int sourceId);
    // Refreshes in the background if the last sync is older than maxAgeHours.
    Q_INVOKABLE bool refreshIfStale(int sourceId, int maxAgeHours);
    Q_INVOKABLE void removeSource(int sourceId);
    Q_INVOKABLE void cancel();

    Q_INVOKABLE QVariantMap source(int sourceId) const;
    Q_INVOKABLE bool sourceExists(int sourceId) const;

    // {server, username, password} from a provider link such as .../get.php?username=..
    // Empty map if the link has no credentials.
    Q_INVOKABLE QVariantMap parseCredentialsUrl(const QString &url) const;

signals:
    void busyChanged();
    void statusChanged();
    void sourcesChanged();
    // isNew: the source was being added (on failure it has been removed again).
    void finished(int sourceId, bool ok, const QString &error, bool isNew);

private:
    struct WorkResult
    {
        bool ok = false;
        QString error;
        int liveCount = 0;
    };

    bool begin(int sourceId, bool isNew);
    void finish(bool ok, const QString &error = {});
    void setStatus(const QString &text, qreal progress = -1);

    using DataHandler = std::function<void(const QByteArray &)>;
    void download(const QUrl &url, const QString &label, const DataHandler &onData);

    void syncXtream();
    void syncM3u();
    void runWorker(std::function<WorkResult()> work);

    QNetworkAccessManager *m_nam = nullptr;
    QPointer<QNetworkReply> m_reply;
    QFutureWatcher<WorkResult> m_watcher;

    bool m_busy = false;
    bool m_isNew = false;
    bool m_cancelled = false;
    int m_activeSourceId = -1;
    iptv::SourceRecord m_source;
    iptv::SourceContent m_content; // collected Xtream data before saving
    QString m_statusText;
    qreal m_progress = -1;
};
