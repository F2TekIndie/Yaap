#pragma once

#include "library/LibraryDatabase.hpp"

#include <QFileSystemWatcher>
#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <vector>
#include <unordered_map>

namespace yaap {

struct LibraryFolderScan final {
    QString rootPath;
    std::vector<LibraryIndexedTrack> tracks;
    QStringList observedSources;
    bool complete{true};
};

struct LibraryScanBatch final {
    std::vector<LibraryFolderScan> folders;
    QStringList watchedDirectories;
    QStringList warnings;
};

class LibraryIndexer final : public QObject {
    Q_OBJECT

public:
    LibraryIndexer(
        LibraryDatabase& database,
        QString artworkCachePath,
        QObject* parent = nullptr);

    bool addFolder(const QString& path, QString& error);
    bool removeFolder(const QString& path, QString& error);
    [[nodiscard]] QStringList folders() const;

public slots:
    void rescan();

signals:
    void scanFinished(int trackCount);
    void scanWarning(const QString& warning);
    void scanFailed(const QString& error);

private:
    static LibraryScanBatch scanFolders(
        const QStringList& roots,
        const QString& artworkCachePath,
        const std::unordered_map<std::string, LibraryIndexedTrack>& previousTracks);
    void applyScan();
    void pruneArtworkCache();

    LibraryDatabase& m_database;
    QString m_artworkCachePath;
    QStringList m_folders;
    QFileSystemWatcher m_watcher;
    QTimer m_rescanDebounce;
    QFutureWatcher<LibraryScanBatch> m_scanWatcher;
    bool m_rescanPending{};
};

} // namespace yaap
