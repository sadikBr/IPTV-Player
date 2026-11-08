#pragma once

#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QNetworkAccessManager;

// The countries and categories of free channels (iptv-org) for the
// "Free channels" tab of Add provider. Loaded when first needed.
class FreeTvCatalog : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // [{code, name, flag}]
    Q_PROPERTY(QVariantList countries READ countries NOTIFY changed)
    // [{code, name}]
    Q_PROPERTY(QVariantList categories READ categories NOTIFY changed)
    Q_PROPERTY(bool loading READ isLoading NOTIFY changed)
    Q_PROPERTY(QString errorString READ errorString NOTIFY changed)
    // The country of this device (e.g. "FR"), to suggest first.
    Q_PROPERTY(QString localCountry READ localCountry CONSTANT)

public:
    explicit FreeTvCatalog(QObject *parent = nullptr);

    QVariantList countries() const { return m_countries; }
    QVariantList categories() const { return m_categories; }
    bool isLoading() const { return m_pending > 0; }
    QString errorString() const { return m_error; }
    QString localCountry() const;

    Q_INVOKABLE void load();
    Q_INVOKABLE QUrl allChannelsUrl() const;
    Q_INVOKABLE QUrl countryUrl(const QString &code) const;
    Q_INVOKABLE QUrl categoryUrl(const QString &id) const;

signals:
    void changed();

private:
    QNetworkAccessManager *m_network = nullptr;
    QVariantList m_countries;
    QVariantList m_categories;
    QString m_error;
    int m_pending = 0;
};
