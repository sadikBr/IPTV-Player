#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QDir>
#include <QImage>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <cstdio>

namespace {

// Loads the home page, then opens the player on an unreachable stream so the
// player page and its error state are exercised too. Exits with 0 if no QML
// warnings were printed. If IPTV_SCREENSHOT_DIR is set, screenshots are saved.
void runSmokeTest(QQmlApplicationEngine &engine, const int &qmlWarnings)
{
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    const QString shotDir = qEnvironmentVariable("IPTV_SCREENSHOT_DIR");
    auto screenshot = [window, shotDir](const QString &name) {
        if (!shotDir.isEmpty() && window)
            window->grabWindow().save(QDir(shotDir).filePath(name + QStringLiteral(".png")));
    };

    const QString smokeUrl = qEnvironmentVariable("IPTV_SMOKE_URL",
                                                  QStringLiteral("http://127.0.0.1:9/test.ts"));

    QTimer::singleShot(500, window, [=] {
        screenshot(QStringLiteral("home"));
        QMetaObject::invokeMethod(window, "openStream",
                                  Q_ARG(QVariant, QUrl(smokeUrl)),
                                  Q_ARG(QVariant, QStringLiteral("Smoke test channel")));
    });
    QTimer::singleShot(2500, window, [=, &qmlWarnings] {
        screenshot(QStringLiteral("player"));
        QCoreApplication::exit(qmlWarnings == 0 ? 0 : 2);
    });
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("IPTVPlayer"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("sadikbr.github.io"));
    QGuiApplication::setApplicationName(QStringLiteral("IPTV Player"));
    QGuiApplication::setApplicationVersion(QStringLiteral(APP_VERSION));

    if (QQuickStyle::name().isEmpty())
        QQuickStyle::setStyle(QStringLiteral("Material"));

    // --smoke-test: load the UI, fail on any QML warning, then quit (used by CTest/CI).
    const bool smokeTest = app.arguments().contains(QStringLiteral("--smoke-test"));
    int qmlWarnings = 0;

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlEngine::warnings, &app, [&](const QList<QQmlError> &warnings) {
        for (const QQmlError &w : warnings)
            std::fprintf(stderr, "QML warning: %s\n", qPrintable(w.toString()));
        qmlWarnings += int(warnings.size());
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

    if (smokeTest)
        runSmokeTest(engine, qmlWarnings);

    return app.exec();
}
