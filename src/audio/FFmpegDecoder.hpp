#pragma once

#include "audio/PcmStream.hpp"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <stop_token>
#include <string>

namespace yaap {

struct AudioStreamInfo final {
    std::string title;
    std::int64_t durationMilliseconds{};
};

struct StreamDecodeResult final {
    AudioStreamInfo info;
    std::size_t decodedFrameCount{};
    std::string error;
    bool cancelled{};

    [[nodiscard]] bool succeeded() const noexcept
    {
        return !cancelled && error.empty() && decodedFrameCount > 0;
    }
};

struct StreamOptions final {
    std::int64_t startPositionMilliseconds{};
    std::chrono::milliseconds ioTimeout{15'000};
    std::chrono::milliseconds prebufferDuration{250};
    bool reconnectNetworkStream{};
    std::string userAgent{"Yaap/0.1"};
    std::string httpHeaders;
};

class FFmpegDecoder final {
public:
    using ReadyCallback = std::function<void(const AudioStreamInfo&)>;

    [[nodiscard]] StreamDecodeResult streamFile(
        const std::filesystem::path& path,
        PcmStream& destination,
        ReadyCallback readyCallback = {},
        StreamOptions options = {},
        std::stop_token stopToken = {}) const;

    [[nodiscard]] StreamDecodeResult streamUrl(
        const std::string& url,
        PcmStream& destination,
        ReadyCallback readyCallback = {},
        StreamOptions options = {},
        std::stop_token stopToken = {}) const;

private:
    [[nodiscard]] StreamDecodeResult streamInput(
        const std::string& input,
        PcmStream& destination,
        ReadyCallback readyCallback,
        StreamOptions options,
        std::stop_token stopToken) const;
};

} // namespace yaap
