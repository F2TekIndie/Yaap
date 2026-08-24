#include "core/PlaybackState.hpp"

namespace yaap {

PlaybackState PlaybackStateMachine::state() const noexcept
{
    return m_state;
}

bool PlaybackStateMachine::canTransitionTo(const PlaybackState next) const noexcept
{
    using enum PlaybackState;

    if (next == m_state) {
        return true;
    }

    switch (m_state) {
    case Empty:
        return next == Loading || next == Error;
    case Loading:
        return next == Ready || next == Error || next == Empty;
    case Ready:
        return next == Playing || next == Stopped || next == Loading || next == Empty || next == Error;
    case Playing:
        return next == Paused || next == Stopped || next == Finished || next == Loading || next == Error;
    case Paused:
        return next == Playing || next == Stopped || next == Loading || next == Error;
    case Stopped:
        return next == Playing || next == Loading || next == Empty || next == Error;
    case Finished:
        return next == Playing || next == Stopped || next == Loading || next == Empty;
    case Error:
        return next == Loading || next == Empty;
    }

    return false;
}

bool PlaybackStateMachine::transitionTo(const PlaybackState next) noexcept
{
    if (!canTransitionTo(next)) {
        return false;
    }

    m_state = next;
    return true;
}

void PlaybackStateMachine::reset() noexcept
{
    m_state = PlaybackState::Empty;
}

} // namespace yaap
