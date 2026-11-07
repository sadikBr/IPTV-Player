#include "Database.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMutex>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QThread>
#include <QVariant>

namespace iptv {

namespace {

QMutex g_mutex;
QString g_filePath;

QString connectionName()
{
    return QStringLiteral("iptv-%1").arg(quintptr(QThread::currentThreadId()));
}

bool exec(QSqlQuery &q, const QString &sql, QString *error)
{
    if (q.exec(sql))
        return true;
    if (error)
        *error = q.lastError().text() + QStringLiteral(" (") + sql.simplified().left(80)
                + QLatin1Char(')');
    return false;
}

void configure(QSqlDatabase &db)
{
    QSqlQuery q(db);
    q.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
    q.exec(QStringLiteral("PRAGMA busy_timeout = 30000"));
    q.exec(QStringLiteral("PRAGMA synchronous = NORMAL"));
}

// Each entry upgrades the schema by one version. Never edit an entry once
// released; add a new one instead.
const QList<QStringList> &migrations()
{
    static const QList<QStringList> list = {
        // Version 1: sources, categories, channels, favorites
        {
            QStringLiteral(R"(CREATE TABLE sources (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL,
                type TEXT NOT NULL,              -- 'xtream' | 'm3u'
                url TEXT NOT NULL,               -- Xtream server or M3U URL
                username TEXT NOT NULL DEFAULT '',
                password TEXT NOT NULL DEFAULT '',
                output_format TEXT NOT NULL DEFAULT 'ts',
                epg_url TEXT NOT NULL DEFAULT '',
                status TEXT NOT NULL DEFAULT '',
                expires_at INTEGER NOT NULL DEFAULT 0,
                max_connections INTEGER NOT NULL DEFAULT 0,
                live_count INTEGER NOT NULL DEFAULT 0,
                last_sync INTEGER NOT NULL DEFAULT 0,
                created_at INTEGER NOT NULL DEFAULT 0))"),
            QStringLiteral(R"(CREATE TABLE categories (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                source_id INTEGER NOT NULL REFERENCES sources(id) ON DELETE CASCADE,
                kind TEXT NOT NULL,
                ext_id TEXT NOT NULL,
                name TEXT NOT NULL,
                position INTEGER NOT NULL))"),
            QStringLiteral("CREATE INDEX idx_categories_source ON categories(source_id, kind, position)"),
            QStringLiteral(R"(CREATE TABLE channels (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                source_id INTEGER NOT NULL REFERENCES sources(id) ON DELETE CASCADE,
                category_id INTEGER REFERENCES categories(id) ON DELETE SET NULL,
                kind TEXT NOT NULL,
                ext_id TEXT NOT NULL,
                name TEXT NOT NULL,
                search_name TEXT NOT NULL,
                logo TEXT NOT NULL DEFAULT '',
                epg_id TEXT NOT NULL DEFAULT '',
                url TEXT NOT NULL DEFAULT '',
                number INTEGER NOT NULL DEFAULT 0,
                position INTEGER NOT NULL))"),
            QStringLiteral("CREATE INDEX idx_channels_source ON channels(source_id, kind, position)"),
            QStringLiteral("CREATE INDEX idx_channels_category ON channels(category_id, position)"),
            QStringLiteral(R"(CREATE TABLE favorites (
                source_id INTEGER NOT NULL REFERENCES sources(id) ON DELETE CASCADE,
                kind TEXT NOT NULL,
                ext_id TEXT NOT NULL,
                added_at INTEGER NOT NULL,
                PRIMARY KEY (source_id, kind, ext_id)))"),
        },
        // Version 2: TV guide
        {
            QStringLiteral("ALTER TABLE sources ADD COLUMN epg_last_sync INTEGER NOT NULL DEFAULT 0"),
            QStringLiteral("ALTER TABLE sources ADD COLUMN epg_count INTEGER NOT NULL DEFAULT 0"),
            QStringLiteral("ALTER TABLE channels ADD COLUMN epg_key TEXT NOT NULL DEFAULT ''"),
            QStringLiteral(R"(CREATE TABLE epg_channels (
                source_id INTEGER NOT NULL REFERENCES sources(id) ON DELETE CASCADE,
                channel_key TEXT NOT NULL,
                name_key TEXT NOT NULL DEFAULT '',
                PRIMARY KEY (source_id, channel_key, name_key)))"),
            QStringLiteral("CREATE INDEX idx_epg_channels_name ON epg_channels(source_id, name_key)"),
            QStringLiteral(R"(CREATE TABLE epg_programs (
                source_id INTEGER NOT NULL REFERENCES sources(id) ON DELETE CASCADE,
                channel_key TEXT NOT NULL,
                start INTEGER NOT NULL,
                stop INTEGER NOT NULL,
                title TEXT NOT NULL,
                sub_title TEXT NOT NULL DEFAULT '',
                description TEXT NOT NULL DEFAULT ''))"),
            QStringLiteral("CREATE INDEX idx_epg_programs ON epg_programs(source_id, channel_key, start)"),
        },
        // Version 3: movies and series
        {
            QStringLiteral("ALTER TABLE channels ADD COLUMN extension TEXT NOT NULL DEFAULT ''"),
            QStringLiteral("ALTER TABLE channels ADD COLUMN rating TEXT NOT NULL DEFAULT ''"),
            QStringLiteral("ALTER TABLE channels ADD COLUMN year TEXT NOT NULL DEFAULT ''"),
            QStringLiteral("ALTER TABLE channels ADD COLUMN plot TEXT NOT NULL DEFAULT ''"),
            QStringLiteral("ALTER TABLE channels ADD COLUMN genre TEXT NOT NULL DEFAULT ''"),
            QStringLiteral("ALTER TABLE sources ADD COLUMN movie_count INTEGER NOT NULL DEFAULT 0"),
            QStringLiteral("ALTER TABLE sources ADD COLUMN series_count INTEGER NOT NULL DEFAULT 0"),
            QStringLiteral(R"(CREATE TABLE episodes (
                source_id INTEGER NOT NULL REFERENCES sources(id) ON DELETE CASCADE,
                series_ext_id TEXT NOT NULL,
                ext_id TEXT NOT NULL,
                season INTEGER NOT NULL,
                episode INTEGER NOT NULL,
                title TEXT NOT NULL DEFAULT '',
                plot TEXT NOT NULL DEFAULT '',
                image TEXT NOT NULL DEFAULT '',
                duration INTEGER NOT NULL DEFAULT 0,
                extension TEXT NOT NULL DEFAULT '',
                url TEXT NOT NULL DEFAULT '',
                position INTEGER NOT NULL))"),
            QStringLiteral("CREATE INDEX idx_episodes ON episodes(source_id, series_ext_id, position)"),
            // Details fetched per movie/series (Xtream), and when episodes were loaded.
            QStringLiteral(R"(CREATE TABLE vod_details (
                source_id INTEGER NOT NULL REFERENCES sources(id) ON DELETE CASCADE,
                kind TEXT NOT NULL,
                ext_id TEXT NOT NULL,
                plot TEXT NOT NULL DEFAULT '',
                cast_names TEXT NOT NULL DEFAULT '',
                director TEXT NOT NULL DEFAULT '',
                genre TEXT NOT NULL DEFAULT '',
                release_date TEXT NOT NULL DEFAULT '',
                rating TEXT NOT NULL DEFAULT '',
                poster TEXT NOT NULL DEFAULT '',
                backdrop TEXT NOT NULL DEFAULT '',
                duration INTEGER NOT NULL DEFAULT 0,
                extension TEXT NOT NULL DEFAULT '',
                fetched_at INTEGER NOT NULL,
                PRIMARY KEY (source_id, kind, ext_id)))"),
            QStringLiteral(R"(CREATE TABLE watch_progress (
                source_id INTEGER NOT NULL REFERENCES sources(id) ON DELETE CASCADE,
                kind TEXT NOT NULL,              -- 'movie' | 'episode'
                ext_id TEXT NOT NULL,
                position_ms INTEGER NOT NULL,
                duration_ms INTEGER NOT NULL,
                updated_at INTEGER NOT NULL,
                title TEXT NOT NULL DEFAULT '',
                image TEXT NOT NULL DEFAULT '',
                series_ext_id TEXT NOT NULL DEFAULT '',
                season INTEGER NOT NULL DEFAULT 0,
                episode INTEGER NOT NULL DEFAULT 0,
                PRIMARY KEY (source_id, kind, ext_id)))"),
            QStringLiteral("CREATE INDEX idx_watch_progress_recent ON watch_progress(updated_at)"),
        },
        // Version 4: per-provider TV guide time correction
        {
            QStringLiteral("ALTER TABLE sources ADD COLUMN epg_shift_minutes INTEGER NOT NULL DEFAULT 0"),
        },
    };
    return list;
}

