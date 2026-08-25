#include "library/LibraryIndexer.hpp"

#include "audio/MediaMetadata.hpp"
#include "library/LibraryDatabase.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSaveFile>
#include <QSet>
#include <QUrl>
#include <QtConcurrentRun>

#include <algorithm>
#include <filesystem>
#include <utility>

namespace yaap {
namespace {

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
    m_scanWatcher.setFuture(QtConcurrent::run([roots, cachePath] {
        return scanFolders(roots, cachePath);
    }));
}

LibraryScanBatch LibraryIndexer::scanFolders(
    const QStringList& roots,
    const QString& artworkCachePath)
{
    LibraryScanBatch batch;
    MediaMetadataReader metadataReader;

    for (const auto& root : roots) {
        LibraryFolderScan folder{.rootPath = root};
        QSet<QString> watchedDirectories{root};
        QDirIterator iterator{root, QDir::Files, QDirIterator::Subdirectories};
        while (iterator.hasNext()) {
            iterator.next();
            const QFileInfo file = iterator.fileInfo();
            watchedDirectories.insert(file.absolutePath());
            if (!isSupportedAudioFile(file)) {
                continue;
            }

            const auto source = QUrl::fromLocalFile(file.absoluteFilePath())
                                    .toString(QUrl::FullyEncoded);
            const auto id = stableId(source);
            const auto metadataResult = metadataReader.read(
                filesystemPath(file.absoluteFilePath()));
            if (!metadataResult.succeeded()) {
                batch.warnings.push_back(
                    file.absoluteFilePath() + ": " + QString::fromUtf8(metadataResult.error));
                continue;
            }

            const auto artwork = storeArtwork(
                artworkCachePath, id, metadataResult.metadata);
            folder.tracks.push_back({
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
                .durationMilliseconds = metadataResult.metadata.durationMilliseconds});
        }
        batch.watchedDirectories.append(watchedDirectories.values());
        batch.folders.push_back(std::move(folder));
    }
    batch.watchedDirectories.removeDuplicates();
    return batch;
}

void LibraryIndexer::applyScan()
{
    const auto batch = m_scanWatcher.result();
    int trackCount = 0;
    for (const auto& folder : batch.folders) {
        QString error;
        if (!m_database.synchronizeFolder(folder.rootPath, folder.tracks, error)) {
            emit scanFailed(error);
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
        // PROTOTYPE: Watch exhaustion falls back to manual/rescheduled full scans.
        emit scanWarning("Could not watch " + QString::number(failedWatches.size())
            + " library directories; rescans remain available.");
    }
    emit scanFinished(trackCount);

    if (m_rescanPending) {
        m_rescanPending = false;
        rescan();
    }
}

} // namespace yaap
