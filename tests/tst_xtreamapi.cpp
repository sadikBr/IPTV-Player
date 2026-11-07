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

    void vodUrls()
    {
        const XtreamCredentials creds{ QUrl(QStringLiteral("http://example.com:8080")),
                                       QStringLiteral("john"), QStringLiteral("secret") };
        QCOMPARE(XtreamApi::movieUrl(creds, QStringLiteral("5001"), QStringLiteral("mkv")).toString(),
                 QStringLiteral("http://example.com:8080/movie/john/secret/5001.mkv"));
        QCOMPARE(XtreamApi::episodeUrl(creds, QStringLiteral("9011"), QString()).toString(),
                 QStringLiteral("http://example.com:8080/series/john/secret/9011.mp4"));
        QCOMPARE(XtreamApi::epgUrl(creds).toString(),
                 QStringLiteral("http://example.com:8080/xmltv.php?username=john&password=secret"));
    }

    void parseMovies()
    {
        QList<ChannelData> movies;
        QString error;
        QVERIFY(XtreamApi::parseMovies(fixture("xtream_vod_streams.json"), &movies, &error));
        QCOMPARE(movies.size(), 2);
        QCOMPARE(movies.at(0).kind, ContentKind::Movie);
        QCOMPARE(movies.at(0).extId, QStringLiteral("5001"));
        QCOMPARE(movies.at(0).extension, QStringLiteral("mkv"));
        QCOMPARE(movies.at(0).rating, QStringLiteral("7.4"));
        QCOMPARE(movies.at(0).year, QStringLiteral("2023")); // from the name
        QCOMPARE(movies.at(0).categoryExtId, QStringLiteral("10"));
        QCOMPARE(movies.at(1).rating, QString());             // 0 = unrated
        QCOMPARE(movies.at(1).year, QStringLiteral("2021"));

        QVERIFY(XtreamApi::parseMovies("{}", &movies, &error)); // empty on some servers
        QVERIFY(movies.isEmpty());
    }

    void parseSeries()
    {
        QList<ChannelData> series;
        QString error;
        QVERIFY(XtreamApi::parseSeries(fixture("xtream_series.json"), &series, &error));
        QCOMPARE(series.size(), 2);
        QCOMPARE(series.at(0).kind, ContentKind::Series);
        QCOMPARE(series.at(0).extId, QStringLiteral("701"));
        QCOMPARE(series.at(0).logo, QStringLiteral("http://img.example.com/s1.jpg"));
        QCOMPARE(series.at(0).year, QStringLiteral("2019"));
        QCOMPARE(series.at(0).genre, QStringLiteral("Crime"));
        QCOMPARE(series.at(0).plot, QStringLiteral("Two detectives."));
        QCOMPARE(series.at(1).year, QStringLiteral("2015")); // release_date
    }

    void parseMovieInfo()
    {
        VodDetails d;
        QString error;
        QVERIFY(XtreamApi::parseMovieInfo(fixture("xtream_vod_info.json"), &d, &error));
        QCOMPARE(d.plot, QStringLiteral("A storm hits the town."));
        QCOMPARE(d.director, QStringLiteral("Sam Lee"));
        QCOMPARE(d.durationSecs, 6300);
        QCOMPARE(d.backdrop, QStringLiteral("http://img.example.com/b1.jpg"));
        QCOMPARE(d.extension, QStringLiteral("mkv"));
        QCOMPARE(d.releaseDate, QStringLiteral("2023-10-01"));
    }

    void parseSeriesInfo()
    {
        VodDetails d;
        QList<EpisodeData> episodes;
        QString error;
        QVERIFY(XtreamApi::parseSeriesInfo(fixture("xtream_series_info.json"), QStringLiteral("701"),
                                           &d, &episodes, &error));
        QCOMPARE(d.backdrop, QStringLiteral("http://img.example.com/b1.jpg")); // string form
        QCOMPARE(episodes.size(), 3);
        // Sorted by season, then episode
        QCOMPARE(episodes.at(0).title, QStringLiteral("Pilot"));
        QCOMPARE(episodes.at(0).extId, QStringLiteral("9011"));
        QCOMPARE(episodes.at(0).seriesExtId, QStringLiteral("701"));
        QCOMPARE(episodes.at(1).episode, 2);
        QCOMPARE(episodes.at(1).durationSecs, 44 * 60 + 10);
        QCOMPARE(episodes.at(2).season, 2);
        QCOMPARE(episodes.at(2).image, QStringLiteral("http://img.example.com/e21.jpg"));

        // Array-of-arrays variant
        QVERIFY(XtreamApi::parseSeriesInfo(fixture("xtream_series_info_array.json"),
                                           QStringLiteral("702"), &d, &episodes, &error));
        QCOMPARE(episodes.size(), 2);
        QCOMPARE(episodes.at(1).title, QStringLiteral("Two"));
    }
};

QTEST_GUILESS_MAIN(TestXtreamApi)
#include "tst_xtreamapi.moc"
