#include "radio/RadioPlaylist.hpp"
#include "radio/RadioBrowserClient.hpp"

#include <catch2/catch_test_macros.hpp>

namespace yaap {

TEST_CASE("Radio playlists parse M3U PLS and XSPF")
{
    const auto m3u = RadioPlaylistParser::parse(
        "#EXTM3U\n#EXTINF:-1,Example Radio\nhttps://radio.example/live\n");
    REQUIRE(m3u.succeeded());
    REQUIRE(m3u.stations.size() == 1);
    CHECK(m3u.stations.front().name == "Example Radio");

    const auto pls = RadioPlaylistParser::parse(
        "[playlist]\nFile1=https://radio.example/a\nTitle1=Station A\n");
    REQUIRE(pls.stations.size() == 1);
    CHECK(pls.stations.front().name == "Station A");

    const auto xspf = RadioPlaylistParser::parse(
        "<?xml version=\"1.0\"?><playlist><trackList><track>"
        "<title>Station X</title><location>https://radio.example/x</location>"
        "</track></trackList></playlist>");
    REQUIRE(xspf.succeeded());
    REQUIRE(xspf.stations.size() == 1);
}

TEST_CASE("ICY demuxer handles metadata split across chunks")
{
    IcyMetadataDemuxer demuxer{4};
    QByteArray firstChunk{"abcd"};
    firstChunk.append(char{2});
    firstChunk.append("StreamTitle='Tes");
    auto first = demuxer.consume(firstChunk);
    CHECK(first.audio == "abcd");
    CHECK_FALSE(first.streamTitle.has_value());
    auto second = demuxer.consume(QByteArray{"t';"}.leftJustified(16, '\0'));
    REQUIRE(second.streamTitle.has_value());
    CHECK(*second.streamTitle == "Test");
}

TEST_CASE("Reconnect policy applies capped exponential backoff")
{
    ReconnectPolicy policy{.initialDelay = std::chrono::milliseconds{500},
        .maximumDelay = std::chrono::milliseconds{2'000}, .multiplier = 2.0};
    CHECK(policy.delayForAttempt(0) == std::chrono::milliseconds{500});
    CHECK(policy.delayForAttempt(2) == std::chrono::milliseconds{2'000});
    CHECK(policy.delayForAttempt(10) == std::chrono::milliseconds{2'000});
}

TEST_CASE("Radio Browser responses map bounded playable station metadata")
{
    const auto result = RadioBrowserParser::parseStations(R"json([
      {
        "stationuuid":"9617a958-0601-11e8-ae97-52543be04c81",
        "name":" Example Radio ",
        "url":"http://radio.example/playlist.m3u",
        "url_resolved":"https://radio.example/live",
        "homepage":"https://radio.example",
        "favicon":"javascript:alert(1)",
        "countrycode":"de",
        "language":"German",
        "tags":"jazz,public radio",
        "codec":"MP3",
        "bitrate":192,
        "hls":0
      },
      {
        "stationuuid":"not a uuid",
        "name":"Rejected",
        "url_resolved":"file:///private/audio"
      }
    ])json");

    REQUIRE(result.succeeded());
    REQUIRE(result.stations.size() == 1);
    const auto& station = result.stations.front();
    CHECK(station.name == "Example Radio");
    CHECK(station.streamUrl == QUrl{"https://radio.example/live"});
    CHECK(station.faviconUrl.isEmpty());
    CHECK(station.countryCode == "DE");
    CHECK(station.codec == "MP3");
    CHECK(station.bitrate == 192);
    CHECK_FALSE(station.hls);
}

TEST_CASE("Radio Browser parser rejects malformed and oversized responses")
{
    CHECK_FALSE(RadioBrowserParser::parseStations("not-json").succeeded());
    CHECK_FALSE(RadioBrowserParser::parseStations(
        QByteArray{2 * 1024 * 1024 + 1, 'x'}).succeeded());
}

} // namespace yaap
