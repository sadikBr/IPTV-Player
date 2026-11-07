#include "UpdateManager.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

#include <cstdio>
#include <memory>

#ifdef Q_OS_ANDROID
#include <QJniEnvironment>
#include <QJniObject>
#include <QtCore/qcoreapplication_platform.h>
#endif

namespace {

const auto kAutoCheckKey = QStringLiteral("updates/autoCheck");
const auto kLastCheckKey = QStringLiteral("updates/lastCheck");
const auto kSkippedKey = QStringLiteral("updates/skippedVersion");

// Where releases are published. IPTV_UPDATE_URL overrides it (tests).
QUrl latestReleaseUrl()
{
    const QString overrideUrl = qEnvironmentVariable("IPTV_UPDATE_URL");
    if (!overrideUrl.isEmpty())
        return QUrl(overrideUrl);
    return QUrl(QStringLiteral("https://api.github.com/repos/sadikBr/IPTV-Player/releases/latest"));
}

constexpr int kCheckEveryHours = 12;

QString detectInstallKind()
{
#if defined(Q_OS_ANDROID)
    return QStringLiteral("android");
#elif defined(Q_OS_WIN)
    // The installer puts its uninstaller next to the app; the zip doesn't.
    const QDir appDir(QCoreApplication::applicationDirPath());
    return appDir.exists(QStringLiteral("unins000.exe")) ? QStringLiteral("windows-setup")
                                                         : QStringLiteral("windows-portable");
#elif defined(Q_OS_LINUX)
    return qEnvironmentVariableIsSet("APPIMAGE") ? QStringLiteral("appimage")
                                                 : QStringLiteral("source");
#else
    return QStringLiteral("source");
#endif
}

} // namespace

UpdateManager::UpdateManager(QObject *parent)
    : QObject(parent), m_network(new QNetworkAccessManager(this)), m_timer(new QTimer(this)),
      m_installKind(detectInstallKind())
{
    // Leftovers from an update that was installed
    QDir(downloadDir()).removeRecursively();

    // Check a little after start-up, then every few hours while running.
    m_timer->setInterval(kCheckEveryHours * 3600 * 1000);
    connect(m_timer, &QTimer::timeout, this, &UpdateManager::autoCheckIfDue);
    m_timer->start();
    if (!qEnvironmentVariableIsSet("IPTV_NO_UPDATE_CHECK"))
        QTimer::singleShot(5000, this, &UpdateManager::autoCheckIfDue);
}

QString UpdateManager::currentVersion() const
{
    return QCoreApplication::applicationVersion();
}

bool UpdateManager::canInstall() const
{
    if (m_release.assetUrl.isEmpty())
        return false;
    return m_installKind == QLatin1String("windows-setup")
        || m_installKind == QLatin1String("appimage")
        || m_installKind == QLatin1String("android");
}

QString UpdateManager::assetName() const
{
    if (m_installKind == QLatin1String("windows-setup"))
        return QStringLiteral("IPTVPlayer-setup-x64.exe");
    if (m_installKind == QLatin1String("appimage"))
        return QStringLiteral("IPTVPlayer-x86_64.AppImage");
    if (m_installKind == QLatin1String("android"))
        return QStringLiteral("IPTVPlayer-android.apk");
    return {};
}

QString UpdateManager::downloadDir() const
{
    // On Android this is the app's private files folder, which the app's
    // FileProvider shares with the system installer.
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
            .filePath(QStringLiteral("updates"));
}

bool UpdateManager::autoCheck() const
{
    return m_settings.value(kAutoCheckKey, true).toBool();
}

void UpdateManager::setAutoCheck(bool on)
{
    if (on == autoCheck())
        return;
    m_settings.setValue(kAutoCheckKey, on);
    emit autoCheckChanged();
    if (on)
        autoCheckIfDue();
}

QDateTime UpdateManager::lastChecked() const
{
    return m_settings.value(kLastCheckKey).toDateTime();
}

bool UpdateManager::showPrompt() const
{
    return m_state == QLatin1String("available") && !m_postponed
        && m_settings.value(kSkippedKey).toString() != m_release.version;
}

void UpdateManager::setState(const QString &state, const QString &error)
{
    m_state = state;
    m_error = error;
    emit stateChanged();
}

void UpdateManager::autoCheckIfDue()
{
    if (qEnvironmentVariableIsSet("IPTV_NO_UPDATE_CHECK"))
        return;
    if (!autoCheck() || m_reply || m_state == QLatin1String("downloading")
        || m_state == QLatin1String("installing"))
        return;
    const QDateTime last = lastChecked();
    if (last.isValid() && last.secsTo(QDateTime::currentDateTimeUtc()) < kCheckEveryHours * 3600
        && m_state != QLatin1String("idle"))
        return;
    check(false);
}

