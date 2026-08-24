#pragma once

#include "audio/DecodedAudio.hpp"

#include <filesystem>
#include <optional>
#include <stop_token>
#include <string>

namespace yaap {

struct DecodeResult final {
    std::optional<DecodedAudio> audio;
    std::string error;
    bool cancelled{};

    [[nodiscard]] bool succeeded() const noexcept { return audio.has_value(); }
};

class FFmpegDecoder final {
public:
    [[nodiscard]] DecodeResult decode(
        const std::filesystem::path& path,
        std::stop_token stopToken = {}) const;
};

} // namespace yaap

