#pragma once

#include <QFutureWatcher>
#include <QList>
#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#include <memory>

class QNetworkAccessManager;
class QTemporaryFile;

// Downloads and imports TV guides (XMLTV), and answers "what's on" questions
// for QML. Guides are downloaded to a temporary file (they can be hundreds of
// MB) and imported on a worker thread. Requests are queued, one at a time.
class EpgManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(int activeSourceId READ activeSourceId NOTIFY busyChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(qreal progress READ progress NOTIFY statusChanged)

public:
    explicit EpgManager(QObject *parent = nullptr);
    ~EpgManager() override;

    bool isBusy() const { return m_activeSourceId > 0; }
    int activeSourceId() const { return m_activeSourceId; }
    QString statusText() const { return m_statusText; }
    qreal progress() const { return m_progress; }

    Q_INVOKABLE void refresh(int sourceId);
    Q_INVOKABLE bool refreshIfStale(int sourceId, int maxAgeHours);
    Q_INVOKABLE void cancel();

    // True if the source has a guide URL (Xtream, or an M3U with a guide link).
    Q_INVOKABLE bool canHaveGuide(int sourceId) const;
    Q_INVOKABLE bool hasGuide(int sourceId) const;

    // {now: {title, subTitle, description, start, stop, progress}, next: {...}}
    // Either may be missing. Dates are JS Dates in QML.
    Q_INVOKABLE QVariantMap nowNext(int sourceId, const QString &epgKey) const;
    // Programs overlapping [from, to] as a list of {title, subTitle, description, start, stop}.
    Q_INVOKABLE QVariantList programs(int sourceId, const QString &epgKey, const QDateTime &from,
                                      const QDateTime &to) const;

signals:
    void busyChanged();
    void statusChanged();
    void finished(int sourceId, bool ok, const QString &error);

private:
    struct ImportResult
    {
        bool ok = false;
        QString error;
        int count = 0;
    };

    void startNext();
    void finish(bool ok, const QString &error);
    void setStatus(const QString &text, qreal progress = -1);
    void import(const QString &path);

    QNetworkAccessManager *m_nam = nullptr;
    QPointer<QNetworkReply> m_reply;
    std::unique_ptr<QTemporaryFile> m_file;
    QFutureWatcher<ImportResult> m_watcher;
    QList<int> m_queue;
    int m_activeSourceId = -1;
    bool m_cancelled = false;
    QString m_statusText;
    qreal m_progress = -1;
};
