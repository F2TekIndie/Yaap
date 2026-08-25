#include "library/LibraryDatabase.hpp"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QUuid>

#include <utility>

namespace yaap {
namespace {

[[nodiscard]] QString queryError(const QSqlQuery& query, const QString& operation)
{
    return operation + ": " + query.lastError().text();
}

[[nodiscard]] Track trackFromQuery(const QSqlQuery& query)
{
    return {
        .id = query.value(0).toString().toStdString(),
        .kind = static_cast<TrackKind>(query.value(1).toInt()),
        .providerId = query.value(2).toString().toStdString(),
        .source = query.value(3).toString().toStdString(),
        .title = query.value(4).toString().toStdString(),
        .artist = query.value(5).toString().toStdString(),
        .album = query.value(6).toString().toStdString(),
        .artworkSource = query.value(7).toString().toStdString(),
        .durationMilliseconds = query.value(8).toLongLong()};
}

} // namespace

LibraryDatabase::LibraryDatabase()
    : m_connectionName("yaap-library-" + QUuid::createUuid().toString(QUuid::WithoutBraces))
{
}

LibraryDatabase::~LibraryDatabase()
{
    if (m_database.isValid()) {
        m_database.close();
        m_database = {};
    }
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool LibraryDatabase::open(const QString& path, QString& error)
{
    if (path.isEmpty()) {
        error = "Library database path is empty.";
        return false;
    }
    if (!QSqlDatabase::isDriverAvailable("QSQLITE")) {
        error = "The Qt SQLite driver is not available.";
        return false;
    }

    m_database = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    m_database.setDatabaseName(path);
    m_database.setConnectOptions("QSQLITE_BUSY_TIMEOUT=5000");
    if (!m_database.open()) {
        error = "Could not open the library database: " + m_database.lastError().text();
        return false;
    }
    return initializeSchema(error);
}

bool LibraryDatabase::isOpen() const noexcept
{
    return m_database.isOpen();
}

bool LibraryDatabase::initializeSchema(QString& error)
{
    const QStringList statements{
        "PRAGMA foreign_keys = ON",
        "PRAGMA journal_mode = WAL",
        "CREATE TABLE IF NOT EXISTS tracks ("
            "id TEXT PRIMARY KEY, kind INTEGER NOT NULL, provider_id TEXT NOT NULL, "
            "source TEXT NOT NULL UNIQUE, title TEXT NOT NULL, artist TEXT NOT NULL, "
            "album TEXT NOT NULL, artwork_source TEXT NOT NULL, duration_ms INTEGER NOT NULL, "
            "library_root TEXT NOT NULL)",
        "CREATE INDEX IF NOT EXISTS tracks_library_root_idx ON tracks(library_root)",
        "CREATE TABLE IF NOT EXISTS library_folders (path TEXT PRIMARY KEY)",
        "CREATE TABLE IF NOT EXISTS playlists ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL UNIQUE)",
        "CREATE TABLE IF NOT EXISTS playlist_items ("
            "playlist_id INTEGER NOT NULL REFERENCES playlists(id) ON DELETE CASCADE, "
            "position INTEGER NOT NULL, track_id TEXT NOT NULL REFERENCES tracks(id) ON DELETE CASCADE, "
            "PRIMARY KEY (playlist_id, position))"};

    for (const auto& statement : statements) {
        QSqlQuery query{m_database};
        if (!query.exec(statement)) {
            error = queryError(query, "Could not initialize the library schema");
            return false;
        }
    }
    return true;
}

bool LibraryDatabase::synchronizeFolder(
    const QString& rootPath,
    const std::vector<Track>& tracksToStore,
    QString& error)
{
    if (!m_database.transaction()) {
        error = "Could not begin library synchronization: " + m_database.lastError().text();
        return false;
    }

    const auto rollback = [&] {
        m_database.rollback();
        return false;
    };

    QSqlQuery temporary{m_database};
    if (!temporary.exec("CREATE TEMP TABLE IF NOT EXISTS current_scan_sources (source TEXT PRIMARY KEY)")
        || !temporary.exec("DELETE FROM current_scan_sources")) {
        error = queryError(temporary, "Could not prepare library synchronization");
        return rollback();
    }

    QSqlQuery rememberFolder{m_database};
    rememberFolder.prepare("INSERT OR IGNORE INTO library_folders(path) VALUES(?)");
    rememberFolder.addBindValue(rootPath);
    if (!rememberFolder.exec()) {
        error = queryError(rememberFolder, "Could not store the library folder");
        return rollback();
    }

    QSqlQuery rememberSource{m_database};
    rememberSource.prepare("INSERT INTO current_scan_sources(source) VALUES(?)");
    for (const auto& track : tracksToStore) {
        if (!track.isValid()) {
            error = "Indexer produced an invalid track.";
            return rollback();
        }
        rememberSource.bindValue(0, QString::fromUtf8(track.source));
        if (!rememberSource.exec()) {
            error = queryError(rememberSource, "Could not record an indexed source");
            return rollback();
        }
        if (!upsertTrack(rootPath, track, error)) {
            return rollback();
        }
    }

    QSqlQuery removeMissing{m_database};
    removeMissing.prepare(
        "DELETE FROM tracks WHERE library_root = ? "
        "AND source NOT IN (SELECT source FROM current_scan_sources)");
    removeMissing.addBindValue(rootPath);
    if (!removeMissing.exec()) {
        error = queryError(removeMissing, "Could not remove missing library tracks");
        return rollback();
    }

    if (!m_database.commit()) {
        error = "Could not commit library synchronization: " + m_database.lastError().text();
        return rollback();
    }
    return true;
}

bool LibraryDatabase::removeFolder(const QString& rootPath, QString& error)
{
    if (!m_database.transaction()) {
        error = "Could not begin removing the library folder: " + m_database.lastError().text();
        return false;
    }
    QSqlQuery tracksQuery{m_database};
    tracksQuery.prepare("DELETE FROM tracks WHERE library_root = ?");
    tracksQuery.addBindValue(rootPath);
    if (!tracksQuery.exec()) {
        error = queryError(tracksQuery, "Could not remove tracks from the library folder");
        m_database.rollback();
        return false;
    }
    QSqlQuery folderQuery{m_database};
    folderQuery.prepare("DELETE FROM library_folders WHERE path = ?");
    folderQuery.addBindValue(rootPath);
    if (!folderQuery.exec()) {
        error = queryError(folderQuery, "Could not remove the library folder");
        m_database.rollback();
        return false;
    }
    if (!m_database.commit()) {
        error = "Could not commit removal of the library folder: " + m_database.lastError().text();
        m_database.rollback();
        return false;
    }
    return true;
}

QStringList LibraryDatabase::folders(QString& error) const
{
    QSqlQuery query{m_database};
    if (!query.exec("SELECT path FROM library_folders ORDER BY path COLLATE NOCASE")) {
        error = queryError(query, "Could not load library folders");
        return {};
    }
    QStringList result;
    while (query.next()) {
        result.push_back(query.value(0).toString());
    }
    return result;
}

bool LibraryDatabase::upsertTrack(
    const QString& rootPath,
    const Track& track,
    QString& error)
{
    QSqlQuery query{m_database};
    query.prepare(
        "INSERT INTO tracks(id, kind, provider_id, source, title, artist, album, "
        "artwork_source, duration_ms, library_root) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(id) DO UPDATE SET kind=excluded.kind, provider_id=excluded.provider_id, "
        "source=excluded.source, title=excluded.title, artist=excluded.artist, "
        "album=excluded.album, artwork_source=excluded.artwork_source, "
        "duration_ms=excluded.duration_ms, library_root=excluded.library_root");
    query.addBindValue(QString::fromUtf8(track.id));
    query.addBindValue(static_cast<int>(track.kind));
    query.addBindValue(QString::fromUtf8(track.providerId));
    query.addBindValue(QString::fromUtf8(track.source));
    query.addBindValue(QString::fromUtf8(track.title));
    query.addBindValue(QString::fromUtf8(track.artist));
    query.addBindValue(QString::fromUtf8(track.album));
    query.addBindValue(QString::fromUtf8(track.artworkSource));
    query.addBindValue(track.durationMilliseconds);
    query.addBindValue(rootPath);
    if (!query.exec()) {
        error = queryError(query, "Could not store an indexed track");
        return false;
    }
    return true;
}

std::vector<Track> LibraryDatabase::tracks(QString& error) const
{
    QSqlQuery query{m_database};
    if (!query.exec(
            "SELECT id, kind, provider_id, source, title, artist, album, artwork_source, duration_ms "
            "FROM tracks ORDER BY artist COLLATE NOCASE, album COLLATE NOCASE, title COLLATE NOCASE")) {
        error = queryError(query, "Could not load library tracks");
        return {};
    }

    std::vector<Track> result;
    while (query.next()) {
        result.push_back(trackFromQuery(query));
    }
    return result;
}

std::optional<Track> LibraryDatabase::trackById(const std::string& id, QString& error) const
{
    QSqlQuery query{m_database};
    query.prepare(
        "SELECT id, kind, provider_id, source, title, artist, album, artwork_source, duration_ms "
        "FROM tracks WHERE id = ?");
    query.addBindValue(QString::fromUtf8(id));
    if (!query.exec()) {
        error = queryError(query, "Could not load a library track");
        return std::nullopt;
    }
    return query.next() ? std::optional<Track>{trackFromQuery(query)} : std::nullopt;
}

std::int64_t LibraryDatabase::createPlaylist(const QString& name, QString& error)
{
    if (name.trimmed().isEmpty()) {
        error = "Playlist name cannot be empty.";
        return 0;
    }
    QSqlQuery query{m_database};
    query.prepare("INSERT INTO playlists(name) VALUES(?)");
    query.addBindValue(name.trimmed());
    if (!query.exec()) {
        error = queryError(query, "Could not create the playlist");
        return 0;
    }
    return query.lastInsertId().toLongLong();
}

bool LibraryDatabase::setPlaylistTracks(
    const std::int64_t playlistId,
    const std::vector<std::string>& trackIds,
    QString& error)
{
    if (!m_database.transaction()) {
        error = "Could not begin playlist update: " + m_database.lastError().text();
        return false;
    }
    QSqlQuery clear{m_database};
    clear.prepare("DELETE FROM playlist_items WHERE playlist_id = ?");
    clear.addBindValue(playlistId);
    if (!clear.exec()) {
        error = queryError(clear, "Could not clear the playlist");
        m_database.rollback();
        return false;
    }

    QSqlQuery insert{m_database};
    insert.prepare("INSERT INTO playlist_items(playlist_id, position, track_id) VALUES(?, ?, ?)");
    for (std::size_t index = 0; index < trackIds.size(); ++index) {
        insert.bindValue(0, playlistId);
        insert.bindValue(1, static_cast<qlonglong>(index));
        insert.bindValue(2, QString::fromUtf8(trackIds[index]));
        if (!insert.exec()) {
            error = queryError(insert, "Could not add a track to the playlist");
            m_database.rollback();
            return false;
        }
    }
    if (!m_database.commit()) {
        error = "Could not commit the playlist: " + m_database.lastError().text();
        m_database.rollback();
        return false;
    }
    return true;
}

std::vector<Playlist> LibraryDatabase::playlists(QString& error) const
{
    QSqlQuery query{m_database};
    if (!query.exec(
            "SELECT p.id, p.name, i.track_id FROM playlists p "
            "LEFT JOIN playlist_items i ON i.playlist_id = p.id "
            "ORDER BY p.name COLLATE NOCASE, i.position")) {
        error = queryError(query, "Could not load playlists");
        return {};
    }

    std::vector<Playlist> result;
    while (query.next()) {
        const auto id = query.value(0).toLongLong();
        if (result.empty() || result.back().id != id) {
            result.push_back({.id = id, .name = query.value(1).toString().toStdString()});
        }
        if (!query.value(2).isNull()) {
            result.back().trackIds.push_back(query.value(2).toString().toStdString());
        }
    }
    return result;
}

} // namespace yaap
