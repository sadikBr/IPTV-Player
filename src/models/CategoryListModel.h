#pragma once

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

// Categories of one source for one kind of content, with two special entries first:
// "All channels" (AllCategories) and "Favorites" (FavoritesCategory).
class CategoryListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int sourceId READ sourceId WRITE setSourceId NOTIFY sourceIdChanged)
    // "live" (default), "movie" or "series"
    Q_PROPERTY(QString kind READ kind WRITE setKind NOTIFY kindChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    // Number of categories of this kind the user has hidden
    Q_PROPERTY(int hiddenCount READ hiddenCount NOTIFY hiddenCountChanged)

public:
    enum Special { AllCategories = -1, FavoritesCategory = -2 };
    Q_ENUM(Special)

    enum Role { CategoryIdRole = Qt::UserRole + 1, NameRole, CountRole };

    explicit CategoryListModel(QObject *parent = nullptr);

    int sourceId() const { return m_sourceId; }
    void setSourceId(int id);
    QString kind() const { return m_kind; }
    void setKind(const QString &kind);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE int indexOf(int categoryId) const;
    Q_INVOKABLE QString nameOf(int categoryId) const;
    Q_INVOKABLE void hideCategory(int categoryId);
    int hiddenCount() const { return m_hiddenCount; }

public slots:
    void reload();

signals:
    void sourceIdChanged();
    void kindChanged();
    void countChanged();
    void hiddenCountChanged();

private:
    struct Row
    {
        int id;
        QString name;
        int count;
    };

    int m_sourceId = -1;
    QString m_kind = QStringLiteral("live");
    int m_hiddenCount = 0;
    QList<Row> m_rows;
};
