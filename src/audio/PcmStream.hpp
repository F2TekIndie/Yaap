#pragma once

#include "audio/PcmFormat.hpp"
#include "audio/SpscPcmRingBuffer.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stop_token>

namespace yaap {

class PcmStream final {
public:
    static constexpr std::size_t defaultCapacityFrames = PcmFormat::sampleRate * 4U;

    explicit PcmStream(std::size_t capacityFrames = defaultCapacityFrames);

    PcmStream(const PcmStream&) = delete;
    PcmStream& operator=(const PcmStream&) = delete;
    PcmStream(PcmStream&&) = delete;
    PcmStream& operator=(PcmStream&&) = delete;

    [[nodiscard]] std::size_t write(std::span<const float> interleavedSamples) noexcept;
    [[nodiscard]] bool waitForWritableFrames(std::stop_token stopToken) noexcept;
    void interruptProducerWait() noexcept;
    [[nodiscard]] std::size_t render(
        std::span<float> output,
        std::size_t requestedFrames) noexcept;

    void play() noexcept;
    void pause() noexcept;
    void markEndOfStream() noexcept;
    void setStartPositionMilliseconds(std::int64_t position) noexcept;
    void setDurationMilliseconds(std::int64_t duration) noexcept;

    [[nodiscard]] bool hasAudio() const noexcept;
    [[nodiscard]] bool isPlaying() const noexcept;
    [[nodiscard]] bool isEndOfStream() const noexcept;
    [[nodiscard]] bool isFinished() const noexcept;
    [[nodiscard]] std::size_t capacityFrames() const noexcept;
    [[nodiscard]] std::size_t bufferedFrames() const noexcept;
    [[nodiscard]] std::size_t producedFrameCount() const noexcept;
    [[nodiscard]] std::size_t underrunCount() const noexcept;
    [[nodiscard]] std::int64_t bufferedMilliseconds() const noexcept;
    [[nodiscard]] std::int64_t positionMilliseconds() const noexcept;
    [[nodiscard]] std::int64_t durationMilliseconds() const noexcept;

private:
    SpscPcmRingBuffer m_ringBuffer;
    std::atomic<std::size_t> m_producedFrames{0};
    std::atomic<std::size_t> m_underrunCount{0};
    std::atomic<std::int64_t> m_startPositionMilliseconds{0};
    std::atomic<std::int64_t> m_durationMilliseconds{0};
    std::atomic<bool> m_hasAudio{false};
    std::atomic<bool> m_playing{false};
    std::atomic<bool> m_endOfStream{false};
    std::atomic<std::uint64_t> m_spaceGeneration{0};
};

} // namespace yaap
