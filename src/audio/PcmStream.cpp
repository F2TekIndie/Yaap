#include "audio/PcmStream.hpp"

#include <algorithm>

namespace yaap {

PcmStream::PcmStream(const std::size_t capacityFrames)
    : m_ringBuffer(capacityFrames)
{
}

std::size_t PcmStream::write(const std::span<const float> interleavedSamples) noexcept
{
    const auto writtenFrames = m_ringBuffer.write(interleavedSamples);
    if (writtenFrames > 0) {
        m_producedFrames.fetch_add(writtenFrames, std::memory_order_relaxed);
        m_hasAudio.store(true, std::memory_order_release);
    }
    return writtenFrames;
}

std::size_t PcmStream::render(
    const std::span<float> output,
    const std::size_t requestedFrames) noexcept
{
    if (!m_playing.load(std::memory_order_acquire)) {
        const auto samplesToClear = std::min(
            output.size(), requestedFrames * PcmFormat::channels);
        std::fill_n(output.begin(), samplesToClear, 0.0F);
        return 0;
    }
    const auto renderedFrames = m_ringBuffer.read(output, requestedFrames);
    if (renderedFrames < requestedFrames && !isEndOfStream()) {
        m_underrunCount.fetch_add(1, std::memory_order_relaxed);
    }
    return renderedFrames;
}

void PcmStream::play() noexcept
{
    if (hasAudio() && !isFinished()) {
        m_playing.store(true, std::memory_order_release);
    }
}

void PcmStream::pause() noexcept
{
    m_playing.store(false, std::memory_order_release);
}

void PcmStream::markEndOfStream() noexcept
{
    if (m_durationMilliseconds.load(std::memory_order_relaxed) <= 0) {
        const auto frames = m_producedFrames.load(std::memory_order_relaxed);
        const auto duration = static_cast<std::int64_t>(
            (frames * 1'000ULL) / PcmFormat::sampleRate);
        m_durationMilliseconds.store(duration, std::memory_order_release);
    }
    m_endOfStream.store(true, std::memory_order_release);
}

void PcmStream::setStartPositionMilliseconds(const std::int64_t position) noexcept
{
    m_startPositionMilliseconds.store(
        std::max<std::int64_t>(position, 0), std::memory_order_release);
}

void PcmStream::setDurationMilliseconds(const std::int64_t duration) noexcept
{
    m_durationMilliseconds.store(std::max<std::int64_t>(duration, 0), std::memory_order_release);
}

bool PcmStream::hasAudio() const noexcept
{
    return m_hasAudio.load(std::memory_order_acquire);
}

bool PcmStream::isPlaying() const noexcept
{
    return m_playing.load(std::memory_order_acquire);
}

bool PcmStream::isEndOfStream() const noexcept
{
    return m_endOfStream.load(std::memory_order_acquire);
}

bool PcmStream::isFinished() const noexcept
{
    return isEndOfStream() && m_ringBuffer.bufferedFrames() == 0;
}

std::size_t PcmStream::capacityFrames() const noexcept
{
    return m_ringBuffer.capacityFrames();
}

std::size_t PcmStream::bufferedFrames() const noexcept
{
    return m_ringBuffer.bufferedFrames();
}

std::size_t PcmStream::producedFrameCount() const noexcept
{
    return m_producedFrames.load(std::memory_order_acquire);
}

std::size_t PcmStream::underrunCount() const noexcept
{
    return m_underrunCount.load(std::memory_order_acquire);
}

std::int64_t PcmStream::bufferedMilliseconds() const noexcept
{
    return static_cast<std::int64_t>(
        (m_ringBuffer.bufferedFrames() * 1'000ULL) / PcmFormat::sampleRate);
}

std::int64_t PcmStream::positionMilliseconds() const noexcept
{
    const auto frames = m_ringBuffer.totalFramesRead();
    const auto elapsed = static_cast<std::int64_t>(
        (frames * 1'000ULL) / PcmFormat::sampleRate);
    return m_startPositionMilliseconds.load(std::memory_order_acquire) + elapsed;
}

std::int64_t PcmStream::durationMilliseconds() const noexcept
{
    return m_durationMilliseconds.load(std::memory_order_acquire);
}

} // namespace yaap
