#include "library/LibraryDatabase.hpp"
#include "library/LibraryIndexer.hpp"
#include "app/LibraryController.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QTemporaryDir>
#include <QUrl>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <QFile>
#include <QDir>

namespace yaap {

TEST_CASE("Library synchronization updates metadata and removes missing tracks")
{
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    LibraryDatabase database;
    QString error;
    REQUIRE(database.open(directory.filePath("library.sqlite3"), error));

    Track first{.id = "one", .source = "file:///one.mp3", .title = "One"};
    Track second{.id = "two", .source = "file:///two.mp3", .title = "Two"};
    REQUIRE(database.synchronizeFolder("/music", {first, second}, error));
    REQUIRE(database.tracks(error).size() == 2);

    first.title = "One (updated)";
    REQUIRE(database.synchronizeFolder("/music", {first}, error));
    const auto tracks = database.tracks(error);
    REQUIRE(tracks.size() == 1);
    CHECK(tracks.front().title == "One (updated)");
}

TEST_CASE("Library playlists retain track order")
{
    QTemporaryDir directory;
    LibraryDatabase database;
    QString error;
    REQUIRE(database.open(directory.filePath("library.sqlite3"), error));
    REQUIRE(database.synchronizeFolder("/music", {
        {.id = "one", .source = "file:///one.mp3", .title = "One"},
        {.id = "two", .source = "file:///two.mp3", .title = "Two"}}, error));

    const auto playlistId = database.createPlaylist("Road trip", error);
    REQUIRE(playlistId > 0);
    REQUIRE(database.setPlaylistTracks(playlistId, {"two", "one"}, error));
    const auto playlists = database.playlists(error);
    REQUIRE(playlists.size() == 1);
    CHECK(playlists.front().trackIds == std::vector<std::string>{"two", "one"});
}

TEST_CASE("Incomplete scans and metadata failures preserve indexed tracks and playlists")
{
    QTemporaryDir directory;
    LibraryDatabase database;
    QString error;
    REQUIRE(database.open(directory.filePath("library.sqlite3"), error));
    const Track first{.id = "one", .source = "file:///one.mp3", .title = "One"};
    const Track second{.id = "two", .source = "file:///two.mp3", .title = "Two"};
    REQUIRE(database.synchronizeFolder("/music", {first, second}, error));
    const auto playlistId = database.createPlaylist("Keep me", error);
    REQUIRE(database.setPlaylistTracks(playlistId, {"one", "two"}, error));

    REQUIRE(database.synchronizeFolder("/music", {},
        {QString::fromUtf8(first.source)}, true, error));
    REQUIRE(database.tracks(error).size() == 1);
    REQUIRE(database.playlists(error).front().trackIds
        == std::vector<std::string>{"one"});

    REQUIRE(database.synchronizeFolder("/music", {}, {}, false, error));
    REQUIRE(database.tracks(error).size() == 1);
    REQUIRE(database.playlists(error).front().trackIds
        == std::vector<std::string>{"one"});
}

TEST_CASE("Library file state supports incremental metadata scans")
{
    QTemporaryDir directory;
    LibraryDatabase database;
    QString error;
    REQUIRE(database.open(directory.filePath("library.sqlite3"), error));
    LibraryIndexedTrack indexed{
        {.id = "one", .source = "file:///one.mp3", .title = "One"}, 123, 456};
    REQUIRE(database.synchronizeFolder("/music", {indexed},
        {QString::fromUtf8(indexed.track.source)}, true, error));
    const auto restored = database.indexedTracks(error);
    REQUIRE(restored.size() == 1);
    CHECK(restored.front().fileSize == 123);
    CHECK(restored.front().modifiedMilliseconds == 456);
}

TEST_CASE("Library playback preserves the stored local file URL")
{
    QTemporaryDir directory;
    LibraryDatabase database;
    QString error;
    REQUIRE(database.open(directory.filePath("library.sqlite3"), error));
    const auto source = QUrl::fromLocalFile(directory.filePath("song.mp3"));
    REQUIRE(database.synchronizeFolder(directory.path(), {{
        .id = "one", .source = source.toString(QUrl::FullyEncoded).toStdString(),
        .title = "One"}}, error));
    LibraryIndexer indexer{database, directory.filePath("artwork")};
    LibraryController controller{database, indexer};
    QUrl requested;
    QObject::connect(&controller, &LibraryController::playbackRequested,
        [&requested](const QUrl& url, const QString&) { requested = url; });

    controller.play(0);
    CHECK(requested == source);
    CHECK(requested.toLocalFile() == directory.filePath("song.mp3"));
}


TEST_CASE("Removing a folder rejects the in-flight scan snapshot")
{
    QTemporaryDir directory;
    LibraryDatabase database;
    QString error;
    REQUIRE(database.open(directory.filePath("library.sqlite3"), error));
    const auto root = directory.filePath("music");
    REQUIRE(QDir{}.mkpath(root));
    REQUIRE(database.synchronizeFolder(root, {{.id = "one",
        .source = "file:///one.mp3", .title = "One"}}, error));
    LibraryIndexer indexer{database, directory.filePath("artwork")};
    bool finished = false;
    QObject::connect(&indexer, &LibraryIndexer::scanFinished, [&] { finished = true; });
    // Constructor starts a scan, but its queued completion cannot run until
    // events are processed below. Remove the root before that completion.
    REQUIRE(indexer.removeFolder(root, error));
    QElapsedTimer timer;
    timer.start();
    while (!finished && timer.elapsed() < 5000) {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    REQUIRE(finished);
    CHECK(database.folders(error).isEmpty());
    CHECK(database.tracks(error).empty());
}

#ifndef Q_OS_WIN
TEST_CASE("An unreadable child directory preserves tracks and playlist membership")
{
    QTemporaryDir directory;
    const auto root = directory.filePath("music");
    const auto child = root + "/album";
    REQUIRE(QDir{}.mkpath(child));
    struct RestorePermissions {
        QString path;
        ~RestorePermissions() { QFile::setPermissions(path,
            QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner); }
    } restore{child};
    REQUIRE(QFile::setPermissions(child, QFile::Permissions{}));
    if (QFileInfo{child}.isReadable()) {
        SKIP("This user can bypass directory permissions.");
    }
    LibraryDatabase database;
    QString error;
    REQUIRE(database.open(directory.filePath("library.sqlite3"), error));
    const auto source = QUrl::fromLocalFile(child + "/song.mp3").toString();
    REQUIRE(database.synchronizeFolder(root, {{.id = "one",
        .source = source.toStdString(), .title = "One"}}, error));
    const auto playlist = database.createPlaylist("Keep", error);
    REQUIRE(database.setPlaylistTracks(playlist, {"one"}, error));
    LibraryIndexer indexer{database, directory.filePath("artwork")};
    bool finished = false;
    bool warned = false;
    QObject::connect(&indexer, &LibraryIndexer::scanFinished, [&] { finished = true; });
    QObject::connect(&indexer, &LibraryIndexer::scanWarning, [&] { warned = true; });
    QElapsedTimer timer;
    timer.start();
    while (!finished && timer.elapsed() < 5000) {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    REQUIRE(finished);
    CHECK(warned);
    CHECK(database.tracks(error).size() == 1);
    REQUIRE(database.playlists(error).size() == 1);
    CHECK(database.playlists(error).front().trackIds == std::vector<std::string>{"one"});
}
#endif

} // namespace yaap
