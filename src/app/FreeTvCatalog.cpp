#include "FreeTvCatalog.h"

#include "FreeTv.h"
#include "NetworkFactory.h"

#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>

using namespace iptv;

FreeTvCatalog::FreeTvCatalog(QObject *parent)
    : QObject(parent), m_network(NetworkFactory::createManager(this))
{
}

QString FreeTvCatalog::localCountry() const
{
    const QString name = QLocale::system().name(); // "fr_FR"
    return name.size() >= 5 ? name.mid(3, 2).toUpper() : QString();
}

void FreeTvCatalog::load()
{
    if (m_pending > 0 || (!m_countries.isEmpty() && !m_categories.isEmpty()))
        return;
    m_error.clear();
    const auto fetch = [this](const QUrl &url, bool countries) {
        ++m_pending;
        QNetworkReply *reply = m_network->get(QNetworkRequest(url));
        connect(reply, &QNetworkReply::finished, this, [this, reply, countries] {
            reply->deleteLater();
            --m_pending;
            if (reply->error() != QNetworkReply::NoError) {
                m_error = tr("Couldn't load the list of free channels: %1").arg(reply->errorString());
            } else {
                QVariantList list;
                const auto choices = countries ? FreeTv::parseCountries(reply->readAll())
                                               : FreeTv::parseCategories(reply->readAll());
                for (const auto &c : choices)
                    list.append(QVariantMap { { QStringLiteral("code"), c.code },
                                              { QStringLiteral("name"), c.name },
                                              { QStringLiteral("flag"), c.flag } });
                (countries ? m_countries : m_categories) = list;
            }
            emit changed();
        });
    };
    fetch(FreeTv::countriesUrl(), true);
    fetch(FreeTv::categoriesUrl(), false);
    emit changed();
}

QUrl FreeTvCatalog::allChannelsUrl() const { return FreeTv::allChannelsUrl(); }
QUrl FreeTvCatalog::countryUrl(const QString &code) const { return FreeTv::countryUrl(code); }
QUrl FreeTvCatalog::categoryUrl(const QString &id) const { return FreeTv::categoryUrl(id); }
