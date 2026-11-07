#pragma once

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

// Live TV categories of one source, with two special entries first:
// "All channels" (AllCategories) and "Favorites" (FavoritesCategory).
class CategoryListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int sourceId READ sourceId WRITE setSourceId NOTIFY sourceIdChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Special { AllCategories = -1, FavoritesCategory = -2 };
    Q_ENUM(Special)

    enum Role { CategoryIdRole = Qt::UserRole + 1, NameRole, CountRole };

    explicit CategoryListModel(QObject *parent = nullptr);

    int sourceId() const { return m_sourceId; }
    void setSourceId(int id);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE int indexOf(int categoryId) const;
    Q_INVOKABLE QString nameOf(int categoryId) const;

public slots:
    void reload();

signals:
    void sourceIdChanged();
    void countChanged();

private:
    struct Row
    {
        int id;
        QString name;
        int count;
    };

    int m_sourceId = -1;
    QList<Row> m_rows;
};
