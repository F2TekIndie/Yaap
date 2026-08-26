#pragma once

#include <cstdint>
#include <string>

namespace yaap {

enum class TrackKind {
    LocalFile,
    InternetRadio,
    OpenSubsonic,
    Jellyfin,
    ExternalProvider,
};

struct Track final {
    std::string id;
    TrackKind kind{TrackKind::LocalFile};
    std::string providerId;
    std::string source;
    std::string title;
    std::string artist;
    std::string album;
    std::string artworkSource;
    std::int64_t durationMilliseconds{};

    [[nodiscard]] bool isValid() const noexcept
    {
        return !id.empty() && !source.empty();
    }

    friend bool operator==(const Track&, const Track&) = default;
};

} // namespace yaap
