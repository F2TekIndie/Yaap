#include "audio/MiniaudioOutput.hpp"
#include "audio/PcmFormat.hpp"
#include "audio/PcmStream.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

TEST_CASE("Miniaudio can open the default playback device", "[.audio-device]")
{
    auto stream = std::make_shared<yaap::PcmStream>();
    std::vector<float> samples(
        yaap::PcmFormat::sampleRate * yaap::PcmFormat::channels / 4U,
        0.0F);
    REQUIRE(stream->write(samples) == yaap::PcmFormat::sampleRate / 4U);
    stream->markEndOfStream();

    yaap::MiniaudioOutput output;
    std::string error;
    INFO(error);
    REQUIRE(output.attach(std::move(stream), error));
    REQUIRE(output.play(error));

    std::this_thread::sleep_for(std::chrono::milliseconds{75});
    REQUIRE(output.positionMilliseconds() > 0);
    output.clear();
}
