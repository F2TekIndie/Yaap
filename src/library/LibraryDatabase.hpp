#pragma once

#include "domain/Playlist.hpp"
#include "domain/Track.hpp"

#include <QSqlDatabase>
#include <QSet>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>
#include <vector>

namespace yaap {

struct LibraryIndexedTrack final {
    Track track;
    qint64 fileSize{-1};
    qint64 modifiedMilliseconds{-1};
};

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
    bool synchronizeFolder(
        const QString& rootPath,
        const std::vector<LibraryIndexedTrack>& tracks,
        const QStringList& observedSources,
        bool removeMissing,
        QString& error);
    bool removeFolder(const QString& rootPath, QString& error);
    [[nodiscard]] QStringList folders(QString& error) const;
    [[nodiscard]] std::vector<Track> tracks(QString& error) const;
    [[nodiscard]] std::vector<LibraryIndexedTrack> indexedTracks(QString& error) const;
    [[nodiscard]] QSet<QString> artworkSources(QString& error) const;
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
    bool upsertFileState(const LibraryIndexedTrack& track, QString& error);

    QString m_connectionName;
    QSqlDatabase m_database;
};

} // namespace yaap
