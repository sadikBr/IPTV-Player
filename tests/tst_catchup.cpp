#include "Catchup.h"

#include <QTest>

using namespace iptv;

class TstCatchup : public QObject
{
    Q_OBJECT

private:
    static QDateTime utc(const char *text)
    {
        QDateTime d = QDateTime::fromString(QString::fromLatin1(text), Qt::ISODate);
        d.setTimeZone(QTimeZone::utc());
        return d;
    }

private slots:
    void availability()
    {
        const QDateTime now = utc("2026-11-07T20:00:00");
        QVERIFY(Catchup::isAvailable(3, utc("2026-11-07T18:00:00"), now));
        QVERIFY(Catchup::isAvailable(3, utc("2026-11-05T20:30:00"), now));  // 2.98 days ago
        QVERIFY(!Catchup::isAvailable(3, utc("2026-11-04T19:00:00"), now)); // too old
        QVERIFY(!Catchup::isAvailable(3, utc("2026-11-07T21:00:00"), now)); // not started
        QVERIFY(!Catchup::isAvailable(0, utc("2026-11-07T18:00:00"), now)); // no archive
        // A program that is on now can be watched from its start.
        QVERIFY(Catchup::isAvailable(1, utc("2026-11-07T19:45:00"), now));
    }

    void serverZone()
    {
        QCOMPARE(Catchup::serverZone(QStringLiteral("Europe/Paris"), 0, {}).id(),
                 QByteArray("Europe/Paris"));
        // No (usable) name: offset from time_now vs timestamp_now (UTC+1, read 3 s apart).
        const QTimeZone fromOffset = Catchup::serverZone(
                QStringLiteral("Not/AZone"), 1794081600, QStringLiteral("2026-11-07 21:00:03"));
        QCOMPARE(fromOffset.offsetFromUtc(QDateTime::currentDateTimeUtc()), 3600);
        QCOMPARE(Catchup::serverZone({}, 0, {}), QTimeZone::utc());
    }

    void xtreamUrl()
    {
        const QUrl url = Catchup::xtreamUrl(QUrl(QStringLiteral("http://example.com:8080")),
                                            QStringLiteral("john"), QStringLiteral("p@ss"),
                                            QStringLiteral("1001"), utc("2026-11-07T19:00:00"),
                                            utc("2026-11-07T19:30:00"),
                                            QTimeZone(QByteArray("Europe/Paris")));
        // 19:00 UTC is 20:00 in Paris (winter time); 30 min + 5 min margin
        QCOMPARE(url.toString(QUrl::FullyEncoded),
                 QStringLiteral("http://example.com:8080/timeshift/john/p%40ss/35/2026-11-07:20-00/1001.ts"));
    }

    void m3uUrl_data()
    {
        QTest::addColumn<QString>("stream");
        QTest::addColumn<QString>("type");
        QTest::addColumn<QString>("source");
        QTest::addColumn<QString>("expected");
        // start 2026-11-07 19:00:00 UTC = 1794078000, end +1800, now 21:00 = 1794085200
        QTest::newRow("default full url")
                << "http://a.tv/live/1.m3u8" << "default"
                << "http://a.tv/archive/1.m3u8?start={utc}&end={utcend}"
                << "http://a.tv/archive/1.m3u8?start=1794078000&end=1794079800";
        QTest::newRow("default query")
                << "http://a.tv/live/1.m3u8?token=x" << "default" << "?utc={utc}&lutc={lutc}"
                << "http://a.tv/live/1.m3u8?token=x&utc=1794078000&lutc=1794085200";
        QTest::newRow("append")
                << "http://a.tv/live/1.m3u8" << "append" << "?utc=${start}&dur={duration:60}"
                << "http://a.tv/live/1.m3u8?utc=1794078000&dur=30";
        QTest::newRow("date parts")
                << "http://a.tv/1" << "default" << "http://a.tv/rec/{Y}/{m}/{d}/{H}-{M}.ts"
                << "http://a.tv/rec/2026/11/07/19-00.ts";
        QTest::newRow("shift")
                << "http://a.tv/live/1.ts" << "shift" << ""
                << "http://a.tv/live/1.ts?utc=1794078000&lutc=1794085200";
        QTest::newRow("flussonic hls")
                << "http://f.tv/ch1/index.m3u8?token=x" << "flussonic" << ""
                << "http://f.tv/ch1/index-1794078000-1800.m3u8?token=x";
        QTest::newRow("flussonic ts")
                << "http://f.tv/ch1/mpegts" << "fs" << ""
                << "http://f.tv/ch1/timeshift_abs-1794078000.ts";
        QTest::newRow("xc")
                << "http://x.tv:8080/live/john/pw/1001.ts" << "xc" << ""
                << "http://x.tv:8080/timeshift/john/pw/35/2026-11-07:19-00/1001.ts";
        QTest::newRow("unknown type") << "http://a.tv/1" << "vod" << "" << "";
        QTest::newRow("default without source") << "http://a.tv/1" << "default" << "" << "";
    }
    void m3uUrl()
    {
        QFETCH(QString, stream);
        QFETCH(QString, type);
        QFETCH(QString, source);
        QFETCH(QString, expected);
        const QUrl url = Catchup::m3uUrl(stream, type, source, utc("2026-11-07T19:00:00"),
                                         utc("2026-11-07T19:30:00"), utc("2026-11-07T21:00:00"));
        QCOMPARE(url.toString(), expected);
    }
};

QTEST_MAIN(TstCatchup)
#include "tst_catchup.moc"
