#include "providers/ProviderClients.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QUrlQuery>

namespace yaap {

TEST_CASE("OpenSubsonic search response maps songs to domain tracks")
{
    QUrlQuery authentication;
    authentication.addQueryItem("u", "alice");
    authentication.addQueryItem("t", "token");
    const auto result = OpenSubsonicClient::parseSearchResponse(
        R"json({"subsonic-response":{"status":"ok","searchResult3":{"song":[{"id":"42","title":"Song","artist":"Artist","album":"Album","duration":12}]}}})json",
        QUrl{"https://music.example"}, authentication);
    REQUIRE(result.succeeded());
    REQUIRE(result.tracks.size() == 1);
    CHECK(result.tracks.front().kind == TrackKind::OpenSubsonic);
    CHECK(result.tracks.front().durationMilliseconds == 12'000);
}

TEST_CASE("Jellyfin item response maps audio items to domain tracks")
{
    const auto result = JellyfinClient::parseItemsResponse(
        R"json({"Items":[{"Id":"7","Name":"Song","Artists":["Artist"],"Album":"Album","RunTimeTicks":30000000}]})json",
        QUrl{"https://jellyfin.example"}, "secret-token");
    REQUIRE(result.succeeded());
    REQUIRE(result.tracks.size() == 1);
    CHECK(result.tracks.front().kind == TrackKind::Jellyfin);
    CHECK(result.tracks.front().durationMilliseconds == 3'000);
}

TEST_CASE("Built-in provider parsers reject oversized response bodies")
{
    const QByteArray oversized(4 * 1024 * 1024 + 1, 'x');
    QUrlQuery authentication;
    CHECK_FALSE(OpenSubsonicClient::parseSearchResponse(
        oversized, QUrl{"https://music.example"}, authentication).succeeded());
    CHECK_FALSE(JellyfinClient::parseItemsResponse(
        oversized, QUrl{"https://jellyfin.example"}, "token").succeeded());
}

} // namespace yaap
