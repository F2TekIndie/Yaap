#include "audio/PcmStream.hpp"
#include "audio/SpscPcmRingBuffer.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <span>
#include <stop_token>
#include <thread>
#include <vector>

TEST_CASE("SPSC PCM ring buffer is bounded and preserves wrapped frame order")
{
    yaap::SpscPcmRingBuffer buffer{3};
    const std::array input{
        1.0F, -1.0F,
        2.0F, -2.0F,
        3.0F, -3.0F,
        4.0F, -4.0F};

    REQUIRE(buffer.write(input) == 3);
    REQUIRE(buffer.bufferedFrames() == 3);
    REQUIRE(buffer.writableFrames() == 0);

    std::array<float, 4> firstOutput{};
    REQUIRE(buffer.read(firstOutput, 2) == 2);
    REQUIRE(firstOutput == std::array{1.0F, -1.0F, 2.0F, -2.0F});

    REQUIRE(buffer.write(std::span{input}.subspan(6)) == 1);
    std::array<float, 6> wrappedOutput{};
    REQUIRE(buffer.read(wrappedOutput, 3) == 2);
    REQUIRE(wrappedOutput[0] == Catch::Approx(3.0F));
    REQUIRE(wrappedOutput[1] == Catch::Approx(-3.0F));
    REQUIRE(wrappedOutput[2] == Catch::Approx(4.0F));
    REQUIRE(wrappedOutput[3] == Catch::Approx(-4.0F));
    REQUIRE(wrappedOutput[4] == Catch::Approx(0.0F));
    REQUIRE(wrappedOutput[5] == Catch::Approx(0.0F));
}

TEST_CASE("PCM stream pads underruns and finishes only after buffered audio drains")
{
    yaap::PcmStream stream{2};
    const std::array samples{0.25F, -0.25F};
    REQUIRE(stream.write(samples) == 1);
    stream.setDurationMilliseconds(1);
    stream.play();

    std::array<float, 4> output{};
    REQUIRE(stream.render(output, 2) == 1);
    REQUIRE(output[0] == Catch::Approx(0.25F));
    REQUIRE(output[1] == Catch::Approx(-0.25F));
    REQUIRE(output[2] == Catch::Approx(0.0F));
    REQUIRE(output[3] == Catch::Approx(0.0F));
    REQUIRE(stream.underrunCount() == 1);
    REQUIRE_FALSE(stream.isFinished());

    stream.markEndOfStream();
    REQUIRE(stream.isFinished());
}

TEST_CASE("PCM stream reports absolute position after a seek restart")
{
    yaap::PcmStream stream{8};
    stream.setStartPositionMilliseconds(2'000);
    const std::array samples{0.0F, 0.0F, 0.0F, 0.0F};
    REQUIRE(stream.write(samples) == 2);
    stream.play();

    std::array<float, 4> output{};
    REQUIRE(stream.render(output, 2) == 2);
    REQUIRE(stream.positionMilliseconds() == 2'000);
}

TEST_CASE("SPSC PCM ring buffer transfers frames concurrently without loss")
{
    constexpr std::size_t frameCount = 10'000;
    yaap::SpscPcmRingBuffer buffer{64};
    std::vector<float> input(frameCount * yaap::PcmFormat::channels);
    for (std::size_t frame = 0; frame < frameCount; ++frame) {
        input[frame * 2] = static_cast<float>(frame);
        input[frame * 2 + 1] = -static_cast<float>(frame);
    }

    std::atomic<bool> producerDone{false};
    std::jthread producer([&](const std::stop_token stopToken) {
        std::size_t producedFrames = 0;
        while (producedFrames < frameCount && !stopToken.stop_requested()) {
            const auto remaining = std::span<const float>{input}.subspan(
                producedFrames * yaap::PcmFormat::channels);
            const auto written = buffer.write(remaining);
            producedFrames += written;
            if (written == 0) {
                std::this_thread::yield();
            }
        }
        producerDone.store(true, std::memory_order_release);
    });

    std::array<float, 34> output{};
    std::size_t consumedFrames = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (consumedFrames < frameCount && std::chrono::steady_clock::now() < deadline) {
        const auto read = buffer.read(output, output.size() / yaap::PcmFormat::channels);
        for (std::size_t index = 0; index < read; ++index) {
            const auto expectedFrame = consumedFrames + index;
            REQUIRE(output[index * 2] == static_cast<float>(expectedFrame));
            REQUIRE(output[index * 2 + 1] == -static_cast<float>(expectedFrame));
        }
        consumedFrames += read;
        if (read == 0) {
            std::this_thread::yield();
        }
    }

    if (consumedFrames != frameCount) {
        producer.request_stop();
    }
    producer.join();
    REQUIRE(consumedFrames == frameCount);
    REQUIRE(producerDone.load(std::memory_order_acquire));
    REQUIRE(buffer.bufferedFrames() == 0);
}
