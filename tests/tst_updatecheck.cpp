#include "UpdateCheck.h"

#include <QFile>
#include <QTest>

using namespace iptv;

class TstUpdateCheck : public QObject
{
    Q_OBJECT

private slots:
    void compareVersions_data()
    {
        QTest::addColumn<QString>("a");
        QTest::addColumn<QString>("b");
        QTest::addColumn<int>("result");
        QTest::newRow("equal") << "0.8.0" << "0.8.0" << 0;
        QTest::newRow("v prefix") << "v0.8.0" << "0.8.0" << 0;
        QTest::newRow("short") << "0.8" << "0.8.0" << 0;
        QTest::newRow("patch") << "0.8.1" << "0.8.0" << 1;
        QTest::newRow("numeric not text") << "0.10.0" << "0.9.3" << 1;
        QTest::newRow("older") << "0.7.0" << "0.8.0" << -1;
        QTest::newRow("major") << "1.0.0" << "0.99.99" << 1;
        QTest::newRow("suffix ignored") << "0.8.0-beta" << "0.8.0" << 0;
    }
    void compareVersions()
    {
        QFETCH(QString, a);
        QFETCH(QString, b);
        QFETCH(int, result);
        QCOMPARE(Updates::compareVersions(a, b), result);
        QCOMPARE(Updates::compareVersions(b, a), -result);
    }

    void parseRelease()
    {
        QFile file(QStringLiteral(FIXTURES_DIR "/github_release.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray json = file.readAll();
        const ReleaseInfo r = Updates::parseRelease(json, QStringLiteral("IPTVPlayer-android.apk"));
        QVERIFY(r.valid);
        QCOMPARE(r.version, QStringLiteral("0.8.0"));
        QCOMPARE(r.notes, QStringLiteral("## What's new\n- Updates"));
        QCOMPARE(r.pageUrl.toString(), QStringLiteral("https://github.com/sadikBr/IPTV-Player/releases/tag/v0.8.0"));
        QCOMPARE(r.assetName, QStringLiteral("IPTVPlayer-android.apk"));
        QCOMPARE(r.assetUrl.toString(), QStringLiteral("https://example.com/a.apk"));
        QCOMPARE(r.assetSize, 100);
        QCOMPARE(r.sha256, QByteArray("abcdef0123"));

        const ReleaseInfo w = Updates::parseRelease(json, QStringLiteral("IPTVPlayer-setup-x64.exe"));
        QCOMPARE(w.assetSize, 200);
        QVERIFY(w.sha256.isEmpty());

        const ReleaseInfo none = Updates::parseRelease(json, QStringLiteral("missing.bin"));
        QVERIFY(none.valid);
        QVERIFY(none.assetName.isEmpty());
    }

    void parseReleaseRejects()
    {
        QVERIFY(!Updates::parseRelease("not json", {}).valid);
        QVERIFY(!Updates::parseRelease("{\"tag_name\":\"v1.0\",\"draft\":true}", {}).valid);
        QVERIFY(!Updates::parseRelease("{\"tag_name\":\"v1.0\",\"prerelease\":true}", {}).valid);
        QVERIFY(!Updates::parseRelease("{\"message\":\"Not Found\"}", {}).valid);
    }
};

QTEST_MAIN(TstUpdateCheck)
#include "tst_updatecheck.moc"
