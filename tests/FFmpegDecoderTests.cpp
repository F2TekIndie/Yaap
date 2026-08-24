#include "audio/FFmpegDecoder.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stop_token>
#include <string>

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

TEST_CASE("FFmpeg decodes and normalizes a WAV file")
{
    TemporaryFile input{".wav"};
    writeSilentWave(input.path());

    const yaap::FFmpegDecoder decoder;
    const auto result = decoder.decode(input.path());

    INFO(result.error);
    REQUIRE(result.succeeded());
    REQUIRE(result.audio->durationMilliseconds >= 90);
    REQUIRE(result.audio->durationMilliseconds <= 110);
    REQUIRE_FALSE(result.audio->interleavedSamples.empty());
    REQUIRE(result.audio->interleavedSamples.size() % yaap::DecodedAudio::outputChannels == 0);
}

TEST_CASE("FFmpeg reports missing and malformed input")
{
    const yaap::FFmpegDecoder decoder;
    const auto missing = decoder.decode("this-file-does-not-exist.wav");
    REQUIRE_FALSE(missing.succeeded());
    REQUIRE_FALSE(missing.error.empty());

    TemporaryFile malformed{".wav"};
    {
        std::ofstream output(malformed.path(), std::ios::binary);
        output << "not a media file";
    }
    const auto invalid = decoder.decode(malformed.path());
    REQUIRE_FALSE(invalid.succeeded());
    REQUIRE_FALSE(invalid.error.empty());
}

TEST_CASE("FFmpeg decode honours cancellation before opening input")
{
    std::stop_source cancellation;
    cancellation.request_stop();

    const yaap::FFmpegDecoder decoder;
    const auto result = decoder.decode("unused.wav", cancellation.get_token());

    REQUIRE(result.cancelled);
    REQUIRE_FALSE(result.succeeded());
}

