#include "domain/PlaybackQueue.hpp"
#include "domain/Provider.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace {

yaap::Track track(std::string id)
{
    return {
        .id = id,
        .source = "file:///" + id + ".flac",
        .title = std::move(id)};
}

} // namespace

TEST_CASE("Playback queue maintains selection while entries change")
{
    yaap::PlaybackQueue queue;
    queue.enqueue(track("one"));
    queue.enqueue(track("two"));
    queue.enqueue(track("three"));

    REQUIRE(queue.select(1));
    REQUIRE(queue.current()->id == "two");
    REQUIRE(queue.remove(0));
    REQUIRE(queue.currentIndex() == 0);
    REQUIRE(queue.current()->id == "two");
}

TEST_CASE("Playback queue implements repeat-one and repeat-all")
{
    yaap::PlaybackQueue queue;
    queue.enqueue(track("one"));
    queue.enqueue(track("two"));
    REQUIRE(queue.select(1));

    REQUIRE(queue.next() == nullptr);
    queue.setRepeatMode(yaap::RepeatMode::All);
    REQUIRE(queue.next()->id == "one");
    queue.setRepeatMode(yaap::RepeatMode::One);
    REQUIRE(queue.next()->id == "one");
}

TEST_CASE("Provider capabilities are explicit")
{
    const auto capabilities = yaap::ProviderCapability::Browse
        | yaap::ProviderCapability::Stream
        | yaap::ProviderCapability::Artwork;
    REQUIRE(yaap::hasCapability(capabilities, yaap::ProviderCapability::Stream));
    REQUIRE_FALSE(yaap::hasCapability(capabilities, yaap::ProviderCapability::Search));
}
