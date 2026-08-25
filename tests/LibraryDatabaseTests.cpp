#include "library/LibraryDatabase.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QTemporaryDir>

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

} // namespace yaap