void UpdateManager::check(bool manual)
{
    if (m_reply)
        return;
    if (manual)
        m_postponed = false;
    QNetworkRequest request(latestReleaseUrl());
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("IPTVPlayer/%1").arg(currentVersion()));
    request.setTransferTimeout(20000);
    m_reply = m_network->get(request);
    QNetworkReply *reply = m_reply;
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, manual] { onCheckFinished(reply, manual); });
    setState(QStringLiteral("checking"));
}

void UpdateManager::onCheckFinished(QNetworkReply *reply, bool manual)
{
    reply->deleteLater();
    const QByteArray body = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError && status != 404) {
        // Quiet when checking on its own: no point bothering about a
        // missing connection.
        setState(manual ? QStringLiteral("error") : QStringLiteral("idle"),
                 tr("Couldn't check for updates: %1").arg(reply->errorString()));
        return;
    }
    m_settings.setValue(kLastCheckKey, QDateTime::currentDateTimeUtc());

    const iptv::ReleaseInfo release = iptv::Updates::parseRelease(body, assetName());
    if (!release.valid
        || iptv::Updates::compareVersions(release.version, currentVersion()) <= 0) {
        m_release = release.valid ? release : iptv::ReleaseInfo();
        setState(QStringLiteral("upToDate"));
        return;
    }
    m_release = release;
    setState(QStringLiteral("available"));
}

void UpdateManager::install()
{
    if (m_state != QLatin1String("available") && m_state != QLatin1String("error"))
        return;
    if (!canInstall()) {
        QDesktopServices::openUrl(m_release.pageUrl);
        return;
    }

    QDir().mkpath(downloadDir());
    const QString path = QDir(downloadDir()).filePath(m_release.assetName);
    auto *file = new QFile(path, this);
    if (!file->open(QIODevice::WriteOnly)) {
        setState(QStringLiteral("error"), tr("Couldn't save the update: %1").arg(file->errorString()));
        delete file;
        return;
    }
    auto hash = std::make_shared<QCryptographicHash>(QCryptographicHash::Sha256);

    QNetworkRequest request(m_release.assetUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("IPTVPlayer/%1").arg(currentVersion()));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_network->get(request);
    QNetworkReply *reply = m_reply;
    m_progress = 0;
    emit progressChanged();
    // Written to disk as it arrives (the files are tens of MB).
    connect(reply, &QNetworkReply::readyRead, file, [reply, file, hash] {
        const QByteArray chunk = reply->readAll();
        hash->addData(chunk);
        file->write(chunk);
    });
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (total <= 0)
            total = m_release.assetSize;
        m_progress = total > 0 ? qreal(received) / total : 0;
        emit progressChanged();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, file, hash] {
        const QByteArray rest = reply->readAll();
        hash->addData(rest);
        file->write(rest);
        const bool written = file->error() == QFileDevice::NoError;
        const qint64 size = file->size();
        const QString path = file->fileName();
        file->close();
        file->deleteLater();
        onDownloadFinished(reply, path, size, written ? hash->result().toHex() : QByteArray());
    });
    setState(QStringLiteral("downloading"));
}

void UpdateManager::cancel()
{
    if (m_reply && m_state == QLatin1String("downloading")) {
        m_reply->abort(); // finished() removes the partial file
        setState(QStringLiteral("available"));
    }
}

void UpdateManager::onDownloadFinished(QNetworkReply *reply, const QString &path, qint64 size,
                                       const QByteArray &sha256)
{
    reply->deleteLater();
    if (reply->error() == QNetworkReply::OperationCanceledError) {
        QFile::remove(path);
        return; // cancel()
    }
    QString problem;
    if (reply->error() != QNetworkReply::NoError)
        problem = tr("The download failed: %1").arg(reply->errorString());
    else if (sha256.isEmpty())
        problem = tr("Couldn't save the update (is the disk full?).");
    else if (m_release.assetSize > 0 && size != m_release.assetSize)
        problem = tr("The download is incomplete. Please try again.");
    else if (!m_release.sha256.isEmpty() && sha256 != m_release.sha256)
        problem = tr("The downloaded file doesn't match the release (checksum). Please try again.");
    if (!problem.isEmpty()) {
        QFile::remove(path);
        setState(QStringLiteral("error"), problem);
        return;
    }

    setState(QStringLiteral("installing"));
    QString error;
    if (!startInstaller(path, &error))
        setState(QStringLiteral("error"), error);
}

