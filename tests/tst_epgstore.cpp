#include "Database.h"
#include "EpgStore.h"
#include "SourceStore.h"

#include <QBuffer>
#include <QFile>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

using namespace iptv;

class TestEpgStore : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    int m_sourceId = -1;

    static qint64 utc(int y, int mo, int d, int h, int mi)
    {
        return QDateTime(QDate(y, mo, d), QTime(h, mi), QTimeZone::utc()).toSecsSinceEpoch();
    }

    static ChannelData channel(const QString &extId, const QString &name, const QString &epgId)
    {
        ChannelData c;
        c.extId = extId;
        c.name = name;
        c.epgId = epgId;
        return c;
    }

    static SourceContent content()
    {
        SourceContent c;
        c.channels = {
            channel("1", "BBC One HD", "bbc1.uk"),     // id differs only in case
            channel("2", "Café TV Élite", ""),         // matched by name
            channel("3", "CNN", "cnn.us"),
            channel("4", "Nothing", "unknown.id"),     // no guide
            channel("5", "Orphan", "orphan.id"),       // programmes without <channel>
        };
        return c;
    }

    static QString epgKeyOf(const QString &extId)
    {
        QSqlQuery q(Database::connection());
        q.prepare(QStringLiteral("SELECT epg_key FROM channels WHERE ext_id = ?"));
        q.addBindValue(extId);
        return q.exec() && q.next() ? q.value(0).toString() : QStringLiteral("<missing>");
    }

    bool importFixture(const char *name, int *count)
    {
        QFile file(QStringLiteral(FIXTURES_DIR "/") + QLatin1String(name));
        if (!file.open(QIODevice::ReadOnly))
            return false;
        EpgStore store(Database::connection());
        const bool ok = store.importGuide(m_sourceId, &file, utc(2026, 11, 6, 0, 0),
                                          utc(2026, 11, 10, 0, 0), count, utc(2026, 11, 7, 12, 0));
        if (!ok)
            qWarning() << store.lastError();
        return ok;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        QVERIFY(Database::initialize(m_dir.filePath(QStringLiteral("epg.sqlite"))));
        SourceStore sources(Database::connection());
        SourceRecord s;
        s.name = QStringLiteral("Test");
        s.type = QStringLiteral("m3u");
        s.url = QStringLiteral("http://example.com/list.m3u");
        m_sourceId = sources.addSource(s);
        QVERIFY(m_sourceId > 0);
        QVERIFY(sources.replaceContent(m_sourceId, content()));
    }

    void importsAndLinks()
    {
        int count = 0;
        QVERIFY(importFixture("guide.xml", &count));
        QCOMPARE(count, 6); // "Ancient History" (2025) is outside the window

        QCOMPARE(epgKeyOf("1"), QStringLiteral("bbc1.uk"));
        QCOMPARE(epgKeyOf("2"), QStringLiteral("named.only"));
        QCOMPARE(epgKeyOf("3"), QStringLiteral("cnn.us"));
        QCOMPARE(epgKeyOf("4"), QString());
        QCOMPARE(epgKeyOf("5"), QStringLiteral("orphan.id"));

        const auto source = SourceStore(Database::connection()).source(m_sourceId);
        QCOMPARE(source->epgCount, 6);
        QVERIFY(source->epgLastSync.isValid());
    }

    void nowNext()
    {
        EpgStore store(Database::connection());
        const auto bbc = store.nowNext(m_sourceId, QStringLiteral("bbc1.uk"), utc(2026, 11, 7, 18, 10));
        QCOMPARE(bbc.size(), 2);
        QCOMPARE(bbc.at(0).title, QStringLiteral("BBC News at Six"));
        QCOMPARE(bbc.at(1).title, QStringLiteral("Weather"));

        // Before anything starts: the first upcoming program
        const auto early = store.nowNext(m_sourceId, QStringLiteral("bbc1.uk"), utc(2026, 11, 7, 12, 0));
        QCOMPARE(early.at(0).title, QStringLiteral("Antiques Roadshow"));

        QVERIFY(store.nowNext(m_sourceId, QString(), utc(2026, 11, 7, 18, 10)).isEmpty());

        // A +1 h correction: at 19:10 the 18:00 program (shown as 19:00) is on air
        const auto shifted = store.nowNext(m_sourceId, QStringLiteral("bbc1.uk"),
                                           utc(2026, 11, 7, 19, 10), 3600);
        QCOMPARE(shifted.at(0).title, QStringLiteral("BBC News at Six"));
        QCOMPARE(shifted.at(0).start, utc(2026, 11, 7, 19, 0));

        const auto range = store.programs(m_sourceId, QStringLiteral("bbc1.uk"),
                                          utc(2026, 11, 7, 17, 30), utc(2026, 11, 7, 19, 0));
        QCOMPARE(range.size(), 3);
    }

    void channelResyncKeepsLinks()
    {
        QVERIFY(SourceStore(Database::connection()).replaceContent(m_sourceId, content()));
        QCOMPARE(epgKeyOf("1"), QStringLiteral("bbc1.uk"));
        QCOMPARE(epgKeyOf("2"), QStringLiteral("named.only"));
    }

    void gzipImportReplacesGuide()
    {
        int count = 0;
        QVERIFY(importFixture("guide.xml.gz", &count));
        QCOMPARE(count, 6);
        QSqlQuery q(Database::connection());
        QVERIFY(q.exec(QStringLiteral("SELECT COUNT(*) FROM epg_programs")) && q.next());
        QCOMPARE(q.value(0).toInt(), 6);
    }

    void prune()
    {
        EpgStore store(Database::connection());
        QVERIFY(store.prune(m_sourceId, utc(2026, 11, 7, 18, 31)));
        QSqlQuery q(Database::connection());
        QVERIFY(q.exec(QStringLiteral("SELECT COUNT(*) FROM epg_programs")) && q.next());
        QCOMPARE(q.value(0).toInt(), 3); // Weather, CNN and orphan end at 19:00
    }

    void largeGuideImport()
    {
        QByteArray xml = "<tv>";
        const qint64 base = utc(2026, 11, 7, 0, 0);
        for (int c = 0; c < 500; ++c) {
            for (int i = 0; i < 200; ++i) {
                const auto t = [](qint64 secs) {
                    return QDateTime::fromSecsSinceEpoch(secs, QTimeZone::utc())
                            .toString(QStringLiteral("yyyyMMddHHmmss +0000")).toLatin1();
                };
                xml += "<programme start=\"" + t(base + i * 1800) + "\" stop=\""
                        + t(base + (i + 1) * 1800) + "\" channel=\"bulk" + QByteArray::number(c)
                        + "\"><title>Show " + QByteArray::number(i)
                        + "</title><desc>A description that is about this long, like real ones.</desc></programme>\n";
            }
        }
        xml += "</tv>";
        QBuffer buffer(&xml);
        buffer.open(QIODevice::ReadOnly);
        QElapsedTimer timer;
        timer.start();
        int count = 0;
        // A provider whose lists aren't loaded yet: the whole guide is kept.
        SourceRecord empty;
        empty.name = QStringLiteral("No lists yet");
        empty.type = QStringLiteral("m3u");
        empty.url = QStringLiteral("http://example.com/empty.m3u");
        const int emptyId = SourceStore(Database::connection()).addSource(empty);
        EpgStore store(Database::connection());
        QVERIFY2(store.importGuide(emptyId, &buffer, base, base + 7 * 86400, &count, base),
                 qPrintable(store.lastError()));
        QCOMPARE(count, 100000);
        qDebug() << "Imported 100k programs in" << timer.elapsed() << "ms";
        QVERIFY(SourceStore(Database::connection()).removeSource(emptyId));
        QVERIFY(importFixture("guide.xml", &count)); // restore for the next tests (prune() trimmed it)
    }

    void onlyListedChannelsAndArchive()
    {
        // BBC keeps 3 days of catch-up; CNN has none; "unlisted" isn't in the lists.
        SourceContent c = content();
        c.channels[0].archiveDays = 3;
        QVERIFY(SourceStore(Database::connection()).replaceContent(m_sourceId, c));

        const qint64 now = utc(2026, 11, 7, 12, 0);
        const auto t = [](qint64 secs) {
            return QDateTime::fromSecsSinceEpoch(secs, QTimeZone::utc())
                    .toString(QStringLiteral("yyyyMMddHHmmss +0000")).toLatin1();
        };
        QByteArray xml = "<tv><channel id=\"bbc1.uk\"/><channel id=\"cnn.us\"/><channel id=\"unlisted\"/>";
        for (const char *id : { "bbc1.uk", "cnn.us", "unlisted" }) {
            for (const qint64 start : { now - 2 * 86400, now - 3600, now + 3600 }) {
                xml += "<programme start=\"" + t(start) + "\" stop=\"" + t(start + 1800)
                        + "\" channel=\"" + id + "\"><title>x</title></programme>";
            }
        }
        xml += "</tv>";
        QBuffer buffer(&xml);
        buffer.open(QIODevice::ReadOnly);
        int count = 0;
        EpgStore store(Database::connection());
        QVERIFY(store.importGuide(m_sourceId, &buffer, now - 7 * 86400, now + 4 * 86400, &count, now));
        // BBC: all 3 (archive reaches 2 days back); CNN: the last 2 (one day back); unlisted: none
        QCOMPARE(count, 5);

        QVERIFY(SourceStore(Database::connection()).replaceContent(m_sourceId, content()));
        QVERIFY(importFixture("guide.xml", &count));
    }

    void invalidGuideKeepsOldGuide()
    {
        QByteArray html = "<html>Not found</html>";
        QBuffer buffer(&html);
        buffer.open(QIODevice::ReadOnly);
        EpgStore store(Database::connection());
        QVERIFY(!store.importGuide(m_sourceId, &buffer, 0, utc(2030, 1, 1, 0, 0)));
        QVERIFY(!store.lastError().isEmpty());
        QSqlQuery q(Database::connection());
        QVERIFY(q.exec(QStringLiteral("SELECT COUNT(*) FROM epg_programs")) && q.next());
        QCOMPARE(q.value(0).toInt(), 6); // unchanged
    }
};

QTEST_GUILESS_MAIN(TestEpgStore)
#include "tst_epgstore.moc"
