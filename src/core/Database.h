#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>

namespace iptv {

// SQLite storage shared by the whole app.
//
// Each thread gets its own connection (SQLite connections must not be shared
// between threads). The database runs in WAL mode so the UI can keep reading
// while a background sync writes.
class Database
{
public:
    // Opens (creating if needed) the database file and migrates the schema.
    // Use ":memory:" for tests. Must be called on the main thread first.
    static bool initialize(const QString &filePath, QString *error = nullptr);

    // Connection for the calling thread, opened on first use.
    static QSqlDatabase connection();

    // Worker threads call this when done so the connection is released.
    static void closeThreadConnection();

    static int schemaVersion();
};

// Broadcasts data changes so every model can refresh.
class DataNotifier : public QObject
{
    Q_OBJECT

public:
    static DataNotifier *instance();

signals:
    void sourcesChanged();
    void contentChanged(int sourceId);
    void favoritesChanged(int sourceId);

private:
    using QObject::QObject;
};

} // namespace iptv
