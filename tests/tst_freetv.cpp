#include "FreeTv.h"
#include "M3uParser.h"

#include <QFile>
#include <QTest>

using namespace iptv;

class TstFreeTv : public QObject
{
    Q_OBJECT

    static QByteArray fixture(const char *name)
    {
        QFile f(QStringLiteral(FIXTURES_DIR "/") + QLatin1String(name));
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    }

private slots:
    void urls()
    {
        QCOMPARE(FreeTv::allChannelsUrl().toString(), QStringLiteral("https://iptv-org.github.io/iptv/index.m3u"));
        QCOMPARE(FreeTv::countryUrl(QStringLiteral("FR")).toString(),
                 QStringLiteral("https://iptv-org.github.io/iptv/countries/fr.m3u"));
        QCOMPARE(FreeTv::categoryUrl(QStringLiteral("news")).toString(),
                 QStringLiteral("https://iptv-org.github.io/iptv/categories/news.m3u"));
    }

    void countries()
    {
        const auto list = FreeTv::parseCountries(fixture("iptvorg_countries.json"));
        QCOMPARE(list.size(), 5);
        QCOMPARE(list.first().name, QStringLiteral("Aland")); // sorted by name
        QCOMPARE(list.first().code, QStringLiteral("AX"));
        QVERIFY(!list.first().flag.isEmpty());
    }

    void categories()
    {
        const auto list = FreeTv::parseCategories(fixture("iptvorg_categories.json"));
        QCOMPARE(list.size(), 3); // the empty adult category is left out
        QCOMPARE(list.first().code, QStringLiteral("animation"));
    }

    void realPlaylist()
    {
        // A sample of the real iptv-org playlist: user-agent options between
        // the channel and its link must not break parsing.
        const SourceContent c = M3uParser::parse(fixture("iptvorg_sample.m3u"));
        QCOMPARE(c.channels.size(), 6);
        QCOMPARE(c.channels.at(1).name, QStringLiteral("1+1 International"));
        QVERIFY(c.channels.at(1).url.startsWith(QStringLiteral("http://158.101.222.193")));
        QCOMPARE(c.channels.at(1).categoryExtId, QStringLiteral("General"));
    }

    void multiCategoryGroups()
    {
        const SourceContent c = M3uParser::parse(
                "#EXTM3U\n#EXTINF:-1 group-title=\"Animation;Kids\",Toons\nhttp://x/1.m3u8\n"
                "#EXTINF:-1 group-title=\"FR; Sport\",Sport\nhttp://x/2.m3u8\n");
        QCOMPARE(c.channels.at(0).categoryExtId, QStringLiteral("Animation"));
        QCOMPARE(c.channels.at(1).categoryExtId, QStringLiteral("FR; Sport")); // a provider's own name
    }
};

QTEST_MAIN(TstFreeTv)
#include "tst_freetv.moc"
