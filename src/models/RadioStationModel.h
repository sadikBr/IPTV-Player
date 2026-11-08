#pragma once

#include "RadioBrowser.h"

#include <QAbstractListModel>
#include <QSet>
#include <QtQml/qqmlregistration.h>

// Radio stations for QML (a search result, top stations, favorites, ...).
class RadioStationModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by RadioManager")
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        UuidRole = Qt::UserRole + 1, NameRole, UrlRole, LogoRole, TagsRole, CountryRole,
        CountryCodeRole, CodecRole, BitrateRole, FavoriteRole, DetailsRole
    };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setStations(const QList<iptv::RadioStation> &stations, const QSet<QString> &favorites);
    void setFavorite(const QString &uuid, bool favorite);
    const iptv::RadioStation *station(int row) const;

signals:
    void countChanged();

private:
    QList<iptv::RadioStation> m_stations;
    QSet<QString> m_favorites;
};
