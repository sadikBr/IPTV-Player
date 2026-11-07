#include "player/PlayerController.h"

#include <QSignalSpy>
#include <QTest>

// A backend driven by the test: it records what the controller asks for and
// changes state only when the test says so.
class FakeBackend : public PlayerBackend
{
    Q_OBJECT
public:
    using PlayerBackend::PlayerBackend;

    QString name() const override { return QStringLiteral("fake"); }
    void setVideoOutput(QObject *) override {}
    bool supportsLowLatency() const override { return true; }
    void setLowLatency(bool on) override { lowLatency = on; }
    void setSource(const QUrl &url) override
    {
        source = url;
        if (!url.isEmpty())
            ++sourceCount;
        setState(State::Stopped);
    }
    void play() override
    {
        ++playCount;
        setState(State::Loading);
    }
    void pause() override { setState(State::Paused); }
    void stop() override { setState(State::Stopped); }
    void setVolume(qreal) override {}
    void setMuted(bool) override {}
    State state() const override { return m_state; }
    qreal bufferProgress() const override { return 1; }
    qint64 position() const override { return pos; }
    qint64 duration() const override { return dur; }
    bool seekable() const override { return dur > 0; }
    void seek(qint64 positionMs) override
    {
        lastSeek = positionMs;
        pos = positionMs;
    }
    QString streamInfo() const override { return {}; }
    QStringList audioTracks() const override { return {}; }
    QStringList subtitleTracks() const override { return {}; }
    int activeAudioTrack() const override { return 0; }
    int activeSubtitleTrack() const override { return -1; }
    void setActiveAudioTrack(int) override {}
    void setActiveSubtitleTrack(int) override {}

    void setState(State s)
    {
        if (s == m_state)
            return;
        m_state = s;
        emit stateChanged();
    }
    void fail()
    {
        m_state = State::Error;
        emit stateChanged();
        emit errorOccurred(QStringLiteral("Connection reset"));
    }
    void startPlaying()
    {
        setState(State::Playing);
        emit durationChanged();
        emit seekableChanged();
    }

    QUrl source;
    int sourceCount = 0;
    int playCount = 0;
    qint64 pos = 0;
    qint64 dur = 0;
    qint64 lastSeek = -1;
    bool lowLatency = false;

private:
    State m_state = State::Stopped;
};

class TstPlayerController : public QObject
{
    Q_OBJECT

private:
    struct Setup
    {
        FakeBackend *backend;
        PlayerController *player;
    };
    Setup make(QObject *parent, int stallMs = 300)
    {
        auto *backend = new FakeBackend;
        auto *player = new PlayerController(backend, parent);
        player->setReconnectTiming(stallMs, 100000, { 50, 50, 50 });
        return { backend, player };
    }
    const QUrl url { QStringLiteral("http://example.com/live/1.ts") };

private slots:
    void liveErrorAfterPlayingReconnects()
    {
        QObject owner;
        auto [backend, player] = make(&owner);
        player->open(url);
        backend->startPlaying();
        QCOMPARE(backend->sourceCount, 1);

        backend->fail();
        QVERIFY(player->isReconnecting());
        QVERIFY(!player->hasFailed());
        QVERIFY(player->isBusy());

        QTRY_COMPARE(backend->sourceCount, 2);
        QCOMPARE(backend->source, url);
        backend->startPlaying();
        QVERIFY(!player->isReconnecting());
        QVERIFY(player->isPlaying());
    }

    void errorBeforePlayingFailsRightAway()
    {
        QObject owner;
        auto [backend, player] = make(&owner);
        player->open(url);
        backend->fail();
        QVERIFY(!player->isReconnecting());
        QVERIFY(player->hasFailed());
        QTest::qWait(200);
        QCOMPARE(backend->sourceCount, 1);
    }

    void liveEndIsADrop()
    {
        QObject owner;
        auto [backend, player] = make(&owner);
        QSignalSpy ended(player, &PlayerController::ended);
        player->open(url);
        backend->startPlaying();
        emit backend->endOfMedia();
        QCOMPARE(ended.count(), 0);
        QVERIFY(player->isReconnecting());
        QTRY_COMPARE(backend->sourceCount, 2);
    }

    void movieEndIsTheEnd()
    {
        QObject owner;
        auto [backend, player] = make(&owner);
        QSignalSpy ended(player, &PlayerController::ended);
        player->setVod(true);
        backend->dur = 600000;
        player->open(url);
        backend->startPlaying();
        backend->pos = 599000;
        QTest::qWait(1200); // let the watchdog see the position
        emit backend->endOfMedia();
        QCOMPARE(ended.count(), 1);
        QVERIFY(!player->isReconnecting());
    }

    void movieDropResumesWhereItWas()
    {
        QObject owner;
        auto [backend, player] = make(&owner);
        player->setVod(true);
        backend->dur = 3600000;
        player->open(url);
        backend->startPlaying();
        backend->pos = 1200000;
        QTest::qWait(1200);

        backend->fail();
        QVERIFY(player->isReconnecting());
        QTRY_COMPARE(backend->sourceCount, 2);
        backend->pos = 0;
        backend->startPlaying();
        QCOMPARE(backend->lastSeek, 1200000);
    }

    void stuckBufferingReconnects()
    {
        QObject owner;
        auto [backend, player] = make(&owner, 1500);
        player->open(url);
        backend->startPlaying();
        backend->setState(PlayerBackend::State::Buffering);
        QTRY_COMPARE_WITH_TIMEOUT(backend->sourceCount, 2, 5000);
    }

    void givesUpAfterAllAttempts()
    {
        QObject owner;
        auto [backend, player] = make(&owner);
        player->open(url);
        backend->startPlaying();
        for (int i = 0; i < 3; ++i) {
            backend->fail();
            QTRY_COMPARE(backend->sourceCount, i + 2);
        }
        backend->fail();
        QVERIFY(!player->isReconnecting());
        QVERIFY(player->hasFailed());
        QVERIFY(!player->errorString().isEmpty());

        // Opening again starts over.
        player->open(url);
        QVERIFY(!player->hasFailed());
    }

    void reconnectCanBeTurnedOff()
    {
        QObject owner;
        auto [backend, player] = make(&owner);
        player->setAutoReconnect(false);
        player->open(url);
        backend->startPlaying();
        backend->fail();
        QVERIFY(!player->isReconnecting());
        QVERIFY(player->hasFailed());
        QTest::qWait(200);
        QCOMPARE(backend->sourceCount, 1);
    }

    void lowLatencyOnlyForLiveTv()
    {
        QObject owner;
        auto [backend, player] = make(&owner);
        player->setLowLatency(true);
        player->open(url);
        QVERIFY(backend->lowLatency);
        player->setVod(true);
        player->open(QUrl(QStringLiteral("http://example.com/movie/1.mp4")));
        QVERIFY(!backend->lowLatency);
    }

    void pauseCancelsReconnect()
    {
        QObject owner;
        auto [backend, player] = make(&owner);
        player->open(url);
        backend->startPlaying();
        backend->fail();
        QVERIFY(player->isReconnecting());
        player->stop();
        QVERIFY(!player->isReconnecting());
        QTest::qWait(200);
        QCOMPARE(backend->sourceCount, 1);
    }
};

QTEST_MAIN(TstPlayerController)
#include "tst_playercontroller.moc"
