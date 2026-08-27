#include "core/PlaybackState.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Playback state follows the local-file happy path")
{
    yaap::PlaybackStateMachine state;

    REQUIRE(state.state() == yaap::PlaybackState::Empty);
    REQUIRE(state.transitionTo(yaap::PlaybackState::Loading));
    REQUIRE(state.transitionTo(yaap::PlaybackState::Ready));
    REQUIRE(state.transitionTo(yaap::PlaybackState::Playing));
    REQUIRE(state.transitionTo(yaap::PlaybackState::Buffering));
    REQUIRE(state.transitionTo(yaap::PlaybackState::Playing));
    REQUIRE(state.transitionTo(yaap::PlaybackState::Paused));
    REQUIRE(state.transitionTo(yaap::PlaybackState::Playing));
    REQUIRE(state.transitionTo(yaap::PlaybackState::Finished));
}

TEST_CASE("Playback state rejects nonsensical transitions")
{
    yaap::PlaybackStateMachine state;

    REQUIRE_FALSE(state.transitionTo(yaap::PlaybackState::Playing));
    REQUIRE(state.state() == yaap::PlaybackState::Empty);

    REQUIRE(state.transitionTo(yaap::PlaybackState::Loading));
    REQUIRE_FALSE(state.transitionTo(yaap::PlaybackState::Playing));
    REQUIRE(state.state() == yaap::PlaybackState::Loading);
}

TEST_CASE("Playback state can recover after an error")
{
    yaap::PlaybackStateMachine state;

    REQUIRE(state.transitionTo(yaap::PlaybackState::Error));
    REQUIRE(state.transitionTo(yaap::PlaybackState::Loading));
    REQUIRE(state.transitionTo(yaap::PlaybackState::Ready));
}

TEST_CASE("A network source may start buffering while it opens")
{
    yaap::PlaybackStateMachine state;
    REQUIRE(state.transitionTo(yaap::PlaybackState::Loading));
    CHECK(state.transitionTo(yaap::PlaybackState::Buffering));
    CHECK(state.transitionTo(yaap::PlaybackState::Playing));
}

TEST_CASE("Loading and error playback can be explicitly stopped")
{
    yaap::PlaybackStateMachine loading;
    REQUIRE(loading.transitionTo(yaap::PlaybackState::Loading));
    CHECK(loading.transitionTo(yaap::PlaybackState::Stopped));

    yaap::PlaybackStateMachine failed;
    REQUIRE(failed.transitionTo(yaap::PlaybackState::Error));
    CHECK(failed.transitionTo(yaap::PlaybackState::Stopped));
}
