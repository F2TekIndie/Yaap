#include "radio/RadioPlaylist.hpp"

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

} // namespace yaap
