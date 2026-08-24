#include "audio/AudioBuffer.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <vector>

TEST_CASE("Audio buffer renders interleaved samples and pads with silence")
{
    yaap::AudioBuffer buffer;
    buffer.setSamples({0.25F, -0.25F, 0.5F, -0.5F});
    buffer.play();

    std::array<float, 8> output{};
    const auto renderedFrames = buffer.render(output, 4);

    REQUIRE(renderedFrames == 2);
    REQUIRE(output[0] == Catch::Approx(0.25F));
    REQUIRE(output[1] == Catch::Approx(-0.25F));
    REQUIRE(output[2] == Catch::Approx(0.5F));
    REQUIRE(output[3] == Catch::Approx(-0.5F));
    REQUIRE(output[4] == Catch::Approx(0.0F));
    REQUIRE(output[7] == Catch::Approx(0.0F));
    REQUIRE(buffer.isFinished());
}

TEST_CASE("Paused audio does not advance")
{
    yaap::AudioBuffer buffer;
    buffer.setSamples(std::vector<float>(960, 0.5F));
    buffer.play();

    std::array<float, 20> output{};
    REQUIRE(buffer.render(output, 10) == 10);
    const auto positionBeforePause = buffer.positionMilliseconds();
    buffer.pause();

    REQUIRE(buffer.render(output, 10) == 0);
    REQUIRE(buffer.positionMilliseconds() == positionBeforePause);
}

TEST_CASE("Stop resets playback to the beginning")
{
    yaap::AudioBuffer buffer;
    buffer.setSamples(std::vector<float>(9'600, 0.5F));
    buffer.play();

    std::array<float, 960> output{};
    REQUIRE(buffer.render(output, 480) == 480);
    REQUIRE(buffer.positionMilliseconds() == 10);

    buffer.stop();
    REQUIRE(buffer.positionMilliseconds() == 0);
    REQUIRE_FALSE(buffer.isPlaying());
    REQUIRE_FALSE(buffer.isFinished());
}

