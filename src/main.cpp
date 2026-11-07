#include "Database.h"
#include "NetworkFactory.h"

#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QKeyEvent>
#include <QKeySequence>
#include <QQuickItem>
#include "app/SpatialNavigation.h"
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>
#include <functional>
#include <memory>

namespace {

// Walks through the main screens and exits with 0 if no QML warnings were
// printed. With IPTV_SMOKE_PLAYLIST (or IPTV_SMOKE_XTREAM) set, it adds a source,
// then visits the channel list, guide, player, movies and series. Otherwise it opens the
// player on an unreachable stream. With IPTV_SCREENSHOT_DIR set, screenshots
// of each step are saved there.
void runSmokeTest(QQmlApplicationEngine &engine, const int &qmlWarnings)
{
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    // IPTV_SMOKE_SIZE=400x800 checks the phone layout.
    const QStringList size = qEnvironmentVariable("IPTV_SMOKE_SIZE").split(QLatin1Char('x'));
    if (size.size() == 2 && window)
        window->resize(size.at(0).toInt(), size.at(1).toInt());
    const QString shotDir = qEnvironmentVariable("IPTV_SCREENSHOT_DIR");
    auto screenshot = [window, shotDir](const QString &name) {
        if (!shotDir.isEmpty() && window)
            window->grabWindow().save(QDir(shotDir).filePath(name + QStringLiteral(".png")));
    };

    const QString playlist = qEnvironmentVariable("IPTV_SMOKE_PLAYLIST");
    // "server|username|password": add an Xtream source instead of a playlist.
    const QStringList xtream = qEnvironmentVariable("IPTV_SMOKE_XTREAM").split(QLatin1Char('|'));
    const bool useXtream = xtream.size() == 3;
    const bool addSource = useXtream || !playlist.isEmpty();
    const QString smokeUrl = qEnvironmentVariable("IPTV_SMOKE_URL",
                                                  QStringLiteral("http://127.0.0.1:9/test.ts"));

    struct Step
    {
        int delayMs;   // wait before this step
        QString shot;  // screenshot name, or empty
        std::function<void()> action;
    };
    auto invoke = [window](const char *method, const QVariant &arg = {}) {
        if (arg.isValid())
            QMetaObject::invokeMethod(window, method, Q_ARG(QVariant, arg));
        else
            QMetaObject::invokeMethod(window, method);
    };

    QList<Step> steps;
    steps.append({ 500, QStringLiteral("home"), [=] { invoke("smokeShowAddPage"); } });
    steps.append({ 700, QStringLiteral("add"), [=] {
                      invoke("smokeCloseAddPage");
                      if (useXtream) {
                          QMetaObject::invokeMethod(window, "smokeAddXtream",
                                                    Q_ARG(QVariant, xtream.at(0)),
                                                    Q_ARG(QVariant, xtream.at(1)),
                                                    Q_ARG(QVariant, xtream.at(2)));
                      } else if (playlist.isEmpty()) {
                          QMetaObject::invokeMethod(window, "openStream", Q_ARG(QVariant, QUrl(smokeUrl)),
                                                    Q_ARG(QVariant, QStringLiteral("Smoke test channel")));
                      } else {
                          invoke("smokeAddPlaylist", QUrl::fromLocalFile(playlist));
                      }
                  } });
    if (addSource) {
        steps.append({ 2800, QStringLiteral("channels"), [=] { invoke("smokeOpenGuide"); } });
        steps.append({ 1500, QStringLiteral("guide"), [=] { invoke("smokePlayFirst"); } });
        steps.append({ 2500, QStringLiteral("player"), [=] { invoke("smokeBack"); } });
        steps.append({ 500, QString(), [=] { invoke("smokeOpenSection", QStringLiteral("movie")); } });
        steps.append({ 1500, QStringLiteral("movies"), [=] { invoke("smokeOpenFirst"); } });
        steps.append({ 1500, QStringLiteral("movie"), [=] { invoke("smokePlay"); } });
        steps.append({ 2500, QStringLiteral("movie-player"), [=] {
                          invoke("smokeBack");
                          invoke("smokeOpenSection", QStringLiteral("series"));
                      } });
        steps.append({ 1500, QStringLiteral("series-list"), [=] { invoke("smokeOpenFirst"); } });
        steps.append({ 2000, QStringLiteral("series"), [=] { invoke("smokePlay"); } });
        steps.append({ 2500, QStringLiteral("episode-player"), [=] { invoke("smokeBack"); } });
        steps.append({ 800, QString(), [=] { invoke("smokeOpenSection", QStringLiteral("home")); } });
        steps.append({ 2000, QStringLiteral("home-dashboard"), [=] {
                          invoke("smokeOpenSection", QStringLiteral("settings"));
                      } });
        steps.append({ 1000, QStringLiteral("settings"), [=] {
                          invoke("smokeManageCategories", QStringLiteral("live"));
                      } });
        steps.append({ 1200, QStringLiteral("categories"), [=] {
                          invoke("smokeHideGroup", QStringLiteral("AR"));
                      } });
        steps.append({ 600, QStringLiteral("categories-hidden"), [=] {
                          invoke("smokeOpenSection", QStringLiteral("live"));
                      } });
        steps.append({ 1200, QStringLiteral("channels-filtered"), {} });
    } else {
        steps.append({ 2000, QStringLiteral("player"), {} });
    }

    // IPTV_UPDATE_URL: check that (test) server for an update and show the
    // dialog; IPTV_SMOKE_INSTALL_UPDATE=1 also installs it.
    if (qEnvironmentVariableIsSet("IPTV_UPDATE_URL")) {
        steps.append({ 300, QString(), [=] { invoke("smokeCheckUpdates"); } });
        if (qEnvironmentVariableIntValue("IPTV_SMOKE_INSTALL_UPDATE") == 1) {
            steps.append({ 2000, QStringLiteral("update"), [=] { invoke("smokeInstallUpdate"); } });
            steps.append({ 600, QStringLiteral("update-downloading"), {} });
            steps.append({ 15000, QString(), {} }); // the app quits itself once installed
        } else {
            steps.append({ 2000, QStringLiteral("update"), {} });
        }
    }

    // IPTV_SMOKE_KEYS="Down,Right,Return:shot-name,...": press keys at the end
    // (arrow-key / TV remote navigation) and log which item has focus.
    const QStringList keys = qEnvironmentVariable("IPTV_SMOKE_KEYS").split(QLatin1Char(','),
                                                                            Qt::SkipEmptyParts);
    for (const QString &entry : keys) {
        const QString keyName = entry.section(QLatin1Char(':'), 0, 0);
        const QString shot = entry.section(QLatin1Char(':'), 1, 1);
        const QKeySequence seq = QKeySequence::fromString(keyName);
        const int key = seq.isEmpty() ? 0 : seq[0].key();
        steps.append({ 450, QString(), [=] {
                          QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier);
                          QCoreApplication::sendEvent(window, &press);
                          QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier);
                          QCoreApplication::sendEvent(window, &release);
                      } });
        steps.append({ 250, shot, [=] {
                          QQuickItem *f = window->activeFocusItem();
                          const QString text = f ? f->property("text").toString() : QString();
                          QStringList chain;
                          for (QQuickItem *i = f; i && chain.size() < 6; i = i->parentItem())
                              chain << QString::fromLatin1(i->metaObject()->className())
                                               .section(QLatin1Char('_'), 0, 0)
                                               + (i->objectName().isEmpty() ? QString()
                                                  : QLatin1Char('#') + i->objectName());
                          std::fprintf(stderr, "key %s -> \"%s\" %s\n", qPrintable(keyName),
                                       qPrintable(text.left(40)), qPrintable(chain.join(QLatin1String(" < "))));
                      } });
    }

    int at = 0;
    for (const Step &step : std::as_const(steps)) {
        at += step.delayMs;
        QTimer::singleShot(at, window, [=] {
            if (!step.shot.isEmpty())
                screenshot(step.shot);
            if (step.action)
                step.action();
        });
    }
    QTimer::singleShot(at + 100, window, [&qmlWarnings] {
        QCoreApplication::exit(qmlWarnings == 0 ? 0 : 2);
    });
}

