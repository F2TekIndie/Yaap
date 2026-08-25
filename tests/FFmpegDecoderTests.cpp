#include "audio/FFmpegDecoder.hpp"
#include "audio/PcmFormat.hpp"
#include "audio/PcmStream.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <future>
#include <stop_token>
#include <string>
#include <thread>

namespace {

class TemporaryFile final {
public:
    explicit TemporaryFile(std::string extension)
    {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        m_path = std::filesystem::temp_directory_path()
            / ("yaap-test-" + std::to_string(stamp) + std::move(extension));
    }

    ~TemporaryFile()
    {
        std::error_code ignored;
        std::filesystem::remove(m_path, ignored);
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return m_path; }

private:
    std::filesystem::path m_path;
};

void writeU16(std::ofstream& output, const std::uint16_t value)
{
    const std::array bytes{
        static_cast<char>(value & 0xFFU),
        static_cast<char>((value >> 8U) & 0xFFU)};
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void writeU32(std::ofstream& output, const std::uint32_t value)
{
    const std::array bytes{
        static_cast<char>(value & 0xFFU),
        static_cast<char>((value >> 8U) & 0xFFU),
        static_cast<char>((value >> 16U) & 0xFFU),
        static_cast<char>((value >> 24U) & 0xFFU)};
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void writeSilentWave(const std::filesystem::path& path)
{
    constexpr std::uint32_t inputSampleRate = 8'000;
    constexpr std::uint16_t channelCount = 1;
    constexpr std::uint16_t bitsPerSample = 16;
    constexpr std::uint32_t sampleCount = 800;
    constexpr std::uint32_t dataSize = sampleCount * sizeof(std::int16_t);

    std::ofstream output(path, std::ios::binary);
    output.write("RIFF", 4);
    writeU32(output, 36 + dataSize);
    output.write("WAVEfmt ", 8);
    writeU32(output, 16);
    writeU16(output, 1);
    writeU16(output, channelCount);
    writeU32(output, inputSampleRate);
    writeU32(output, inputSampleRate * channelCount * bitsPerSample / 8);
    writeU16(output, channelCount * bitsPerSample / 8);
    writeU16(output, bitsPerSample);
    output.write("data", 4);
    writeU32(output, dataSize);
    const std::array<char, dataSize> silence{};
    output.write(silence.data(), static_cast<std::streamsize>(silence.size()));
}

} // namespace

TEST_CASE("FFmpeg continuously decodes a WAV file through a bounded PCM stream")
{
    TemporaryFile input{".wav"};
    writeSilentWave(input.path());

    yaap::PcmStream stream{64};
    std::atomic<bool> readyWasPublished{false};
    std::atomic<std::int64_t> publishedDuration{0};
    std::stop_source cancellation;
    const yaap::FFmpegDecoder decoder;
    auto decodeFuture = std::async(std::launch::async, [&] {
        return decoder.streamFile(
            input.path(),
            stream,
            [&](const yaap::AudioStreamInfo& info) {
                publishedDuration.store(info.durationMilliseconds, std::memory_order_release);
                readyWasPublished.store(true, std::memory_order_release);
            },
            {},
            cancellation.get_token());
    });

    std::array<float, 34> output{};
    std::size_t renderedFrames = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (std::chrono::steady_clock::now() < deadline) {
        if (stream.hasAudio()) {
            stream.play();
        }
        renderedFrames += stream.render(output, output.size() / yaap::PcmFormat::channels);

        const auto producerFinished =
            decodeFuture.wait_for(std::chrono::milliseconds{0}) == std::future_status::ready;
        if (producerFinished && stream.bufferedFrames() == 0) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }

    if (decodeFuture.wait_for(std::chrono::milliseconds{0}) != std::future_status::ready) {
        cancellation.request_stop();
    }
    const auto result = decodeFuture.get();

    INFO(result.error);
    REQUIRE(result.succeeded());
    REQUIRE(readyWasPublished.load(std::memory_order_acquire));
    REQUIRE(publishedDuration.load(std::memory_order_acquire) >= 90);
    REQUIRE(publishedDuration.load(std::memory_order_acquire) <= 110);
    REQUIRE(result.info.durationMilliseconds >= 90);
    REQUIRE(result.info.durationMilliseconds <= 110);
    REQUIRE(result.decodedFrameCount == renderedFrames);
    REQUIRE(result.decodedFrameCount > stream.capacityFrames());
    REQUIRE(stream.isEndOfStream());
    REQUIRE(stream.isFinished());
}

TEST_CASE("FFmpeg reports missing and malformed streaming input")
{
    const yaap::FFmpegDecoder decoder;
    yaap::PcmStream missingStream;
    const auto missing = decoder.streamFile("this-file-does-not-exist.wav", missingStream);
    REQUIRE_FALSE(missing.succeeded());
    REQUIRE_FALSE(missing.error.empty());

    TemporaryFile malformed{".wav"};
    {
        std::ofstream output(malformed.path(), std::ios::binary);
        output << "not a media file";
    }
    yaap::PcmStream malformedStream;
    const auto invalid = decoder.streamFile(malformed.path(), malformedStream);
    REQUIRE_FALSE(invalid.succeeded());
    REQUIRE_FALSE(invalid.error.empty());
}

TEST_CASE("FFmpeg streaming honours cancellation before opening input")
{
    std::stop_source cancellation;
    cancellation.request_stop();

    const yaap::FFmpegDecoder decoder;
    yaap::PcmStream stream;
    const auto result = decoder.streamFile(
        "unused.wav", stream, {}, {}, cancellation.get_token());

    REQUIRE(result.cancelled);
    REQUIRE_FALSE(result.succeeded());
}

TEST_CASE("FFmpeg streaming seeks before producing PCM")
{
    TemporaryFile input{".wav"};
    writeSilentWave(input.path());

    yaap::PcmStream stream;
    yaap::StreamOptions options;
    options.startPositionMilliseconds = 50;
    const yaap::FFmpegDecoder decoder;
    const auto result = decoder.streamFile(input.path(), stream, {}, options);

    INFO(result.error);
    REQUIRE(result.succeeded());
    REQUIRE(stream.positionMilliseconds() == 50);
    REQUIRE(stream.durationMilliseconds() >= 90);
    REQUIRE(stream.durationMilliseconds() <= 110);
    REQUIRE(result.decodedFrameCount < yaap::PcmFormat::sampleRate / 10U);
}
