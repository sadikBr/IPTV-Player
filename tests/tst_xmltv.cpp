#include "GzipDevice.h"
#include "XmltvParser.h"

#include <QBuffer>
#include <QFile>
#include <QtTest>

using namespace iptv;

class TestXmltv : public QObject
{
    Q_OBJECT

private:
    static qint64 utc(int y, int mo, int d, int h, int mi)
    {
        return QDateTime(QDate(y, mo, d), QTime(h, mi), QTimeZone::utc()).toSecsSinceEpoch();
    }

    struct Result
    {
        QList<EpgChannel> channels;
        QList<EpgProgram> programs;
        bool ok = false;
        QString error;
    };

    static Result parseDevice(QIODevice *device)
    {
        Result r;
        r.ok = XmltvParser::parse(
                device, [&](const EpgChannel &c) { r.channels.append(c); },
                [&](const EpgProgram &p) {
                    r.programs.append(p);
                    return true;
                },
                &r.error);
        return r;
    }

private slots:
    void parseTime_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<qint64>("expected");
        QTest::newRow("utc offset") << "20261107180000 +0000" << utc(2026, 11, 7, 18, 0);
        QTest::newRow("no offset") << "20261107180000" << utc(2026, 11, 7, 18, 0);
        QTest::newRow("plus one") << "20261107193000 +0100" << utc(2026, 11, 7, 18, 30);
        QTest::newRow("minus five") << "20261107130000 -0500" << utc(2026, 11, 7, 18, 0);
        QTest::newRow("colon offset") << "20261107190000 +01:00" << utc(2026, 11, 7, 18, 0);
        QTest::newRow("no seconds") << "202611071800 +0000" << utc(2026, 11, 7, 18, 0);
        QTest::newRow("garbage") << "garbage" << qint64(-1);
        QTest::newRow("bad date") << "20261340180000" << qint64(-1);
    }

    void parseTime()
    {
        QFETCH(QString, text);
        QFETCH(qint64, expected);
        QCOMPARE(XmltvParser::parseTime(text), expected);
    }

    void parsesFixture()
    {
        QFile file(QStringLiteral(FIXTURES_DIR "/guide.xml"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const Result r = parseDevice(&file);
        QVERIFY2(r.ok, qPrintable(r.error));

        QCOMPARE(r.channels.size(), 3);
        QCOMPARE(r.channels.at(0).id, QStringLiteral("BBC1.uk"));
        QCOMPARE(r.channels.at(0).names, QStringList({ "BBC One", "BBC One HD" }));

        QCOMPARE(r.programs.size(), 7); // the one with an invalid start is skipped
        const EpgProgram &first = r.programs.at(0);
        QCOMPARE(first.channelId, QStringLiteral("BBC1.uk"));
        QCOMPARE(first.title, QStringLiteral("Antiques Roadshow"));
        QCOMPARE(first.subTitle, QStringLiteral("Series 48, Episode 3"));
        QCOMPARE(first.description, QStringLiteral("Valuable & surprising finds."));
        QCOMPARE(first.start, utc(2026, 11, 7, 17, 0));
        QCOMPARE(first.stop, utc(2026, 11, 7, 18, 0));

        QCOMPARE(r.programs.at(2).start, utc(2026, 11, 7, 18, 30)); // +0100 offset
        const EpgProgram &noStop = r.programs.at(4);
        QCOMPARE(noStop.stop - noStop.start, 30 * 60);
    }

    void parsesGzip()
    {
        QFile file(QStringLiteral(FIXTURES_DIR "/guide.xml.gz"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(GzipDevice::isGzip(&file));
        GzipDevice gzip(&file);
        QVERIFY(gzip.open(QIODevice::ReadOnly));
        const Result r = parseDevice(&gzip);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.channels.size(), 3);
        QCOMPARE(r.programs.size(), 7);
    }

    void rejectsNonXmltv()
    {
        QByteArray html = "<html><body>404</body></html>";
        QBuffer buffer(&html);
        buffer.open(QIODevice::ReadOnly);
        const Result r = parseDevice(&buffer);
        QVERIFY(!r.ok);
        QVERIFY(!r.error.isEmpty());

        QByteArray broken = "<tv><programme start=\"20261107180000\" channel=\"a\"><title>x</ti";
        QBuffer brokenBuffer(&broken);
        brokenBuffer.open(QIODevice::ReadOnly);
        // A truncated download keeps what was read.
        QVERIFY(parseDevice(&brokenBuffer).ok);
    }

    void largeGuideStreams()
    {
        QByteArray xml = "<tv>";
        for (int c = 0; c < 200; ++c) {
            for (int i = 0; i < 100; ++i) {
                xml += "<programme start=\"20261107180000 +0000\" stop=\"20261107235900 +0000\" channel=\"ch"
                        + QByteArray::number(c) + "\"><title>Show</title><desc>Some text</desc></programme>\n";
            }
        }
        xml += "</tv>";
        QBuffer buffer(&xml);
        buffer.open(QIODevice::ReadOnly);
        QElapsedTimer timer;
        timer.start();
        int count = 0;
        QVERIFY(XmltvParser::parse(&buffer, nullptr, [&](const EpgProgram &) { return ++count > 0; },
                                   nullptr));
        QCOMPARE(count, 20000);
        qDebug() << "Parsed 20k programs in" << timer.elapsed() << "ms";
    }
};

QTEST_GUILESS_MAIN(TestXmltv)
#include "tst_xmltv.moc"
