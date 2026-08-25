#pragma once

#include "domain/Playlist.hpp"
#include "domain/Track.hpp"

#include <QSqlDatabase>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>
#include <vector>

namespace yaap {

class LibraryDatabase final {
public:
    LibraryDatabase();
    ~LibraryDatabase();

    LibraryDatabase(const LibraryDatabase&) = delete;
    LibraryDatabase& operator=(const LibraryDatabase&) = delete;
    LibraryDatabase(LibraryDatabase&&) = delete;
    LibraryDatabase& operator=(LibraryDatabase&&) = delete;

    bool open(const QString& path, QString& error);
    [[nodiscard]] bool isOpen() const noexcept;

    bool synchronizeFolder(
        const QString& rootPath,
        const std::vector<Track>& tracks,
        QString& error);
    bool removeFolder(const QString& rootPath, QString& error);
    [[nodiscard]] QStringList folders(QString& error) const;
    [[nodiscard]] std::vector<Track> tracks(QString& error) const;
    [[nodiscard]] std::optional<Track> trackById(
        const std::string& id,
        QString& error) const;

    [[nodiscard]] std::int64_t createPlaylist(const QString& name, QString& error);
    bool setPlaylistTracks(
        std::int64_t playlistId,
        const std::vector<std::string>& trackIds,
        QString& error);
    [[nodiscard]] std::vector<Playlist> playlists(QString& error) const;

private:
    bool initializeSchema(QString& error);
    bool upsertTrack(
        const QString& rootPath,
        const Track& track,
        QString& error);

    QString m_connectionName;
    QSqlDatabase m_database;
};

} // namespace yaap
