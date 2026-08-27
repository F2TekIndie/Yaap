#include "library/LibraryIndexer.hpp"

#include "audio/MediaMetadata.hpp"
#include "library/LibraryDatabase.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QSaveFile>
#include <QSet>
#include <QUrl>
#include <QtConcurrentRun>

#include <algorithm>
#include <filesystem>
#include <utility>
#include <unordered_map>

namespace yaap {
namespace {

constexpr qsizetype maximumWatchedDirectories = 2'048;
constexpr qsizetype maximumScanWarnings = 200;

[[nodiscard]] bool isSupportedAudioFile(const QFileInfo& file)
{
    static const QSet<QString> extensions{
        "aac", "flac", "m4a", "mp3", "ogg", "opus", "wav", "wma"};
    return extensions.contains(file.suffix().toLower());
}

[[nodiscard]] QString stableId(const QString& source)
{
    return "local:" + QString::fromLatin1(QCryptographicHash::hash(
        source.toUtf8(), QCryptographicHash::Sha256).toHex());
}

[[nodiscard]] std::filesystem::path filesystemPath(const QString& path)
{
#ifdef _WIN32
    return std::filesystem::path{path.toStdWString()};
#else
    const auto encoded = path.toUtf8();
    return std::filesystem::path{
        std::string{encoded.constData(), static_cast<std::size_t>(encoded.size())}};
#endif
}

[[nodiscard]] QString storeArtwork(
    const QString& cachePath,
    const QString& id,
    const MediaMetadata& metadata)
{
    if (metadata.artwork.empty()) {
        return {};
    }
    const auto extension = metadata.artworkMimeType == "image/png" ? ".png" : ".jpg";
    const auto filePath = QDir{cachePath}.filePath(id.mid(id.indexOf(':') + 1) + extension);
    QSaveFile output{filePath};
    if (!output.open(QIODevice::WriteOnly)) {
        return {};
    }
    if (output.write(
            reinterpret_cast<const char*>(metadata.artwork.data()),
            static_cast<qint64>(metadata.artwork.size()))
        != static_cast<qint64>(metadata.artwork.size())
        || !output.commit()) {
        return {};
    }
    return QUrl::fromLocalFile(filePath).toString(QUrl::FullyEncoded);
}

} // namespace

LibraryIndexer::LibraryIndexer(
    LibraryDatabase& database,
    QString artworkCachePath,
    QObject* parent)
    : QObject(parent)
    , m_database(database)
    , m_artworkCachePath(std::move(artworkCachePath))
{
    QDir{}.mkpath(m_artworkCachePath);
    QString error;
    m_folders = m_database.folders(error);
    if (!error.isEmpty()) {
        emit scanFailed(error);
    }

    m_rescanDebounce.setSingleShot(true);
    m_rescanDebounce.setInterval(500);
    connect(&m_rescanDebounce, &QTimer::timeout, this, &LibraryIndexer::rescan);
    connect(
        &m_watcher,
        &QFileSystemWatcher::directoryChanged,
        &m_rescanDebounce,
        qOverload<>(&QTimer::start));
    connect(&m_scanWatcher, &QFutureWatcher<LibraryScanBatch>::finished, this, &LibraryIndexer::applyScan);

    if (!m_folders.isEmpty()) {
        rescan();
    }
}

bool LibraryIndexer::addFolder(const QString& path, QString& error)
{
    const auto canonicalPath = QFileInfo{path}.canonicalFilePath();
    if (canonicalPath.isEmpty() || !QFileInfo{canonicalPath}.isDir()) {
        error = "Library folder does not exist: " + path;
        return false;
    }
    if (!m_folders.contains(canonicalPath)) {
        m_folders.push_back(canonicalPath);
        rescan();
    }
    return true;
}

bool LibraryIndexer::removeFolder(const QString& path, QString& error)
{
    const auto cleanPath = QDir::cleanPath(path);
    auto iterator = std::find_if(m_folders.begin(), m_folders.end(), [&](const QString& root) {
        return QDir::cleanPath(root) == cleanPath;
    });
    if (iterator == m_folders.end()) {
        return true;
    }
    const auto storedPath = *iterator;
    if (!m_database.removeFolder(storedPath, error)) {
        return false;
    }
    m_folders.erase(iterator);
    rescan();
    return true;
}

QStringList LibraryIndexer::folders() const
{
    return m_folders;
}

void LibraryIndexer::rescan()
{
    if (m_scanWatcher.isRunning()) {
        m_rescanPending = true;
        return;
    }
    const auto roots = m_folders;
    const auto cachePath = m_artworkCachePath;
    QString error;
    auto indexedTracks = m_database.indexedTracks(error);
    if (!error.isEmpty()) {
        emit scanFailed(error);
        return;
    }
    std::unordered_map<std::string, LibraryIndexedTrack> previousTracks;
    previousTracks.reserve(indexedTracks.size());
    for (auto& indexedTrack : indexedTracks) {
        previousTracks.emplace(indexedTrack.track.source, std::move(indexedTrack));
    }
    m_scanWatcher.setFuture(QtConcurrent::run(
        [roots, cachePath, previousTracks = std::move(previousTracks)] {
        return scanFolders(roots, cachePath, previousTracks);
    }));
}

LibraryScanBatch LibraryIndexer::scanFolders(
    const QStringList& roots,
    const QString& artworkCachePath,
    const std::unordered_map<std::string, LibraryIndexedTrack>& previousTracks)
{
    LibraryScanBatch batch;
    MediaMetadataReader metadataReader;

    for (const auto& root : roots) {
        LibraryFolderScan folder{.rootPath = root};
        QSet<QString> watchedDirectories{root};
        const QFileInfo rootInfo{root};
        if (!rootInfo.exists() || !rootInfo.isDir() || !rootInfo.isReadable()) {
            folder.complete = false;
            batch.warnings.push_back(
                "Library folder is unavailable; keeping its indexed tracks: " + root);
            batch.folders.push_back(std::move(folder));
            continue;
        }
        QDirIterator iterator{root, QDir::Files, QDirIterator::Subdirectories};
        while (iterator.hasNext()) {
            iterator.next();
            const QFileInfo file = iterator.fileInfo();
            if (!isSupportedAudioFile(file)) {
                continue;
            }
            watchedDirectories.insert(file.absolutePath());

            const auto source = QUrl::fromLocalFile(file.absoluteFilePath())
                                    .toString(QUrl::FullyEncoded);
            folder.observedSources.push_back(source);
            const auto sourceKey = source.toStdString();
            const auto previous = previousTracks.find(sourceKey);
            const auto fileSize = file.size();
            const auto modifiedMilliseconds = file.lastModified().toMSecsSinceEpoch();
            if (previous != previousTracks.end()
                && previous->second.fileSize == fileSize
                && previous->second.modifiedMilliseconds == modifiedMilliseconds) {
                folder.tracks.push_back(previous->second);
                continue;
            }
            const auto id = stableId(source);
            const auto metadataResult = metadataReader.read(
                filesystemPath(file.absoluteFilePath()));
            if (!metadataResult.succeeded()) {
                if (batch.warnings.size() < maximumScanWarnings) {
                    batch.warnings.push_back(file.absoluteFilePath() + ": "
                        + QString::fromUtf8(metadataResult.error));
                }
                // Preserve the prior row and its file-state timestamp so the
                // next scan retries metadata instead of treating this as a deletion.
                if (previous != previousTracks.end()) {
                    folder.tracks.push_back(previous->second);
                }
                continue;
            }

            const auto artwork = storeArtwork(
                artworkCachePath, id, metadataResult.metadata);
            folder.tracks.push_back({{
                .id = id.toStdString(),
                .kind = TrackKind::LocalFile,
                .providerId = "local",
                .source = source.toStdString(),
                .title = metadataResult.metadata.title.empty()
                    ? file.completeBaseName().toStdString()
                    : metadataResult.metadata.title,
                .artist = metadataResult.metadata.artist,
                .album = metadataResult.metadata.album,
                .artworkSource = artwork.toStdString(),
                .durationMilliseconds = metadataResult.metadata.durationMilliseconds},
                fileSize, modifiedMilliseconds});
        }
        if (!QFileInfo{root}.exists()) {
            folder.complete = false;
            batch.warnings.push_back(
                "Library folder became unavailable during its scan; keeping prior entries: " + root);
        }
        const auto availableWatchSlots = std::max<qsizetype>(
            maximumWatchedDirectories - batch.watchedDirectories.size(), 0);
        auto directories = watchedDirectories.values();
        if (directories.size() > availableWatchSlots) {
            directories = directories.mid(0, availableWatchSlots);
            if (batch.warnings.size() < maximumScanWarnings) {
                batch.warnings.push_back("Library watch limit reached; use Rescan for changes "
                    "outside the first 2048 directories.");
            }
        }
        batch.watchedDirectories.append(directories);
        batch.folders.push_back(std::move(folder));
    }
    batch.watchedDirectories.removeDuplicates();
    return batch;
}

void LibraryIndexer::applyScan()
{
    auto batch = m_scanWatcher.future().takeResult();
    int trackCount = 0;
    bool synchronized = true;
    for (const auto& folder : batch.folders) {
        QString error;
        if (!m_database.synchronizeFolder(folder.rootPath, folder.tracks,
                folder.observedSources, folder.complete, error)) {
            emit scanFailed(error);
            synchronized = false;
            continue;
        }
        trackCount += static_cast<int>(folder.tracks.size());
    }
    for (const auto& warning : batch.warnings) {
        emit scanWarning(warning);
    }

    const auto watched = m_watcher.directories();
    if (!watched.isEmpty()) {
        m_watcher.removePaths(watched);
    }
    const auto failedWatches = m_watcher.addPaths(batch.watchedDirectories);
    if (!failedWatches.isEmpty()) {
        emit scanWarning("Could not watch " + QString::number(failedWatches.size())
            + " library directories; rescans remain available.");
    }
    if (synchronized) {
        pruneArtworkCache();
    }
    emit scanFinished(trackCount);

    if (m_rescanPending) {
        m_rescanPending = false;
        rescan();
    }
}

void LibraryIndexer::pruneArtworkCache()
{
    QString error;
    const auto artworkSources = m_database.artworkSources(error);
    if (!error.isEmpty()) {
        emit scanWarning(error);
        return;
    }
    QSet<QString> referencedFiles;
    for (const auto& source : artworkSources) {
        const QUrl url{source};
        if (url.isLocalFile()) {
            referencedFiles.insert(QFileInfo{url.toLocalFile()}.absoluteFilePath());
        }
    }
    const QDir cache{m_artworkCachePath};
    for (const auto& file : cache.entryInfoList(QDir::Files | QDir::NoSymLinks)) {
        if (!referencedFiles.contains(file.absoluteFilePath())
            && !QFile::remove(file.absoluteFilePath())) {
            emit scanWarning("Could not remove unreferenced artwork: " + file.absoluteFilePath());
        }
    }
}

} // namespace yaap
