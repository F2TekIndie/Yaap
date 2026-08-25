#include "audio/SpscPcmRingBuffer.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace yaap {
namespace {

static_assert(std::atomic<std::size_t>::is_always_lock_free);

[[nodiscard]] std::size_t checkedSampleCapacity(const std::size_t capacityFrames)
{
    if (capacityFrames == 0
        || capacityFrames > std::numeric_limits<std::size_t>::max() / PcmFormat::channels) {
        throw std::invalid_argument("PCM ring-buffer capacity must be a positive frame count");
    }
    return capacityFrames * PcmFormat::channels;
}

} // namespace

SpscPcmRingBuffer::SpscPcmRingBuffer(const std::size_t capacityFrames)
    : m_samples(checkedSampleCapacity(capacityFrames))
    , m_capacityFrames(capacityFrames)
{
}

std::size_t SpscPcmRingBuffer::write(const std::span<const float> interleavedSamples) noexcept
{
    const auto offeredFrames = interleavedSamples.size() / PcmFormat::channels;
    if (offeredFrames == 0) {
        return 0;
    }

    const auto writeFrame = m_writeFrame.load(std::memory_order_relaxed);
    const auto readFrame = m_readFrame.load(std::memory_order_acquire);
    const auto usedFrames = writeFrame - readFrame;
    if (usedFrames >= m_capacityFrames) {
        return 0;
    }

    const auto framesToWrite = std::min(offeredFrames, m_capacityFrames - usedFrames);
    const auto ringOffsetFrames = writeFrame % m_capacityFrames;
    const auto firstFrames = std::min(framesToWrite, m_capacityFrames - ringOffsetFrames);
    const auto firstSamples = firstFrames * PcmFormat::channels;
    const auto ringOffsetSamples = ringOffsetFrames * PcmFormat::channels;

    std::copy_n(
        interleavedSamples.begin(),
        firstSamples,
        m_samples.begin() + static_cast<std::ptrdiff_t>(ringOffsetSamples));

    const auto remainingFrames = framesToWrite - firstFrames;
    if (remainingFrames > 0) {
        const auto remainingSamples = remainingFrames * PcmFormat::channels;
        std::copy_n(
            interleavedSamples.begin() + static_cast<std::ptrdiff_t>(firstSamples),
            remainingSamples,
            m_samples.begin());
    }

    m_writeFrame.store(writeFrame + framesToWrite, std::memory_order_release);
    return framesToWrite;
}

std::size_t SpscPcmRingBuffer::read(
    const std::span<float> output,
    const std::size_t requestedFrames) noexcept
{
    const auto outputFrames = std::min(requestedFrames, output.size() / PcmFormat::channels);
    const auto outputSamples = outputFrames * PcmFormat::channels;
    std::fill_n(output.begin(), outputSamples, 0.0F);
    if (outputFrames == 0) {
        return 0;
    }

    const auto readFrame = m_readFrame.load(std::memory_order_relaxed);
    const auto writeFrame = m_writeFrame.load(std::memory_order_acquire);
    const auto availableFrames = writeFrame - readFrame;
    const auto framesToRead = std::min(outputFrames, availableFrames);
    if (framesToRead == 0) {
        return 0;
    }

    const auto ringOffsetFrames = readFrame % m_capacityFrames;
    const auto firstFrames = std::min(framesToRead, m_capacityFrames - ringOffsetFrames);
    const auto firstSamples = firstFrames * PcmFormat::channels;
    const auto ringOffsetSamples = ringOffsetFrames * PcmFormat::channels;

    std::copy_n(
        m_samples.begin() + static_cast<std::ptrdiff_t>(ringOffsetSamples),
        firstSamples,
        output.begin());

    const auto remainingFrames = framesToRead - firstFrames;
    if (remainingFrames > 0) {
        const auto remainingSamples = remainingFrames * PcmFormat::channels;
        std::copy_n(
            m_samples.begin(),
            remainingSamples,
            output.begin() + static_cast<std::ptrdiff_t>(firstSamples));
    }

    m_readFrame.store(readFrame + framesToRead, std::memory_order_release);
    m_totalFramesRead.fetch_add(framesToRead, std::memory_order_relaxed);
    return framesToRead;
}

void SpscPcmRingBuffer::reset() noexcept
{
    m_readFrame.store(0, std::memory_order_relaxed);
    m_writeFrame.store(0, std::memory_order_relaxed);
    m_totalFramesRead.store(0, std::memory_order_relaxed);
}

std::size_t SpscPcmRingBuffer::capacityFrames() const noexcept
{
    return m_capacityFrames;
}

std::size_t SpscPcmRingBuffer::bufferedFrames() const noexcept
{
    const auto writeFrame = m_writeFrame.load(std::memory_order_acquire);
    const auto readFrame = m_readFrame.load(std::memory_order_acquire);
    return std::min(writeFrame - readFrame, m_capacityFrames);
}

std::size_t SpscPcmRingBuffer::writableFrames() const noexcept
{
    return m_capacityFrames - bufferedFrames();
}

std::size_t SpscPcmRingBuffer::totalFramesRead() const noexcept
{
    return m_totalFramesRead.load(std::memory_order_acquire);
}

} // namespace yaap
