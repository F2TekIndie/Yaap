#pragma once

#include "domain/Track.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace yaap {

enum class SessionStatus {
    Empty,
    Opening,
    Ready,
    Buffering,
    Playing,
    Paused,
    Stopped,
    Finished,
    Error,
};

struct PlaybackSessionSnapshot final {
    std::optional<Track> track;
    SessionStatus status{SessionStatus::Empty};
    std::int64_t positionMilliseconds{};
    std::int64_t durationMilliseconds{};
    std::int64_t bufferedMilliseconds{};
    bool seekable{};
    std::string error;
};

} // namespace yaap
