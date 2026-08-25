#include "domain/PlaybackQueue.hpp"

#include <utility>

namespace yaap {

std::size_t PlaybackQueue::size() const noexcept
{
    return m_tracks.size();
}

bool PlaybackQueue::empty() const noexcept
{
    return m_tracks.empty();
}

std::span<const Track> PlaybackQueue::tracks() const noexcept
{
    return m_tracks;
}

std::optional<std::size_t> PlaybackQueue::currentIndex() const noexcept
{
    return m_currentIndex;
}

const Track* PlaybackQueue::current() const noexcept
{
    return m_currentIndex ? &m_tracks[*m_currentIndex] : nullptr;
}

RepeatMode PlaybackQueue::repeatMode() const noexcept
{
    return m_repeatMode;
}

std::size_t PlaybackQueue::enqueue(Track track)
{
    m_tracks.push_back(std::move(track));
    if (!m_currentIndex) {
        m_currentIndex = 0;
    }
    return m_tracks.size() - 1;
}

void PlaybackQueue::enqueue(std::vector<Track> tracks)
{
    for (auto& track : tracks) {
        enqueue(std::move(track));
    }
}

bool PlaybackQueue::remove(const std::size_t index)
{
    if (index >= m_tracks.size()) {
        return false;
    }

    m_tracks.erase(m_tracks.begin() + static_cast<std::ptrdiff_t>(index));
    if (m_tracks.empty()) {
        m_currentIndex.reset();
    } else if (m_currentIndex) {
        if (*m_currentIndex > index) {
            --*m_currentIndex;
        } else if (*m_currentIndex == index && *m_currentIndex >= m_tracks.size()) {
            m_currentIndex = m_tracks.size() - 1;
        }
    }
    return true;
}

bool PlaybackQueue::select(const std::size_t index) noexcept
{
    if (index >= m_tracks.size()) {
        return false;
    }
    m_currentIndex = index;
    return true;
}

const Track* PlaybackQueue::next() noexcept
{
    if (!m_currentIndex || m_tracks.empty()) {
        return nullptr;
    }
    if (m_repeatMode == RepeatMode::One) {
        return current();
    }
    if (*m_currentIndex + 1 < m_tracks.size()) {
        ++*m_currentIndex;
        return current();
    }
    if (m_repeatMode == RepeatMode::All) {
        m_currentIndex = 0;
        return current();
    }
    return nullptr;
}

const Track* PlaybackQueue::previous() noexcept
{
    if (!m_currentIndex || m_tracks.empty()) {
        return nullptr;
    }
    if (m_repeatMode == RepeatMode::One) {
        return current();
    }
    if (*m_currentIndex > 0) {
        --*m_currentIndex;
        return current();
    }
    if (m_repeatMode == RepeatMode::All) {
        m_currentIndex = m_tracks.size() - 1;
        return current();
    }
    return nullptr;
}

void PlaybackQueue::clear() noexcept
{
    m_tracks.clear();
    m_currentIndex.reset();
}

void PlaybackQueue::setRepeatMode(const RepeatMode mode) noexcept
{
    m_repeatMode = mode;
}

} // namespace yaap
