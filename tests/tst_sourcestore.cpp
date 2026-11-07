#include "Database.h"
#include "SourceStore.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtConcurrent/QtConcurrentRun>
#include <QtTest>

using namespace iptv;

class TestSourceStore : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    static SourceContent sampleContent(int channelCount)
    {
        SourceContent c;
        c.epgUrl = QStringLiteral("http://epg.example.com/a.xml");
        c.categories = { { QStringLiteral("1"), QStringLiteral("News"), ContentKind::Live },
                         { QStringLiteral("2"), QStringLiteral("Sports"), ContentKind::Live } };
        for (int i = 0; i < channelCount; ++i) {
            ChannelData ch;
            ch.extId = QString::number(1000 + i);
            ch.name = QStringLiteral("Channel %1").arg(i);
            ch.categoryExtId = i % 2 ? QStringLiteral("2") : QStringLiteral("1");
            ch.number = i + 1;
            c.channels.append(ch);
        }
        return c;
    }

    static int count(const QString &sql)
    {
        QSqlQuery q(Database::connection());
        return q.exec(sql) && q.next() ? q.value(0).toInt() : -1;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        QString error;
        QVERIFY2(Database::initialize(m_dir.filePath(QStringLiteral("test.sqlite")), &error),
                 qPrintable(error));
        QCOMPARE(Database::schemaVersion(), Database::latestSchemaVersion());
    }

    void migrationIsIdempotent()
    {
        QString error;
        QVERIFY2(Database::initialize(m_dir.filePath(QStringLiteral("test.sqlite")), &error),
                 qPrintable(error));
        QCOMPARE(Database::schemaVersion(), Database::latestSchemaVersion());
    }

    void addAndReadSource()
    {
        SourceStore store(Database::connection());
        SourceRecord s;
        s.name = QStringLiteral("My provider");
        s.type = QStringLiteral("xtream");
        s.url = QStringLiteral("http://example.com:8080");
        s.username = QStringLiteral("john");
        s.password = QStringLiteral("secret");
        s.outputFormat = QStringLiteral("m3u8");
        s.expiresAt = QDateTime::fromSecsSinceEpoch(1798761600);
        const int id = store.addSource(s);
        QVERIFY2(id > 0, qPrintable(store.lastError()));

        const auto read = store.source(id);
        QVERIFY(read.has_value());
        QCOMPARE(read->name, s.name);
        QCOMPARE(read->password, s.password);
        QCOMPARE(read->outputFormat, QStringLiteral("m3u8"));
        QCOMPARE(read->expiresAt.toSecsSinceEpoch(), 1798761600);
        QVERIFY(read->isXtream());
        QCOMPARE(store.sources().size(), 1);

        const QUrl url = SourceStore::streamUrl(*read, ContentKind::Live, QStringLiteral("42"), {});
        QCOMPARE(url.toString(), QStringLiteral("http://example.com:8080/live/john/secret/42.m3u8"));
    }

    void replaceContentKeepsFavorites()
    {
        SourceStore store(Database::connection());
        const int id = store.sources().constFirst().id;

        QVERIFY2(store.replaceContent(id, sampleContent(10)), qPrintable(store.lastError()));
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM channels")), 10);
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM categories")), 2);
        QCOMPARE(store.source(id)->liveCount, 10);
        QCOMPARE(store.source(id)->epgUrl, QStringLiteral("http://epg.example.com/a.xml"));
        QVERIFY(store.source(id)->lastSync.isValid());

        QVERIFY(store.setFavorite(id, ContentKind::Live, QStringLiteral("1003"), true));
        QVERIFY(store.isFavorite(id, ContentKind::Live, QStringLiteral("1003")));

        // Re-sync with fewer channels: content replaced, favorite kept
        QVERIFY(store.replaceContent(id, sampleContent(5)));
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM channels")), 5);
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM categories")), 2);
        QVERIFY(store.isFavorite(id, ContentKind::Live, QStringLiteral("1003")));

        QVERIFY(store.setFavorite(id, ContentKind::Live, QStringLiteral("1003"), false));
        QVERIFY(!store.isFavorite(id, ContentKind::Live, QStringLiteral("1003")));
    }

    void writeFromWorkerThread()
    {
        const int id = SourceStore(Database::connection()).sources().constFirst().id;
        QFuture<bool> future = QtConcurrent::run([id] {
            SourceStore store(Database::connection());
            const bool ok = store.replaceContent(id, sampleContent(20000));
            Database::closeThreadConnection();
            return ok;
        });
        future.waitForFinished();
        QVERIFY(future.result());
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM channels")), 20000);
    }

    void moviesSeriesAndEpisodes()
    {
        SourceStore store(Database::connection());
        const int id = store.sources().constFirst().id;
        SourceContent c = sampleContent(3);
        ChannelData movie;
        movie.kind = ContentKind::Movie;
        movie.extId = QStringLiteral("5001");
        movie.name = QStringLiteral("The Long Night");
        movie.extension = QStringLiteral("mkv");
        movie.year = QStringLiteral("2023");
        c.channels.append(movie);
        ChannelData series;
        series.kind = ContentKind::Series;
        series.extId = QStringLiteral("701");
        series.name = QStringLiteral("Ocean Drive");
        c.channels.append(series);
        QVERIFY2(store.replaceContent(id, c), qPrintable(store.lastError()));

        const auto source = store.source(id);
        QCOMPARE(source->liveCount, 3);
        QCOMPARE(source->movieCount, 1);
        QCOMPARE(source->seriesCount, 1);
        QCOMPARE(SourceStore::streamUrl(*source, ContentKind::Movie, QStringLiteral("5001"), {},
                                        QStringLiteral("mkv")).toString(),
                 QStringLiteral("http://example.com:8080/movie/john/secret/5001.mkv"));

        // Episodes loaded later (Xtream) survive a re-sync while the series exists
        EpisodeData e1;
        e1.extId = QStringLiteral("9011");
        e1.season = 1;
        e1.episode = 1;
        e1.title = QStringLiteral("Pilot");
        EpisodeData e2 = e1;
        e2.extId = QStringLiteral("9012");
        e2.episode = 2;
        QVERIFY(store.replaceEpisodes(id, QStringLiteral("701"), { e2, e1 }));
        QCOMPARE(store.episodes(id, QStringLiteral("701")).size(), 2);
        QCOMPARE(store.episodes(id, QStringLiteral("701")).first().title, QStringLiteral("Pilot"));
        QVERIFY(store.replaceContent(id, c));
        QCOMPARE(store.episodes(id, QStringLiteral("701")).size(), 2);
        c.channels.removeLast(); // series gone -> its episodes too
        QVERIFY(store.replaceContent(id, c));
        QCOMPARE(store.episodes(id, QStringLiteral("701")).size(), 0);

        VodDetails d;
        d.plot = QStringLiteral("A storm.");
        d.durationSecs = 6300;
        QVERIFY(store.saveDetails(id, ContentKind::Movie, QStringLiteral("5001"), d));
        QDateTime fetched;
        const auto read = store.details(id, ContentKind::Movie, QStringLiteral("5001"), &fetched);
        QVERIFY(read.has_value());
        QCOMPARE(read->plot, d.plot);
        QCOMPARE(read->durationSecs, 6300);
        QVERIFY(fetched.isValid());
    }

    void watchProgress()
    {
        SourceStore store(Database::connection());
        const int id = store.sources().constFirst().id;
        WatchProgress movie;
        movie.sourceId = id;
        movie.kind = ContentKind::Movie;
        movie.extId = QStringLiteral("5001");
        movie.positionMs = 600000;
        movie.durationMs = 6300000;
        movie.title = QStringLiteral("The Long Night");
        movie.updatedAt = QDateTime::fromSecsSinceEpoch(1000);
        QVERIFY(store.saveProgress(movie));

        WatchProgress ep;
        ep.sourceId = id;
        ep.kind = ContentKind::Episode;
        ep.seriesExtId = QStringLiteral("701");
        ep.durationMs = 2700000;
        ep.positionMs = 2690000; // finished
        ep.extId = QStringLiteral("9011");
        ep.updatedAt = QDateTime::fromSecsSinceEpoch(2000);
        QVERIFY(store.saveProgress(ep));
        ep.extId = QStringLiteral("9012");
        ep.positionMs = 100000; // in progress, most recent
        ep.updatedAt = QDateTime::fromSecsSinceEpoch(3000);
        QVERIFY(store.saveProgress(ep));

        const auto recent = store.recentProgress(10);
        QCOMPARE(recent.size(), 2);
        QCOMPARE(recent.at(0).extId, QStringLiteral("9012")); // newest first, one per series
        QCOMPARE(recent.at(1).extId, QStringLiteral("5001"));
        QCOMPARE(store.progress(id, ContentKind::Movie, QStringLiteral("5001"))->positionMs, 600000);
        QVERIFY(store.progress(id, ContentKind::Episode, QStringLiteral("9011"))->finished());

        QVERIFY(store.removeProgress(id, ContentKind::Movie, QStringLiteral("5001")));
        QCOMPARE(store.recentProgress(10).size(), 1);
    }

    void hiddenCategories()
    {
        SourceStore store(Database::connection());
        const int id = store.sources().constFirst().id;
        QVERIFY(store.replaceContent(id, sampleContent(10))); // categories "1" and "2", 5 channels each
        const auto visible = [id]() {
            QSqlQuery q(Database::connection());
            q.prepare(QStringLiteral("SELECT COUNT(*) FROM channels c WHERE c.source_id = ? AND ")
                      + SourceStore::notInHiddenCategory());
            q.addBindValue(id);
            return q.exec() && q.next() ? q.value(0).toInt() : -1;
        };
        QCOMPARE(visible(), 10);
        QVERIFY(store.setCategoriesHidden(id, ContentKind::Live, { QStringLiteral("2") }, true));
        QCOMPARE(visible(), 5);
        // Survives a re-sync (stored by provider id)
        QVERIFY(store.replaceContent(id, sampleContent(10)));
        QCOMPARE(visible(), 5);
        QVERIFY(store.setCategoriesHidden(id, ContentKind::Live, { QStringLiteral("2") }, false));
        QCOMPARE(visible(), 10);
    }

    void removeSourceCascades()
    {
        SourceStore store(Database::connection());
        const int id = store.sources().constFirst().id;
        QVERIFY(store.setFavorite(id, ContentKind::Live, QStringLiteral("1000"), true));
        QVERIFY(store.removeSource(id));
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM channels")), 0);
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM categories")), 0);
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM favorites")), 0);
    }

    void categoryGroup_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<QString>("group");
        QTest::newRow("bracket") << "[EN] KIDS" << "EN";
        QTest::newRow("bracket lower") << "[fr] Séries VF" << "FR";
        QTest::newRow("bracket arabic") << "[AR] رمضان 2025" << "AR";
        QTest::newRow("bracket spaces") << " [ CA ] QUEBECOISE" << "CA";
        QTest::newRow("pipe") << "UK | General" << "UK";
        QTest::newRow("colon") << "FR: Sport" << "FR";
        QTest::newRow("dash") << "DE - Filme" << "DE";
        QTest::newRow("parens") << "(4K) Movies" << "4K";
        QTest::newRow("none") << "Sports" << "";
        QTest::newRow("word with dash") << "Movies 24/7" << "";
    }

    void categoryGroup()
    {
        QFETCH(QString, name);
        QFETCH(QString, group);
        QCOMPARE(SourceStore::categoryGroup(name), group);
    }

    void searchKey()
    {
        QCOMPARE(SourceStore::searchKey(QStringLiteral("Café TV ÉLITE")), QStringLiteral("cafe tv elite"));
        QCOMPARE(SourceStore::searchKey(QStringLiteral("BBC One HD")), QStringLiteral("bbc one hd"));
    }
};

QTEST_GUILESS_MAIN(TestSourceStore)
#include "tst_sourcestore.moc"
