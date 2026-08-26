#pragma once

#include <cstdint>
#include <string>

namespace yaap {

enum class NowPlayingMetadataSource {
    Icy,
    Container,
    TimedId3,
};

struct NowPlayingMetadata final {
    std::string rawText;
    std::string displayText;
    std::string artist;
    std::string title;
    std::string album;
    std::string streamUrl;
    std::int64_t observedAtEpochMilliseconds{};
    NowPlayingMetadataSource source{NowPlayingMetadataSource::Container};

    [[nodiscard]] bool sameContentAs(const NowPlayingMetadata& other) const noexcept
    {
        return rawText == other.rawText
            && displayText == other.displayText
            && artist == other.artist
            && title == other.title
            && album == other.album
            && streamUrl == other.streamUrl;
    }
};

} // namespace yaap
