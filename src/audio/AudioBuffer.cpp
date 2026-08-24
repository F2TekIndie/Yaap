#include "audio/AudioBuffer.hpp"

#include <algorithm>

namespace yaap {

void AudioBuffer::setSamples(std::vector<float> interleavedSamples)
{
    m_playing.store(false, std::memory_order_release);
    m_cursor.store(0, std::memory_order_release);
    m_finished.store(false, std::memory_order_release);
    m_samples = std::move(interleavedSamples);
}

void AudioBuffer::clear() noexcept
{
    m_playing.store(false, std::memory_order_release);
    m_cursor.store(0, std::memory_order_release);
    m_finished.store(false, std::memory_order_release);
    m_samples.clear();
}

void AudioBuffer::play() noexcept
{
    if (m_samples.empty()) {
        return;
    }

    if (m_cursor.load(std::memory_order_acquire) >= m_samples.size()) {
        m_cursor.store(0, std::memory_order_release);
    }

    m_finished.store(false, std::memory_order_release);
    m_playing.store(true, std::memory_order_release);
}

void AudioBuffer::pause() noexcept
{
    m_playing.store(false, std::memory_order_release);
}

void AudioBuffer::stop() noexcept
{
    m_playing.store(false, std::memory_order_release);
    m_cursor.store(0, std::memory_order_release);
    m_finished.store(false, std::memory_order_release);
}

std::size_t AudioBuffer::render(std::span<float> output, const std::size_t frameCount) noexcept
{
    const auto requestedSamples = frameCount * channels;
    const auto outputSamples = std::min(requestedSamples, output.size());
    std::fill_n(output.begin(), outputSamples, 0.0F);

    if (!m_playing.load(std::memory_order_acquire) || m_samples.empty()) {
        return 0;
    }

    const auto cursor = m_cursor.load(std::memory_order_relaxed);
    const auto available = m_samples.size() - std::min(cursor, m_samples.size());
    const auto copied = std::min(outputSamples, available);

    std::copy_n(m_samples.begin() + static_cast<std::ptrdiff_t>(cursor), copied, output.begin());
    const auto nextCursor = cursor + copied;
    m_cursor.store(nextCursor, std::memory_order_release);

    if (nextCursor >= m_samples.size()) {
        m_playing.store(false, std::memory_order_release);
        m_finished.store(true, std::memory_order_release);
    }

    return copied / channels;
}

bool AudioBuffer::hasAudio() const noexcept
{
    return !m_samples.empty();
}

bool AudioBuffer::isPlaying() const noexcept
{
    return m_playing.load(std::memory_order_acquire);
}

bool AudioBuffer::isFinished() const noexcept
{
    return m_finished.load(std::memory_order_acquire);
}

std::int64_t AudioBuffer::positionMilliseconds() const noexcept
{
    const auto frames = m_cursor.load(std::memory_order_acquire) / channels;
    return static_cast<std::int64_t>((frames * 1'000ULL) / sampleRate);
}

std::int64_t AudioBuffer::durationMilliseconds() const noexcept
{
    const auto frames = m_samples.size() / channels;
    return static_cast<std::int64_t>((frames * 1'000ULL) / sampleRate);
}

} // namespace yaap

