#include "RadioBrowser.h"

#include <QFile>
#include <QTest>
#include <QUrlQuery>

using namespace iptv;

class TstRadioBrowser : public QObject
{
    Q_OBJECT

private slots:
    void stations()
    {
        QFile f(QStringLiteral(FIXTURES_DIR "/radiobrowser_stations.json"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const auto list = RadioBrowser::parseStations(f.readAll());
        QCOMPARE(list.size(), 4);
        const RadioStation &s = list.first();
        QCOMPARE(s.name, QStringLiteral("Classic Vinyl HD"));
        QVERIFY(s.url.startsWith(QStringLiteral("http")));
        QCOMPARE(s.codec, QStringLiteral("MP3"));
        QCOMPARE(s.bitrate, 320);
        QCOMPARE(s.countryCode, QStringLiteral("US"));
        QVERIFY(s.tags.contains(QStringLiteral("big band")));
        QVERIFY(!s.uuid.isEmpty());
        // No url_resolved: the plain url is used; no favicon is fine
        QVERIFY(!list.last().url.isEmpty());
        QVERIFY(list.last().favicon.isEmpty());
        QVERIFY(RadioBrowser::parseStations("not json").isEmpty());
    }

    void servers()
    {
        const QStringList servers = RadioBrowser::parseServers(
                R"([{"ip":"1.2.3.4","name":"de1.api.radio-browser.info"},{"ip":"::1","name":"de1.api.radio-browser.info"},{"ip":"5.6.7.8","name":"evil.example.com"}])");
        QCOMPARE(servers, QStringList { QStringLiteral("de1.api.radio-browser.info") });
        QVERIFY(!RadioBrowser::fallbackServers().isEmpty());
    }

    void urls()
    {
        const QUrl url = RadioBrowser::searchUrl(QStringLiteral("de1.api.radio-browser.info"),
                                                 QStringLiteral(" jazz fm "), QStringLiteral("FR"),
                                                 QStringLiteral("jazz"), 50);
        QCOMPARE(url.host(), QStringLiteral("de1.api.radio-browser.info"));
        QCOMPARE(url.path(), QStringLiteral("/json/stations/search"));
        const QUrlQuery q(url);
        QCOMPARE(q.queryItemValue(QStringLiteral("name")), QStringLiteral("jazz fm"));
        QCOMPARE(q.queryItemValue(QStringLiteral("countrycode")), QStringLiteral("FR"));
        QCOMPARE(q.queryItemValue(QStringLiteral("tag")), QStringLiteral("jazz"));
        QCOMPARE(q.queryItemValue(QStringLiteral("hidebroken")), QStringLiteral("true"));
        QCOMPARE(RadioBrowser::clickUrl(QStringLiteral("de1.api.radio-browser.info"), QStringLiteral("abc")).toString(),
                 QStringLiteral("https://de1.api.radio-browser.info/json/url/abc"));
    }
};

QTEST_MAIN(TstRadioBrowser)
#include "tst_radiobrowser.moc"
