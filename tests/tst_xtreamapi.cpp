#include "XtreamApi.h"

#include <QFile>
#include <QUrlQuery>
#include <QtTest>

using namespace iptv;

class TestXtreamApi : public QObject
{
    Q_OBJECT

private:
    static QByteArray fixture(const char *name)
    {
        QFile f(QStringLiteral(FIXTURES_DIR "/") + QLatin1String(name));
        if (!f.open(QIODevice::ReadOnly))
            qFatal("Missing fixture %s", name);
        return f.readAll();
    }

private slots:
    void normalizeServer_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("expected");
        QTest::newRow("bare host") << "example.com:8080" << "http://example.com:8080";
        QTest::newRow("trailing slash") << "http://example.com:8080/" << "http://example.com:8080";
        QTest::newRow("player_api") << "http://example.com:8080/player_api.php?username=a"
                                    << "http://example.com:8080";
        QTest::newRow("get.php") << "https://tv.example.com/get.php?username=a&password=b"
                                 << "https://tv.example.com";
        QTest::newRow("prefix") << "http://example.com/iptv/" << "http://example.com/iptv";
        QTest::newRow("spaces") << "  http://example.com  " << "http://example.com";
        QTest::newRow("empty") << "" << "";
        QTest::newRow("ftp") << "ftp://example.com" << "";
    }

    void normalizeServer()
    {
        QFETCH(QString, input);
        QFETCH(QString, expected);
        const QUrl url = XtreamApi::normalizeServer(input);
        QCOMPARE(url.isValid() ? url.toString() : QString(), expected);
    }

    void parseCredentialsUrl()
    {
        XtreamCredentials creds;
        QVERIFY(XtreamApi::parseCredentialsUrl(
                QStringLiteral("http://example.com:8080/get.php?username=john&password=s%26cret&type=m3u_plus"),
                &creds));
        QCOMPARE(creds.server.toString(), QStringLiteral("http://example.com:8080"));
        QCOMPARE(creds.username, QStringLiteral("john"));
        QCOMPARE(creds.password, QStringLiteral("s&cret"));

        QVERIFY(!XtreamApi::parseCredentialsUrl(QStringLiteral("http://example.com/list.m3u"), nullptr));
    }

    void apiUrlEncodesCredentials()
    {
        const XtreamCredentials creds{ QUrl(QStringLiteral("http://example.com:8080")),
                                       QStringLiteral("john doe"), QStringLiteral("p&ss+word") };
        const QUrl url = XtreamApi::apiUrl(creds, QStringLiteral("get_live_streams"));
        QCOMPARE(url.path(), QStringLiteral("/player_api.php"));
        const QUrlQuery query(url);
        QCOMPARE(query.queryItemValue(QStringLiteral("username"), QUrl::FullyDecoded),
                 QStringLiteral("john doe"));
        QCOMPARE(query.queryItemValue(QStringLiteral("password"), QUrl::FullyDecoded),
                 QStringLiteral("p&ss+word"));
        QCOMPARE(query.queryItemValue(QStringLiteral("action")), QStringLiteral("get_live_streams"));
    }

    void liveStreamUrl()
    {
        const XtreamCredentials creds{ QUrl(QStringLiteral("http://example.com:8080")),
                                       QStringLiteral("john"), QStringLiteral("pa/ss") };
        QCOMPARE(XtreamApi::liveStreamUrl(creds, QStringLiteral("1001"), QStringLiteral("m3u8"))
                         .toString(QUrl::FullyEncoded),
                 QStringLiteral("http://example.com:8080/live/john/pa%2Fss/1001.m3u8"));
        QCOMPARE(XtreamApi::liveStreamUrl(creds, QStringLiteral("7"), QString()).toString(),
                 QStringLiteral("http://example.com:8080/live/john/pa%2Fss/7.ts"));
    }

    void parseAccount()
    {
        XtreamAccount account;
        QString error;
        QVERIFY(XtreamApi::parseAccount(fixture("xtream_account.json"), &account, &error));
        QVERIFY(account.authenticated);
        QCOMPARE(account.status, QStringLiteral("Active"));
        QCOMPARE(account.maxConnections, 2);
        QCOMPARE(account.expiresAt.toSecsSinceEpoch(), 1798761600);
        QCOMPARE(account.allowedFormats, QStringList({ "m3u8", "ts", "rtmp" }));
    }

    void parseAccountFailures()
    {
        XtreamAccount account;
        QString error;
        QVERIFY(!XtreamApi::parseAccount(fixture("xtream_auth_failed.json"), &account, &error));
        QVERIFY(error.contains(QStringLiteral("username")));

        error.clear();
        QVERIFY(!XtreamApi::parseAccount("[]", &account, &error));
        QVERIFY(!error.isEmpty());

        error.clear();
        QVERIFY(!XtreamApi::parseAccount("<html>nginx</html>", &account, &error));
        QVERIFY(error.contains(QStringLiteral("server address")));
    }

    void parseCategories()
    {
        QList<CategoryData> categories;
        QString error;
        QVERIFY(XtreamApi::parseCategories(fixture("xtream_live_categories.json"),
                                           ContentKind::Live, &categories, &error));
        QCOMPARE(categories.size(), 3); // the one without id is skipped
        QCOMPARE(categories.at(0).extId, QStringLiteral("1"));
        QCOMPARE(categories.at(1).extId, QStringLiteral("2")); // numeric id
        QCOMPARE(categories.at(2).name, QStringLiteral("Category 3"));
    }

    void parseLiveStreams()
    {
        QList<ChannelData> channels;
        QString error;
        QVERIFY(XtreamApi::parseLiveStreams(fixture("xtream_live_streams.json"), &channels, &error));
        QCOMPARE(channels.size(), 4); // the one without stream_id is skipped

        QCOMPARE(channels.at(0).extId, QStringLiteral("1001"));
        QCOMPARE(channels.at(0).number, 1);
        QCOMPARE(channels.at(0).epgId, QStringLiteral("bbc1.uk"));
        QCOMPARE(channels.at(0).categoryExtId, QStringLiteral("1"));
        QVERIFY(channels.at(0).url.isEmpty());

        QCOMPARE(channels.at(1).extId, QStringLiteral("1002")); // string id
        QCOMPARE(channels.at(1).number, 2);                     // string num
        QVERIFY(channels.at(1).epgId.isEmpty());                // null

        QCOMPARE(channels.at(2).number, 3);                         // missing num -> position
        QCOMPARE(channels.at(2).categoryExtId, QStringLiteral("2")); // category_ids fallback
        QVERIFY(channels.at(2).logo.isEmpty());

        QCOMPARE(channels.at(3).name, QStringLiteral("Channel 1004"));
    }
};

QTEST_GUILESS_MAIN(TestXtreamApi)
#include "tst_xtreamapi.moc"
