#pragma once

#include "domain/Track.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace yaap {

enum class RepeatMode {
    Off,
    One,
    All,
};

class PlaybackQueue final {
public:
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::span<const Track> tracks() const noexcept;
    [[nodiscard]] std::optional<std::size_t> currentIndex() const noexcept;
    [[nodiscard]] const Track* current() const noexcept;
    [[nodiscard]] RepeatMode repeatMode() const noexcept;

    std::size_t enqueue(Track track);
    void enqueue(std::vector<Track> tracks);
    bool remove(std::size_t index);
    bool select(std::size_t index) noexcept;
    [[nodiscard]] const Track* next() noexcept;
    [[nodiscard]] const Track* previous() noexcept;
    void clear() noexcept;
    void setRepeatMode(RepeatMode mode) noexcept;

private:
    std::vector<Track> m_tracks;
    std::optional<std::size_t> m_currentIndex;
    RepeatMode m_repeatMode{RepeatMode::Off};
};

} // namespace yaap
