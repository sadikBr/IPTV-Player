// Plays a real stream through Qt Multimedia from a local server that drops
// the connection, and checks that the player reconnects by itself.
// Needs python3 and ffmpeg; runs only with IPTV_STREAM_TEST=1 (CI), because
// old Qt Multimedia versions (6.4) don't notice dropped streams at all.
#include "player/PlayerController.h"

#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QVideoSink>

class TstStreamReconnect : public QObject
{
    Q_OBJECT

private slots:
    void reconnectsAfterDrops()
    {
        if (qEnvironmentVariableIntValue("IPTV_STREAM_TEST") != 1)
            QSKIP("Set IPTV_STREAM_TEST=1 to run (needs python3 and ffmpeg).");
        const QString python = QStandardPaths::findExecutable(QStringLiteral("python3"));
        const QString ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
        if (python.isEmpty() || ffmpeg.isEmpty())
            QSKIP("python3 or ffmpeg missing");

        QTemporaryDir dir;
        const QString ts = dir.filePath(QStringLiteral("live.ts"));
        QCOMPARE(QProcess::execute(ffmpeg, {
                     QStringLiteral("-loglevel"), QStringLiteral("error"), QStringLiteral("-y"),
                     QStringLiteral("-f"), QStringLiteral("lavfi"),
                     QStringLiteral("-i"), QStringLiteral("testsrc=size=320x180:rate=25"),
                     QStringLiteral("-t"), QStringLiteral("60"),
                     QStringLiteral("-c:v"), QStringLiteral("libx264"),
                     QStringLiteral("-b:v"), QStringLiteral("300k"),
                     QStringLiteral("-g"), QStringLiteral("25"),
                     QStringLiteral("-f"), QStringLiteral("mpegts"), ts }), 0);

        // Two drops, then a good connection.
        QProcess server;
        server.setProcessChannelMode(QProcess::ForwardedErrorChannel);
        server.start(python, { QStringLiteral(STREAM_SERVER), ts, QStringLiteral("60"),
                               QStringLiteral("2") });
        QVERIFY(server.waitForReadyRead(10000));
        const QByteArray first = server.readLine().trimmed();
        QVERIFY2(first.startsWith("port "), first.constData());
        const int port = first.mid(5).toInt();
        int connections = 0;
        connect(&server, &QProcess::readyReadStandardOutput, this, [&] {
            while (server.canReadLine()) {
                const QByteArray line = server.readLine().trimmed();
                qInfo() << "server:" << line;
                if (line.startsWith("connection"))
                    ++connections;
            }
        });

        QVideoSink sink;
        PlayerController player;
        player.setVideoOutput(&sink);
        bool sawReconnecting = false;
        connect(&player, &PlayerController::stateChanged, this, [&] {
            qInfo() << "player: playing" << player.isPlaying() << "busy" << player.isBusy()
                    << "reconnecting" << player.isReconnecting() << "attempt"
                    << player.reconnectAttempt() << "failed" << player.hasFailed()
                    << "position" << player.position();
            if (player.isReconnecting())
                sawReconnecting = true;
        });
        player.open(QUrl(QStringLiteral("http://127.0.0.1:%1/live/1.ts").arg(port)));

        QTRY_VERIFY_WITH_TIMEOUT(player.isPlaying(), 15000);
        QTRY_VERIFY_WITH_TIMEOUT(connections >= 3, 60000);
        QVERIFY(sawReconnecting);
        QTRY_VERIFY_WITH_TIMEOUT(player.isPlaying() && !player.isReconnecting(), 20000);
        // Still playing a while later: the good connection stays up.
        QTest::qWait(5000);
        QVERIFY(player.isPlaying());
        QVERIFY(!player.hasFailed());
        QCOMPARE(connections, 3);

        player.stop();
        server.kill();
        server.waitForFinished();
    }
};

QTEST_MAIN(TstStreamReconnect)
#include "tst_streamreconnect.moc"
