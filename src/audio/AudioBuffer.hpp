#pragma once

#include "audio/DecodedAudio.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace yaap {

class AudioBuffer final {
public:
    static constexpr std::size_t channels = DecodedAudio::outputChannels;
    static constexpr std::uint32_t sampleRate = DecodedAudio::outputSampleRate;

    // These mutation functions may only be called while the audio device is stopped.
    // That invariant keeps render() lock-free for the real-time audio callback.
    void setSamples(std::vector<float> interleavedSamples);
    void clear() noexcept;

    void play() noexcept;
    void pause() noexcept;
    void stop() noexcept;

    [[nodiscard]] std::size_t render(std::span<float> output, std::size_t frameCount) noexcept;
    [[nodiscard]] bool hasAudio() const noexcept;
    [[nodiscard]] bool isPlaying() const noexcept;
    [[nodiscard]] bool isFinished() const noexcept;
    [[nodiscard]] std::int64_t positionMilliseconds() const noexcept;
    [[nodiscard]] std::int64_t durationMilliseconds() const noexcept;

private:
    std::vector<float> m_samples;
    std::atomic<std::size_t> m_cursor{0};
    std::atomic<bool> m_playing{false};
    std::atomic<bool> m_finished{false};
};

} // namespace yaap