bool UpdateManager::startInstaller(const QString &path, QString *error)
{
#if defined(Q_OS_ANDROID)
    // Hand the APK to Android's package installer (it asks the user).
    QJniObject context(QNativeInterface::QAndroidApplication::context());
    const QString authority =
            context.callObjectMethod("getPackageName", "()Ljava/lang/String;").toString()
            + QStringLiteral(".qtprovider");
    QJniObject file("java/io/File", "(Ljava/lang/String;)V",
                    QJniObject::fromString(path).object<jstring>());
    QJniObject uri = QJniObject::callStaticObjectMethod(
            "androidx/core/content/FileProvider", "getUriForFile",
            "(Landroid/content/Context;Ljava/lang/String;Ljava/io/File;)Landroid/net/Uri;",
            context.object(), QJniObject::fromString(authority).object<jstring>(),
            file.object());
    QJniEnvironment env;
    if (env.checkAndClearExceptions() || !uri.isValid()) {
        *error = tr("Couldn't open the installer.");
        return false;
    }
    QJniObject intent("android/content/Intent", "(Ljava/lang/String;)V",
                      QJniObject::fromString(QStringLiteral("android.intent.action.VIEW"))
                              .object<jstring>());
    intent.callObjectMethod("setDataAndType",
                            "(Landroid/net/Uri;Ljava/lang/String;)Landroid/content/Intent;",
                            uri.object(),
                            QJniObject::fromString(QStringLiteral(
                                    "application/vnd.android.package-archive")).object<jstring>());
    // FLAG_GRANT_READ_URI_PERMISSION | FLAG_ACTIVITY_NEW_TASK
    intent.callObjectMethod("addFlags", "(I)Landroid/content/Intent;", jint(0x00000001 | 0x10000000));
    context.callMethod<void>("startActivity", "(Landroid/content/Intent;)V", intent.object());
    if (env.checkAndClearExceptions()) {
        *error = tr("Couldn't open the installer.");
        return false;
    }
    // The installer takes over; Android restarts the app after updating.
    QTimer::singleShot(3000, this, [this] { setState(QStringLiteral("available")); });
    return true;
#elif defined(Q_OS_WIN)
    // Silent install: shows a progress window, closes this app, updates it in
    // place and starts it again (see installer/windows/iptvplayer.iss).
    QStringList args { QStringLiteral("/SILENT"), QStringLiteral("/SUPPRESSMSGBOXES"),
                       QStringLiteral("/NORESTART"), QStringLiteral("/CLOSEAPPLICATIONS") };
    // Installed for all users (Program Files): update that copy, not a new
    // per-user one. Windows asks for admin rights.
    const QString appDir = QDir::cleanPath(QCoreApplication::applicationDirPath());
    for (const char *var : { "ProgramFiles", "ProgramW6432", "ProgramFiles(x86)" }) {
        const QString programFiles = QDir::cleanPath(qEnvironmentVariable(var));
        if (!programFiles.isEmpty()
            && appDir.startsWith(programFiles + QLatin1Char('/'), Qt::CaseInsensitive)) {
            args << QStringLiteral("/ALLUSERS");
            break;
        }
    }
    if (!QProcess::startDetached(path, args)) {
        *error = tr("Couldn't start the installer.");
        return false;
    }
    QTimer::singleShot(500, qApp, &QCoreApplication::quit);
    return true;
#else
    // AppImage: put the new file in place of the running one and restart.
    const QString target = qEnvironmentVariable("APPIMAGE");
    const QFileInfo targetInfo(target);
    const QString staged = targetInfo.dir().filePath(QStringLiteral(".%1.update")
                                                             .arg(targetInfo.fileName()));
    QFile::remove(staged);
    if (target.isEmpty() || !QFile::copy(path, staged)) {
        *error = tr("Couldn't write to %1. Download the new version from the release page instead.")
                         .arg(targetInfo.absolutePath());
        return false;
    }
    QFile::setPermissions(staged, QFile::permissions(target) | QFileDevice::ExeOwner
                                          | QFileDevice::ExeGroup | QFileDevice::ExeOther);
    // rename() replaces the old file in one step; the running copy keeps
    // working until it exits.
    if (std::rename(QFile::encodeName(staged).constData(),
                    QFile::encodeName(target).constData()) != 0) {
        QFile::remove(staged);
        *error = tr("Couldn't replace %1.").arg(target);
        return false;
    }
    QProcess::startDetached(target, {});
    QTimer::singleShot(300, qApp, &QCoreApplication::quit);
    return true;
#endif
}

void UpdateManager::skipVersion()
{
    m_settings.setValue(kSkippedKey, m_release.version);
    emit stateChanged();
}

void UpdateManager::later()
{
    m_postponed = true;
    emit stateChanged();
}
