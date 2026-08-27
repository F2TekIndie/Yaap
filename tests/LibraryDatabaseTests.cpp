#include "library/LibraryDatabase.hpp"
#include "library/LibraryIndexer.hpp"
#include "app/LibraryController.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QTemporaryDir>
#include <QUrl>

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

} // namespace yaap
