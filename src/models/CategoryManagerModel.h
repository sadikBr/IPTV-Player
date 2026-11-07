#pragma once

#include <QAbstractListModel>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

// All categories of one source and kind, with their visibility, for the
// "Choose categories" screen. Categories are grouped by language/region
// prefix ("[EN] …", "FR | …") so whole groups can be switched at once.
class CategoryManagerModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int sourceId READ sourceId WRITE setSourceId NOTIFY sourceIdChanged)
    Q_PROPERTY(QString kind READ kind WRITE setKind NOTIFY kindChanged)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    // Only show categories of this group ("" = all groups)
    Q_PROPERTY(QString groupFilter READ groupFilter WRITE setGroupFilter NOTIFY groupFilterChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY statsChanged)
    Q_PROPERTY(int hiddenCount READ hiddenCount NOTIFY statsChanged)
    // [{name, total, hidden}] sorted by size; "Other" collects the rest
    Q_PROPERTY(QVariantList groups READ groups NOTIFY statsChanged)

public:
    enum Role { NameRole = Qt::UserRole + 1, CountRole, HiddenRole, GroupRole };

    explicit CategoryManagerModel(QObject *parent = nullptr);

    int sourceId() const { return m_sourceId; }
    void setSourceId(int id);
    QString kind() const { return m_kind; }
    void setKind(const QString &kind);
    QString filterText() const { return m_filterText; }
    void setFilterText(const QString &text);
    QString groupFilter() const { return m_groupFilter; }
    void setGroupFilter(const QString &group);

    int totalCount() const { return int(m_all.size()); }
    int hiddenCount() const;
    QVariantList groups() const { return m_groups; }

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void setHidden(int row, bool hidden);
    // Every category of a group ("Other" for those without one)
    Q_INVOKABLE void setGroupHidden(const QString &group, bool hidden);
    // Every category currently listed (respects search and group filter)
    Q_INVOKABLE void setListedHidden(bool hidden);

public slots:
    void reload();

signals:
    void sourceIdChanged();
    void kindChanged();
    void filterTextChanged();
    void groupFilterChanged();
    void countChanged();
    void statsChanged();

private:
    struct Category
    {
        QString extId;
        QString name;
        QString searchKey;
        QString group;
        int count = 0;
        bool hidden = false;
    };

    void applyFilter();
    void updateStats();
    void apply(const QList<int> &indexes, bool hidden);

    int m_sourceId = -1;
    QString m_kind = QStringLiteral("live");
    QString m_filterText;
    QString m_groupFilter;
    QList<Category> m_all;
    QList<int> m_rows; // indexes into m_all
    QVariantList m_groups;
    bool m_applying = false;
};
