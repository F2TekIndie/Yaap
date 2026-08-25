#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace yaap {

struct MediaMetadata final {
    std::string title;
    std::string artist;
    std::string album;
    std::int64_t durationMilliseconds{};
    std::string artworkMimeType;
    std::vector<std::uint8_t> artwork;
};

struct MetadataResult final {
    MediaMetadata metadata;
    std::string error;

    [[nodiscard]] bool succeeded() const noexcept { return error.empty(); }
};

class MediaMetadataReader final {
public:
    [[nodiscard]] MetadataResult read(const std::filesystem::path& path) const;
};

} // namespace yaap
