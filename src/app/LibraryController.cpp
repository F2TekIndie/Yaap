#include "app/LibraryController.hpp"

#include "library/LibraryDatabase.hpp"
#include "library/LibraryIndexer.hpp"

#include <utility>

namespace yaap {

LibraryController::LibraryController(
    LibraryDatabase& database,
    LibraryIndexer& indexer,
    QObject* parent)
    : QAbstractListModel(parent)
    , m_database(database)
    , m_indexer(indexer)
{
    connect(&m_indexer, &LibraryIndexer::scanFinished, this, [this] {
        m_scanning = false;
        emit scanningChanged();
        reload();
    });
    connect(&m_indexer, &LibraryIndexer::scanFailed, this, [this](const QString& error) {
        m_scanning = false;
        emit scanningChanged();
        setError(error);
    });
    connect(&m_indexer, &LibraryIndexer::scanWarning, this, [this](const QString& warning) {
        setError(warning);
    });
    reload();
}

int LibraryController::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_tracks.size());
}

QVariant LibraryController::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0
        || index.row() >= static_cast<int>(m_tracks.size())) {
        return {};
    }
    const auto& track = m_tracks[static_cast<std::size_t>(index.row())];
    switch (role) {
    case TrackIdRole: return QString::fromUtf8(track.id);
    case TitleRole: return QString::fromUtf8(track.title);
    case ArtistRole: return QString::fromUtf8(track.artist);
    case AlbumRole: return QString::fromUtf8(track.album);
    case SourceRole: return QString::fromUtf8(track.source);
    case ArtworkRole: return QString::fromUtf8(track.artworkSource);
    case DurationRole: return static_cast<qlonglong>(track.durationMilliseconds);
    default: return {};
    }
}

QHash<int, QByteArray> LibraryController::roleNames() const
{
    return {{TrackIdRole, "trackId"}, {TitleRole, "title"}, {ArtistRole, "artist"},
        {AlbumRole, "album"}, {SourceRole, "source"}, {ArtworkRole, "artwork"},
        {DurationRole, "durationMilliseconds"}};
}

int LibraryController::trackCount() const noexcept
{
    return static_cast<int>(m_tracks.size());
}

bool LibraryController::scanning() const noexcept
{
    return m_scanning;
}

QStringList LibraryController::folders() const
{
    return m_indexer.folders();
}

QString LibraryController::errorMessage() const
{
    return m_errorMessage;
}

void LibraryController::addFolder(const QUrl& folder)
{
    QString error;
    if (!folder.isLocalFile() || !m_indexer.addFolder(folder.toLocalFile(), error)) {
        setError(error.isEmpty() ? "A local library folder is required." : error);
        return;
    }
    m_scanning = true;
    emit scanningChanged();
    emit foldersChanged();
}

void LibraryController::removeFolder(const QUrl& folder)
{
    QString error;
    if (!folder.isLocalFile() || !m_indexer.removeFolder(folder.toLocalFile(), error)) {
        setError(error.isEmpty() ? "A local library folder is required." : error);
        return;
    }
    emit foldersChanged();
    reload();
}

void LibraryController::rescan()
{
    m_scanning = true;
    emit scanningChanged();
    m_indexer.rescan();
}

qint64 LibraryController::createPlaylist(const QString& name)
{
    QString error;
    const auto id = m_database.createPlaylist(name, error);
    if (id == 0) {
        setError(error);
    }
    return id;
}

void LibraryController::play(const int row)
{
    if (row < 0 || row >= static_cast<int>(m_tracks.size())) {
        return;
    }
    const auto& track = m_tracks[static_cast<std::size_t>(row)];
    emit playbackRequested(QUrl{QString::fromUtf8(track.source)},
        QString::fromStdString(track.title), QUrl{QString::fromStdString(track.artworkSource)});
}

void LibraryController::reload()
{
    QString error;
    auto tracks = m_database.tracks(error);
    if (!error.isEmpty()) {
        setError(error);
        return;
    }
    beginResetModel();
    m_tracks = std::move(tracks);
    endResetModel();
    emit tracksChanged();
}

void LibraryController::setError(QString error)
{
    m_errorMessage = std::move(error);
    emit errorMessageChanged();
}

} // namespace yaap
