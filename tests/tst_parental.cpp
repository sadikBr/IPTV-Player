#include "Parental.h"

#include <QTest>

using namespace iptv;

class TstParental : public QObject
{
    Q_OBJECT

private slots:
    void pins()
    {
        QVERIFY(Parental::isValidPin(QStringLiteral("1234")));
        QVERIFY(Parental::isValidPin(QStringLiteral("12345678")));
        QVERIFY(!Parental::isValidPin(QStringLiteral("123")));
        QVERIFY(!Parental::isValidPin(QStringLiteral("12a4")));
        QVERIFY(!Parental::isValidPin(QStringLiteral("")));

        const QByteArray salt = Parental::newSalt();
        QCOMPARE(salt.size(), 32);
        QVERIFY(salt != Parental::newSalt());
        const QByteArray hash = Parental::hashPin(QStringLiteral("1234"), salt);
        QCOMPARE(hash, Parental::hashPin(QStringLiteral("1234"), salt));
        QVERIFY(hash != Parental::hashPin(QStringLiteral("1235"), salt));
        QVERIFY(hash != Parental::hashPin(QStringLiteral("1234"), Parental::newSalt()));
        QVERIFY(!hash.contains("1234"));
    }

    void adultCategories_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<bool>("adult");
        QTest::newRow("xxx") << "XXX Movies" << true;
        QTest::newRow("prefixed") << "[EN] Adult" << true;
        QTest::newRow("18+") << "VOD 18+" << true;
        QTest::newRow("+18") << "FR | +18" << true;
        QTest::newRow("adultes") << "Films pour adultes" << true;
        QTest::newRow("kids") << "Kids" << false;
        QTest::newRow("sports") << "Sports HD" << false;
        QTest::newRow("year") << "Movies 2018" << false;
        QTest::newRow("channel 180") << "Channel 180" << false;
        QTest::newRow("essex") << "Essex News" << false;
    }
    void adultCategories()
    {
        QFETCH(QString, name);
        QFETCH(bool, adult);
        QCOMPARE(Parental::looksAdult(name), adult);
    }
};

QTEST_MAIN(TstParental)
#include "tst_parental.moc"
