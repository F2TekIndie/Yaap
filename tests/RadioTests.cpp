#include "app/RadioController.hpp"
#include "audio/StreamMetadata.hpp"
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

TEST_CASE("Stream metadata parser normalizes ICY and dictionary fields")
{
    const auto icy = StreamMetadataParser::parseIcy(
        "StreamTitle='Example Artist - Example Song';StreamUrl='https://example.test';");
    REQUIRE(icy.has_value());
    CHECK(icy->artist == "Example Artist");
    CHECK(icy->title == "Example Song");
    CHECK(icy->displayText == "Example Artist — Example Song");
    CHECK(icy->streamUrl == "https://example.test");

    const auto semicolonTitle = StreamMetadataParser::parseIcy(
        "StreamTitle='Artist - A Song; Part II';");
    REQUIRE(semicolonTitle.has_value());
    CHECK(semicolonTitle->title == "A Song; Part II");

    const auto fields = StreamMetadataParser::parseFields(
        {{"ARTIST", "Artist"}, {"title", "Title"}, {"album", "Album"}},
        NowPlayingMetadataSource::TimedId3);
    REQUIRE(fields.has_value());
    CHECK(fields->artist == "Artist");
    CHECK(fields->title == "Title");
    CHECK(fields->album == "Album");
    CHECK(fields->source == NowPlayingMetadataSource::TimedId3);

    CHECK_FALSE(StreamMetadataParser::parseIcy("StreamUrl='https://example.test';"));
    CHECK_FALSE(StreamMetadataParser::parseIcy(std::string(20 * 1024, 'x')));
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

TEST_CASE("Saved Radio Browser stations retain identifying details")
{
    QSettings{}.clear();
    RadioBrowserStation station;
    station.stationUuid = "9617a958-0601-11e8-ae97-52543be04c81";
    station.name = "Detailed Radio";
    station.streamUrl = QUrl{"https://radio.example/live"};
    station.homepageUrl = QUrl{"https://radio.example"};
    station.faviconUrl = QUrl{"https://radio.example/icon.png"};
    station.countryCode = "DE";
    station.language = "German";
    station.tags = "jazz,public radio";
    station.codec = "MP3";
    station.bitrate = 192;
    station.hls = true;

    {
        RadioController controller;
        REQUIRE(controller.addDirectoryStation(station));
        REQUIRE(controller.count() == 1);
    }

    RadioController restored;
    REQUIRE(restored.count() == 1);
    const auto row = restored.index(0);
    CHECK(restored.data(row, RadioController::NameRole).toString() == "Detailed Radio");
    CHECK(restored.data(row, RadioController::HomepageUrlRole).toUrl()
        == QUrl{"https://radio.example"});
    CHECK(restored.data(row, RadioController::CountryCodeRole).toString() == "DE");
    CHECK(restored.data(row, RadioController::LanguageRole).toString() == "German");
    CHECK(restored.data(row, RadioController::TagsRole).toString() == "jazz,public radio");
    CHECK(restored.data(row, RadioController::CodecRole).toString() == "MP3");
    CHECK(restored.data(row, RadioController::BitrateRole).toInt() == 192);
    CHECK(restored.data(row, RadioController::HlsRole).toBool());
}

} // namespace yaap
