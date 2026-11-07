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

    void searchKey()
    {
        QCOMPARE(SourceStore::searchKey(QStringLiteral("Café TV ÉLITE")), QStringLiteral("cafe tv elite"));
        QCOMPARE(SourceStore::searchKey(QStringLiteral("BBC One HD")), QStringLiteral("bbc one hd"));
    }
};

QTEST_GUILESS_MAIN(TestSourceStore)
#include "tst_sourcestore.moc"
