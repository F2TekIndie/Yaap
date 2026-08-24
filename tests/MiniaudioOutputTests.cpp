#include "audio/MiniaudioOutput.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string>
#include <thread>
#include <vector>

TEST_CASE("Miniaudio can open the default playback device", "[.audio-device]")
{
    yaap::DecodedAudio audio;
    audio.interleavedSamples.resize(
        yaap::DecodedAudio::outputSampleRate * yaap::DecodedAudio::outputChannels / 4U,
        0.0F);

    yaap::MiniaudioOutput output;
    std::string error;
    INFO(error);
    REQUIRE(output.load(std::move(audio), error));
    REQUIRE(output.play(error));

    std::this_thread::sleep_for(std::chrono::milliseconds{75});
    REQUIRE(output.positionMilliseconds() > 0);
    output.stop();
}