QString dataDirectory(bool smokeTest, std::unique_ptr<QTemporaryDir> &tempDir)
{
    const QString overridden = qEnvironmentVariable("IPTV_DATA_DIR");
    if (!overridden.isEmpty())
        return overridden;
    if (smokeTest) {
        // Never touch the user's real data from a test run.
        tempDir = std::make_unique<QTemporaryDir>();
        return tempDir->path();
    }
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("IPTVPlayer"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("sadikbr.github.io"));
    QGuiApplication::setApplicationName(QStringLiteral("IPTV Player"));
    QGuiApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/appicon/app-icon-256.png")));
    // Wayland uses this to find the .desktop file (task bar icon and name).
    QGuiApplication::setDesktopFileName(QStringLiteral("io.github.sadikbr.iptvplayer"));

    // The UI is designed for the Material style. Don't let the platform pick
    // its native style (on Windows it is used by default and can't be
    // customised); QT_QUICK_CONTROLS_STYLE can still override this.
    if (!qEnvironmentVariableIsSet("QT_QUICK_CONTROLS_STYLE"))
        QQuickStyle::setStyle(QStringLiteral("Material"));

    // --smoke-test: load the UI, fail on any QML warning, then quit (used by CTest/CI).
    const bool smokeTest = app.arguments().contains(QStringLiteral("--smoke-test"));
    int qmlWarnings = 0;

    // Tests never ask GitHub for updates (unless pointed at a test server).
    if (smokeTest && !qEnvironmentVariableIsSet("IPTV_UPDATE_URL"))
        qputenv("IPTV_NO_UPDATE_CHECK", "1");

    std::unique_ptr<QTemporaryDir> tempDataDir;
    const QString dataDir = dataDirectory(smokeTest, tempDataDir);
    if (smokeTest || qEnvironmentVariableIsSet("IPTV_DATA_DIR")) {
        // Keep settings next to the test data instead of the user's real settings.
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dataDir);
    }
    QString dbError;
    if (!iptv::Database::initialize(QDir(dataDir).filePath(QStringLiteral("iptv.sqlite")),
                                    &dbError)) {
        std::fprintf(stderr, "Could not open the database in %s: %s\n", qPrintable(dataDir),
                     qPrintable(dbError));
        return 1;
    }

    NetworkFactory networkFactory;
    QQmlApplicationEngine engine;
    engine.setNetworkAccessManagerFactory(&networkFactory);
    QObject::connect(&engine, &QQmlEngine::warnings, &app, [&](const QList<QQmlError> &warnings) {
        for (const QQmlError &w : warnings) {
            // Broken channel logos are normal (provider content), not UI bugs.
            if (w.description().contains(QLatin1String("QQuickImage")))
                continue;
            std::fprintf(stderr, "QML warning: %s\n", qPrintable(w.toString()));
            ++qmlWarnings;
        }
    });
    QObject::connect(
            &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
            [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    engine.loadFromModule("IPTVPlayer", "Main");
#else
    engine.load(QUrl(QStringLiteral("qrc:/IPTVPlayer/Main.qml")));
#endif

    if (engine.rootObjects().isEmpty())
        return 1;

    // Arrow keys / TV remote move between buttons, cards and lists.
    if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst()))
        new SpatialNavigation(window);

    if (smokeTest)
        runSmokeTest(engine, qmlWarnings);

    return app.exec();
}
