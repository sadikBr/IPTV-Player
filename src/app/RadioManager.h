#pragma once

#include "RadioBrowser.h"
#include "RadioStationModel.h"

#include <QObject>
#include <QPointer>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class QNetworkAccessManager;
class QNetworkReply;

// Internet radio: browses the radio-browser.info directory, keeps favorite
// and recently played stations, and knows which station is playing.
class RadioManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(RadioStationModel *stations READ stations CONSTANT)
    // Stations for the app-wide search (separate from the Radio page)
    Q_PROPERTY(RadioStationModel *searchResults READ searchResults CONSTANT)
    // "favorites", "recent", "top", "trending", "country", "genre", "search"
    Q_PROPERTY(QString mode READ mode NOTIFY stateChanged)
    Q_PROPERTY(QString modeValue READ modeValue NOTIFY stateChanged)
    Q_PROPERTY(bool loading READ isLoading NOTIFY stateChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY stateChanged)
    // The station playing (or last played): {uuid, name, url, logo, details, favorite}
    Q_PROPERTY(QVariantMap current READ current NOTIFY currentChanged)
    Q_PROPERTY(QString localCountry READ localCountry CONSTANT)
    Q_PROPERTY(QStringList genres READ genres CONSTANT)
    Q_PROPERTY(int favoriteCount READ favoriteCount NOTIFY favoritesChanged)

public:
    explicit RadioManager(QObject *parent = nullptr);

    RadioStationModel *stations() { return &m_model; }
    RadioStationModel *searchResults() { return &m_searchModel; }
    QString mode() const { return m_mode; }
    QString modeValue() const { return m_modeValue; }
    bool isLoading() const { return m_reply != nullptr; }
    QString errorString() const { return m_error; }
    QVariantMap current() const;
    QString localCountry() const;
    QStringList genres() const;
    int favoriteCount() const;

    Q_INVOKABLE void showFavorites();
    Q_INVOKABLE void showRecent();
    Q_INVOKABLE void showTop();
    Q_INVOKABLE void showTrending();
    Q_INVOKABLE void showCountry(const QString &code);
    Q_INVOKABLE void showGenre(const QString &tag);
    Q_INVOKABLE void search(const QString &text);
    Q_INVOKABLE void reload();

    // Plays the station in the list; emits playRequested with its URL.
    Q_INVOKABLE void play(int row);
    Q_INVOKABLE void searchStations(const QString &text);
    Q_INVOKABLE void playSearchResult(int row);
    Q_INVOKABLE void toggleFavorite(int row);
    Q_INVOKABLE void toggleCurrentFavorite();

signals:
    void stateChanged();
    void currentChanged();
    void favoritesChanged();
    void playRequested(const QUrl &url, const QString &name);

private:
    void setMode(const QString &mode, const QString &value);
    void showLocal(const QList<iptv::RadioStation> &stations);
    void query(const QUrl &url);
    void withServer(std::function<void(const QString &)> then);
    void setFavorite(const iptv::RadioStation &station, bool favorite);
    QSet<QString> favoriteIds() const;

    QNetworkAccessManager *m_network = nullptr;
    RadioStationModel m_model;
    RadioStationModel m_searchModel;
    QPointer<QNetworkReply> m_searchReply;
    QPointer<QNetworkReply> m_reply;
    QString m_mode;
    QString m_modeValue;
    QString m_error;
    QString m_server;
    bool m_resolvingServer = false;
    QList<std::function<void(const QString &)>> m_waiting;
    iptv::RadioStation m_current;
};
