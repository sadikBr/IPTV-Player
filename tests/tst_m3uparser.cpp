#include "M3uParser.h"

#include <QFile>
#include <QtTest>

using namespace iptv;

class TestM3uParser : public QObject
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
    void detectsPlaylists()
    {
        QVERIFY(M3uParser::looksLikeM3u(fixture("playlist.m3u")));
        QVERIFY(M3uParser::looksLikeM3u("\n  #EXTM3U\n"));
        QVERIFY(!M3uParser::looksLikeM3u("<html><body>Not found</body></html>"));
        QVERIFY(!M3uParser::looksLikeM3u("{\"user_info\":{}}"));
    }

    void parsesFixture()
    {
        const SourceContent content = M3uParser::parse(fixture("playlist.m3u"));

        QCOMPARE(content.epgUrl, QStringLiteral("http://epg.example.com/guide.xml.gz"));
        QCOMPARE(content.channels.size(), 9); // 6 live, 1 movie, 2 series

        const ChannelData &bbc = content.channels.at(0);
        QCOMPARE(bbc.name, QStringLiteral("BBC One HD"));
        QCOMPARE(bbc.epgId, QStringLiteral("bbc1.uk"));
        QCOMPARE(bbc.logo, QStringLiteral("http://logos.example.com/bbc1.png"));
        QCOMPARE(bbc.categoryExtId, QStringLiteral("UK | General"));
        QCOMPARE(bbc.url, QStringLiteral("http://example.com:8080/live/user/pass/1001.ts"));
        QCOMPARE(bbc.extId, bbc.url);
        QCOMPARE(bbc.kind, ContentKind::Live);
        QCOMPARE(bbc.number, 1);

        // Comma inside a quoted attribute, attributes in a different order, tvg-chno
        const ChannelData &cnn = content.channels.at(1);
        QCOMPARE(cnn.name, QStringLiteral("CNN International"));
        QCOMPARE(cnn.categoryExtId, QStringLiteral("News, World"));
        QCOMPARE(cnn.number, 42);
        QCOMPARE(cnn.url, QStringLiteral("http://example.com:8080/live/user/pass/1002.ts"));

        // #EXTGRP group
        QCOMPARE(content.channels.at(2).name, QStringLiteral("Channel Without Attributes"));
        QCOMPARE(content.channels.at(2).categoryExtId, QStringLiteral("Misc"));

        // Empty title falls back to tvg-name
        QCOMPARE(content.channels.at(3).name, QStringLiteral("Name From Attr"));

        // Kind from URL path; movie year from the name
        QCOMPARE(content.channels.at(4).kind, ContentKind::Movie);
        QCOMPARE(content.channels.at(4).number, 1);
        QCOMPARE(content.channels.at(4).year, QStringLiteral("2024"));

        // Series episodes are grouped into one item per show
        const ChannelData &show = content.channels.at(5);
        QCOMPARE(show.kind, ContentKind::Series);
        QCOMPARE(show.name, QStringLiteral("Some Show"));
        QCOMPARE(show.categoryExtId, QStringLiteral("Series | Drama"));
        QCOMPARE(content.channels.at(6).kind, ContentKind::Series);
        QCOMPARE(content.channels.at(6).name, QStringLiteral("Another Show Episode Without Numbers"));

        QCOMPARE(content.episodes.size(), 4);
        QCOMPARE(content.episodes.at(0).seriesExtId, show.extId);
        QCOMPARE(content.episodes.at(0).season, 1);
        QCOMPARE(content.episodes.at(0).episode, 1);
        QCOMPARE(content.episodes.at(1).episode, 2);
        QCOMPARE(content.episodes.at(1).title, QStringLiteral("The Second One"));
        QCOMPARE(content.episodes.at(1).image, QStringLiteral("http://logos.example.com/show.jpg"));
        QCOMPARE(content.episodes.at(2).seriesExtId, show.extId); // "Some.Show.S02E01"
        QCOMPARE(content.episodes.at(2).season, 2);
        QCOMPARE(content.episodes.at(3).season, 1);
        QCOMPARE(content.episodes.at(3).episode, 1);
        QCOMPARE(content.episodes.at(3).url, QStringLiteral("http://example.com:8080/series/user/pass/7101.mp4"));

        // URL without #EXTINF
        const ChannelData &bare = content.channels.at(7);
        QCOMPARE(bare.name, QStringLiteral("bare-url-without-extinf.ts"));
        QVERIFY(bare.categoryExtId.isEmpty());
        QCOMPARE(bare.kind, ContentKind::Live);

        // UTF-8 names
        QCOMPARE(content.channels.at(8).name, QStringLiteral("Café TV Élite"));

        // Categories in order of first appearance, per kind
        QCOMPARE(content.categories.size(), 5);
        QCOMPARE(content.categories.at(0).name, QStringLiteral("UK | General"));
        QCOMPARE(content.categories.at(1).name, QStringLiteral("News, World"));
        QCOMPARE(content.categories.at(2).name, QStringLiteral("Misc"));
        QCOMPARE(content.categories.at(3).kind, ContentKind::Movie);
        QCOMPARE(content.categories.at(4).kind, ContentKind::Series);
    }

    void handlesEmptyAndGarbage()
    {
        QVERIFY(M3uParser::parse(QByteArray()).channels.isEmpty());
        QVERIFY(M3uParser::parse("#EXTM3U\n").channels.isEmpty());
        // Unterminated quote must not crash or loop
        const auto c = M3uParser::parse("#EXTM3U\n#EXTINF:-1 tvg-name=\"broken,Name\nhttp://x/1.ts\n");
        QCOMPARE(c.channels.size(), 1);
    }

    void largePlaylistIsFast()
    {
        QByteArray data = "#EXTM3U\n";
        for (int i = 0; i < 50000; ++i) {
            data += "#EXTINF:-1 tvg-id=\"ch" + QByteArray::number(i)
                    + "\" tvg-logo=\"http://logo/x.png\" group-title=\"Group "
                    + QByteArray::number(i % 100) + "\",Channel " + QByteArray::number(i)
                    + "\nhttp://example.com/live/u/p/" + QByteArray::number(i) + ".ts\n";
        }
        QElapsedTimer timer;
        timer.start();
        const auto content = M3uParser::parse(data);
        QCOMPARE(content.channels.size(), 50000);
        QCOMPARE(content.categories.size(), 100);
        qDebug() << "Parsed 50k entries in" << timer.elapsed() << "ms";
    }
};

QTEST_GUILESS_MAIN(TestM3uParser)
#include "tst_m3uparser.moc"
