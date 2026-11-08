#include "Backup.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>

namespace iptv::Backup {

namespace {

constexpr int kFormat = 1;
const QString kApp = QStringLiteral("IPTVPlayer");

// Per provider: rows keyed by the provider's own ids (they survive list updates).
const QStringList kSourceTables = { QStringLiteral("favorites"), QStringLiteral("hidden_categories"),
                                    QStringLiteral("locked_categories"), QStringLiteral("watch_progress") };
const QStringList kSourceFields = { QStringLiteral("name"), QStringLiteral("type"), QStringLiteral("url"),
                                    QStringLiteral("username"), QStringLiteral("password"),
                                    QStringLiteral("output_format"), QStringLiteral("epg_url"),
                                    QStringLiteral("epg_shift_minutes"), QStringLiteral("server_timezone") };

QSet<QString> columns(QSqlDatabase &db, const QString &table)
{
    QSet<QString> result;
    const QSqlRecord record = db.record(table);
    for (int i = 0; i < record.count(); ++i)
        result.insert(record.fieldName(i));
    return result;
}

QJsonArray rows(QSqlDatabase &db, const QString &sql, const QVariantList &binds, const QString &skip = {})
{
    QJsonArray result;
    QSqlQuery q(db);
    q.prepare(sql);
    for (const QVariant &b : binds)
        q.addBindValue(b);
    if (!q.exec())
        return result;
    while (q.next()) {
        QJsonObject o;
        const QSqlRecord r = q.record();
        for (int i = 0; i < r.count(); ++i) {
            if (r.fieldName(i) != skip)
                o.insert(r.fieldName(i), QJsonValue::fromVariant(r.value(i)));
        }
        result.append(o);
    }
    return result;
}

// INSERT OR REPLACE with only the columns the table really has.
bool insertRow(QSqlDatabase &db, const QString &table, const QSet<QString> &allowed, QJsonObject row,
               const QVariantMap &extra = {})
{
    for (auto it = extra.cbegin(); it != extra.cend(); ++it)
        row.insert(it.key(), QJsonValue::fromVariant(it.value()));
    QStringList names;
    QVariantList values;
    for (auto it = row.constBegin(); it != row.constEnd(); ++it) {
        if (!allowed.contains(it.key()))
            continue;
        names << it.key();
        values << it.value().toVariant();
    }
    if (names.isEmpty())
        return false;
    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT OR REPLACE INTO %1 (%2) VALUES (%3)")
                      .arg(table, names.join(QStringLiteral(", ")),
                           QStringList(names.size(), QStringLiteral("?")).join(QStringLiteral(", "))));
    for (const QVariant &v : values)
        q.addBindValue(v.isNull() ? QVariant(QStringLiteral("")) : v);
    return q.exec();
}

} // namespace

QByteArray exportJson(QSqlDatabase db, const QVariantMap &settings)
{
    QJsonObject root;
    root.insert(QStringLiteral("app"), kApp);
    root.insert(QStringLiteral("format"), kFormat);
    root.insert(QStringLiteral("version"), QCoreApplication::applicationVersion());
    root.insert(QStringLiteral("exportedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    QJsonArray providers;
    QSqlQuery q(db);
    q.exec(QStringLiteral("SELECT id, %1 FROM sources ORDER BY created_at, id")
                   .arg(kSourceFields.join(QStringLiteral(", "))));
    while (q.next()) {
        QJsonObject p;
        for (int i = 0; i < kSourceFields.size(); ++i)
            p.insert(kSourceFields.at(i), QJsonValue::fromVariant(q.value(i + 1)));
        const int id = q.value(0).toInt();
        for (const QString &table : kSourceTables) {
            p.insert(table, rows(db, QStringLiteral("SELECT * FROM %1 WHERE source_id = ?").arg(table),
                                 { id }, QStringLiteral("source_id")));
        }
        providers.append(p);
    }
    root.insert(QStringLiteral("providers"), providers);
    root.insert(QStringLiteral("radioStations"),
                rows(db, QStringLiteral("SELECT * FROM radio_stations WHERE favorite = 1 OR played_at > 0"), {}));
    root.insert(QStringLiteral("settings"), QJsonObject::fromVariantMap(settings));
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

ImportResult importJson(QSqlDatabase db, const QByteArray &json)
{
    ImportResult result;
    QJsonParseError parseError;
    const QJsonObject root = QJsonDocument::fromJson(json, &parseError).object();
    if (parseError.error != QJsonParseError::NoError || root.value(QStringLiteral("app")).toString() != kApp) {
        result.error = QCoreApplication::translate("Backup", "This isn't an IPTV Player backup file.");
        return result;
    }
    if (root.value(QStringLiteral("format")).toInt() > kFormat) {
        result.error = QCoreApplication::translate("Backup",
                "This backup comes from a newer version of the app. Update the app first.");
        return result;
    }

    if (!db.transaction()) {
        result.error = db.lastError().text();
        return result;
    }
    const QSet<QString> sourceColumns = columns(db, QStringLiteral("sources"));
    QHash<QString, QSet<QString>> tableColumns;
    for (const QString &table : kSourceTables)
        tableColumns.insert(table, columns(db, table));

    for (const QJsonValue &v : root.value(QStringLiteral("providers")).toArray()) {
        const QJsonObject p = v.toObject();
        const QString type = p.value(QStringLiteral("type")).toString();
        const QString url = p.value(QStringLiteral("url")).toString();
        const QString username = p.value(QStringLiteral("username")).toString();
        if (type.isEmpty() || url.isEmpty())
            continue;

        // The same provider already here? (same type, address and login)
        int id = -1;
        QSqlQuery find(db);
        find.prepare(QStringLiteral("SELECT id FROM sources WHERE type = ? AND url = ? AND username = ?"));
        find.addBindValue(type);
        find.addBindValue(url);
        find.addBindValue(username);
        if (find.exec() && find.next())
            id = find.value(0).toInt();

        if (id < 0) {
            QJsonObject source;
            for (const QString &field : kSourceFields) {
                if (p.contains(field) && sourceColumns.contains(field))
                    source.insert(field, p.value(field));
            }
            source.insert(QStringLiteral("created_at"), QDateTime::currentSecsSinceEpoch());
            if (!insertRow(db, QStringLiteral("sources"), sourceColumns, source)) {
                db.rollback();
                result.error = db.lastError().text();
                return result;
            }
            QSqlQuery last(db);
            last.exec(QStringLiteral("SELECT last_insert_rowid()"));
            id = last.next() ? last.value(0).toInt() : -1;
            ++result.providersAdded;
            result.newSourceIds << id;
        } else {
            ++result.providersMerged;
        }

        for (const QString &table : kSourceTables) {
            for (const QJsonValue &row : p.value(table).toArray())
                insertRow(db, table, tableColumns.value(table), row.toObject(),
                          { { QStringLiteral("source_id"), id } });
        }
    }

    const QSet<QString> radioColumns = columns(db, QStringLiteral("radio_stations"));
    for (const QJsonValue &v : root.value(QStringLiteral("radioStations")).toArray()) {
        if (insertRow(db, QStringLiteral("radio_stations"), radioColumns, v.toObject()))
            ++result.radioStations;
    }

    if (!db.commit()) {
        result.error = db.lastError().text();
        db.rollback();
        return result;
    }
    result.settings = root.value(QStringLiteral("settings")).toObject().toVariantMap();
    result.ok = true;
    return result;
}

} // namespace iptv::Backup
