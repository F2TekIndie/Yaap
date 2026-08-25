#pragma once

#include "audio/PcmFormat.hpp"

#include <atomic>
#include <cstddef>
#include <span>
#include <vector>

namespace yaap {

class SpscPcmRingBuffer final {
public:
    explicit SpscPcmRingBuffer(std::size_t capacityFrames);

    SpscPcmRingBuffer(const SpscPcmRingBuffer&) = delete;
    SpscPcmRingBuffer& operator=(const SpscPcmRingBuffer&) = delete;
    SpscPcmRingBuffer(SpscPcmRingBuffer&&) = delete;
    SpscPcmRingBuffer& operator=(SpscPcmRingBuffer&&) = delete;

    // Exactly one producer may call write(), and exactly one consumer may call
    // read(). Neither operation allocates, locks, or blocks.
    [[nodiscard]] std::size_t write(std::span<const float> interleavedSamples) noexcept;
    [[nodiscard]] std::size_t read(
        std::span<float> output,
        std::size_t requestedFrames) noexcept;

    // reset() requires both producer and consumer to be stopped.
    void reset() noexcept;

    [[nodiscard]] std::size_t capacityFrames() const noexcept;
    [[nodiscard]] std::size_t bufferedFrames() const noexcept;
    [[nodiscard]] std::size_t writableFrames() const noexcept;
    [[nodiscard]] std::size_t totalFramesRead() const noexcept;

private:
    std::vector<float> m_samples;
    const std::size_t m_capacityFrames;
    std::atomic<std::size_t> m_readFrame{0};
    std::atomic<std::size_t> m_writeFrame{0};
    std::atomic<std::size_t> m_totalFramesRead{0};
};

} // namespace yaap
