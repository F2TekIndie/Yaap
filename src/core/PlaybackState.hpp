#pragma once

#include <string_view>

namespace yaap {

enum class PlaybackState {
    Empty,
    Loading,
    Ready,
    Playing,
    Buffering,
    Paused,
    Stopped,
    Finished,
    Error,
};

[[nodiscard]] constexpr std::string_view toString(PlaybackState state) noexcept
{
    using enum PlaybackState;

    switch (state) {
    case Empty:
        return "Empty";
    case Loading:
        return "Loading";
    case Ready:
        return "Ready";
    case Playing:
        return "Playing";
    case Buffering:
        return "Buffering";
    case Paused:
        return "Paused";
    case Stopped:
        return "Stopped";
    case Finished:
        return "Finished";
    case Error:
        return "Error";
    }

    return "Unknown";
}

class PlaybackStateMachine final {
public:
    [[nodiscard]] PlaybackState state() const noexcept;
    [[nodiscard]] bool canTransitionTo(PlaybackState next) const noexcept;
    bool transitionTo(PlaybackState next) noexcept;
    void reset() noexcept;

private:
    PlaybackState m_state{PlaybackState::Empty};
};

} // namespace yaap