bool migrate(QSqlDatabase &db, QString *error)
{
    QSqlQuery q(db);
    if (!exec(q, QStringLiteral("PRAGMA user_version"), error) || !q.next())
        return false;
    int version = q.value(0).toInt();
    const auto &steps = migrations();

    while (version < steps.size()) {
        if (!db.transaction()) {
            if (error)
                *error = db.lastError().text();
            return false;
        }
        for (const QString &sql : steps.at(version)) {
            if (!exec(q, sql, error)) {
                db.rollback();
                return false;
            }
        }
        ++version;
        if (!exec(q, QStringLiteral("PRAGMA user_version = %1").arg(version), error)
            || !db.commit()) {
            db.rollback();
            return false;
        }
    }
    return true;
}

QSqlDatabase open(const QString &name, const QString &path, QString *error)
{
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
    db.setDatabaseName(path);
    if (!db.open()) {
        if (error)
            *error = db.lastError().text();
        return db;
    }
    configure(db);
    return db;
}

} // namespace

bool Database::initialize(const QString &filePath, QString *error)
{
    {
        QMutexLocker lock(&g_mutex);
        g_filePath = filePath;
    }

    if (filePath != QLatin1String(":memory:"))
        QDir().mkpath(QFileInfo(filePath).absolutePath());

    QSqlDatabase db = connection();
    if (!db.isOpen()) {
        if (error)
            *error = db.lastError().text();
        return false;
    }
    if (filePath != QLatin1String(":memory:")) {
        QSqlQuery q(db);
        q.exec(QStringLiteral("PRAGMA journal_mode = WAL"));
    }
    return migrate(db, error);
}

QSqlDatabase Database::connection()
{
    const QString name = connectionName();
    if (QSqlDatabase::contains(name))
        return QSqlDatabase::database(name);

    QString path;
    {
        QMutexLocker lock(&g_mutex);
        path = g_filePath;
    }
    QString error;
    QSqlDatabase db = open(name, path, &error);
    if (!db.isOpen())
        qWarning("Could not open database %s: %s", qPrintable(path), qPrintable(error));
    return db;
}

void Database::closeThreadConnection()
{
    const QString name = connectionName();
    {
        QSqlDatabase db = QSqlDatabase::database(name, false);
        if (db.isValid())
            db.close();
    }
    QSqlDatabase::removeDatabase(name);
}

int Database::schemaVersion()
{
    QSqlQuery q(connection());
    if (q.exec(QStringLiteral("PRAGMA user_version")) && q.next())
        return q.value(0).toInt();
    return 0;
}

int Database::latestSchemaVersion()
{
    return int(migrations().size());
}

DataNotifier *DataNotifier::instance()
{
    static DataNotifier *notifier = new DataNotifier(QCoreApplication::instance());
    return notifier;
}

} // namespace iptv
