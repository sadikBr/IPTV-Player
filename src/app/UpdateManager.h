#pragma once

#include "UpdateCheck.h"

#include <QDateTime>
#include <QObject>
#include <QPointer>
#include <QSettings>
#include <QtQml/qqmlregistration.h>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

// Checks GitHub Releases for a newer version, downloads it and installs it.
//
// How it installs depends on how the app was installed:
//  - Windows installer: runs the new setup silently, which closes the app,
//    updates it in place and starts it again
//  - Linux AppImage: replaces the AppImage file and restarts
//  - Android: opens the system installer for the new APK
//  - anything else (portable zip, built from source): shows the release page
class UpdateManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // idle, checking, upToDate, available, downloading, installing, error
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY stateChanged)
    Q_PROPERTY(QString releaseNotes READ releaseNotes NOTIFY stateChanged)
    Q_PROPERTY(QUrl releasePage READ releasePage NOTIFY stateChanged)
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(qint64 downloadSize READ downloadSize NOTIFY stateChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY stateChanged)
    // True when the update can be installed from the app (else: release page).
    Q_PROPERTY(bool canInstall READ canInstall NOTIFY stateChanged)
    // How this copy was installed: windows-setup, windows-portable, appimage,
    // android, source
    Q_PROPERTY(QString installKind READ installKind CONSTANT)
    Q_PROPERTY(bool autoCheck READ autoCheck WRITE setAutoCheck NOTIFY autoCheckChanged)
    Q_PROPERTY(QDateTime lastChecked READ lastChecked NOTIFY stateChanged)
    // An update was found that the user hasn't skipped or postponed.
    Q_PROPERTY(bool showPrompt READ showPrompt NOTIFY stateChanged)

public:
    explicit UpdateManager(QObject *parent = nullptr);

    QString state() const { return m_state; }
    QString currentVersion() const;
    QString latestVersion() const { return m_release.version; }
    QString releaseNotes() const { return m_release.notes; }
    QUrl releasePage() const { return m_release.pageUrl; }
    qreal progress() const { return m_progress; }
    qint64 downloadSize() const { return m_release.assetSize; }
    QString errorString() const { return m_error; }
    bool canInstall() const;
    QString installKind() const { return m_installKind; }
    bool autoCheck() const;
    void setAutoCheck(bool on);
    QDateTime lastChecked() const;
    bool showPrompt() const;

    // manual: also report "up to date" and errors (the user asked).
    Q_INVOKABLE void check(bool manual = true);
    Q_INVOKABLE void install();
    Q_INVOKABLE void cancel();
    // Don't ask again for this version (Settings still shows it).
    Q_INVOKABLE void skipVersion();
    // Ask again next time the app starts.
    Q_INVOKABLE void later();

signals:
    void stateChanged();
    void progressChanged();
    void autoCheckChanged();

private:
    void setState(const QString &state, const QString &error = {});
    void onCheckFinished(QNetworkReply *reply, bool manual);
    void onDownloadFinished(QNetworkReply *reply, const QString &path, qint64 size,
                            const QByteArray &sha256);
    bool startInstaller(const QString &path, QString *error);
    QString assetName() const;
    QString downloadDir() const;
    void autoCheckIfDue();

    QSettings m_settings;
    QNetworkAccessManager *m_network = nullptr;
    QPointer<QNetworkReply> m_reply;
    QTimer *m_timer = nullptr;
    iptv::ReleaseInfo m_release;
    QString m_state = QStringLiteral("idle");
    QString m_error;
    QString m_installKind;
    qreal m_progress = 0;
    bool m_postponed = false;
};
