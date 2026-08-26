#pragma once

#include <QAbstractListModel>
#include <QStringList>
#include <QUrl>

#include <vector>

#include "domain/Track.hpp"

namespace yaap {

class LibraryDatabase;
class LibraryIndexer;

class LibraryController final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int trackCount READ trackCount NOTIFY tracksChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(QStringList folders READ folders NOTIFY foldersChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

public:
    enum Role {
        TrackIdRole = Qt::UserRole + 1,
        TitleRole,
        ArtistRole,
        AlbumRole,
        SourceRole,
        ArtworkRole,
        DurationRole,
    };
    Q_ENUM(Role)

    LibraryController(
        LibraryDatabase& database,
        LibraryIndexer& indexer,
        QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] int trackCount() const noexcept;
    [[nodiscard]] bool scanning() const noexcept;
    [[nodiscard]] QStringList folders() const;
    [[nodiscard]] QString errorMessage() const;

    Q_INVOKABLE void addFolder(const QUrl& folder);
    Q_INVOKABLE void removeFolder(const QUrl& folder);
    Q_INVOKABLE void rescan();
    Q_INVOKABLE qint64 createPlaylist(const QString& name);
    Q_INVOKABLE void play(int row);

signals:
    void tracksChanged();
    void scanningChanged();
    void foldersChanged();
    void errorMessageChanged();
    void playbackRequested(const QUrl& url, const QString& title);

private:
    void reload();
    void setError(QString error);

    LibraryDatabase& m_database;
    LibraryIndexer& m_indexer;
    std::vector<Track> m_tracks;
    bool m_scanning{};
    QString m_errorMessage;
};

} // namespace yaap
