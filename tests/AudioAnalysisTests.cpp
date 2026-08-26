#include "app/AudioVisualizationModel.hpp"
#include "audio/AudioAnalysisEngine.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <numbers>
#include <thread>
#include <vector>

namespace yaap {
namespace {

[[nodiscard]] std::array<float, audioSpectrumFftSize> sineWave(
    const float frequencyHz, const float amplitude)
{
    std::array<float, audioSpectrumFftSize> samples{};
    for (std::size_t index = 0; index < samples.size(); ++index) {
        samples[index] = amplitude * std::sin(
            2.0F * std::numbers::pi_v<float> * frequencyHz
            * static_cast<float>(index) / static_cast<float>(PcmFormat::sampleRate));
    }
    return samples;
}

[[nodiscard]] std::vector<float> interleavedSine(
    const std::size_t frameCount, const float frequencyHz)
{
    std::vector<float> samples(frameCount * PcmFormat::channels);
    for (std::size_t frame = 0; frame < frameCount; ++frame) {
        const auto sample = 0.75F * std::sin(
            2.0F * std::numbers::pi_v<float> * frequencyHz
            * static_cast<float>(frame) / static_cast<float>(PcmFormat::sampleRate));
        samples[frame * PcmFormat::channels] = sample;
        samples[frame * PcmFormat::channels + 1] = sample;
    }
    return samples;
}

[[nodiscard]] std::size_t strongestBand(const AudioSpectrumFrame& frame)
{
    return static_cast<std::size_t>(std::distance(frame.levels.cbegin(),
        std::max_element(frame.levels.cbegin(), frame.levels.cend())));
}

} // namespace

TEST_CASE("FFT analysis maps a sine wave into logarithmic spectrum bands")
{
    AudioSpectrumAnalyzer analyzer;
    REQUIRE(analyzer.available());

    const auto samples = sineWave(1'000.0F, 0.8F);
    const auto frame = analyzer.analyzeMono(samples);
    const auto band = strongestBand(frame);

    CHECK(frame.active);
    CHECK(frame.levels[band] > 0.75F);
    CHECK(frame.centerFrequenciesHz[band] > 800.0F);
    CHECK(frame.centerFrequenciesHz[band] < 1'250.0F);
    CHECK(frame.rms == Catch::Approx(0.8F / std::sqrt(2.0F)).margin(0.02F));
    CHECK(frame.peak == Catch::Approx(0.8F).margin(0.01F));

    const std::array<float, audioSpectrumFftSize> silence{};
    const auto silentFrame = analyzer.analyzeMono(silence);
    CHECK(std::ranges::all_of(silentFrame.levels,
        [](const float level) { return level == 0.0F; }));
    CHECK(silentFrame.rms == 0.0F);
    CHECK(silentFrame.peak == 0.0F);
}

TEST_CASE("Audio analysis engine transfers device samples without blocking its producer")
{
    AudioAnalysisEngine engine;
    REQUIRE(engine.available());
    const auto samples = interleavedSine(audioSpectrumFftSize * 3, 440.0F);
    engine.submitInterleaved(samples);

    AudioSpectrumFrame frame;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
    while (std::chrono::steady_clock::now() < deadline) {
        frame = engine.snapshot();
        if (frame.sequence > 0) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{2});
    }

    REQUIRE(frame.sequence > 0);
    const auto band = strongestBand(frame);
    CHECK(frame.centerFrequenciesHz[band] > 350.0F);
    CHECK(frame.centerFrequenciesHz[band] < 550.0F);
    CHECK(frame.levels[band] > 0.70F);
    CHECK(engine.droppedFrameCount() == 0);

    const auto previousSequence = frame.sequence;
    engine.reset();
    const auto resetDeadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
    while (std::chrono::steady_clock::now() < resetDeadline) {
        frame = engine.snapshot();
        if (frame.sequence > previousSequence && !frame.active
            && std::ranges::all_of(frame.levels,
                [](const float level) { return level == 0.0F; })) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{2});
    }
    REQUIRE(frame.sequence > previousSequence);
    CHECK_FALSE(frame.active);
    CHECK(std::ranges::all_of(frame.levels,
        [](const float level) { return level == 0.0F; }));
}

TEST_CASE("Audio visualization model publishes bounded read-only QML data")
{
    AudioAnalysisEngine engine;
    AudioVisualizationModel model{engine};
    bool frameChanged{};
    QObject::connect(&model, &AudioVisualizationModel::frameChanged,
        [&frameChanged] { frameChanged = true; });

    const auto samples = interleavedSine(audioSpectrumFftSize * 3, 220.0F);
    engine.submitInterleaved(samples);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
    while (!frameChanged && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds{2});
    }

    REQUIRE(frameChanged);
    CHECK(model.rowCount() == static_cast<int>(audioSpectrumBandCount));
    CHECK(model.revision() > 0);
    CHECK(model.levelAt(-1) == 0.0);
    CHECK(model.levelAt(model.count()) == 0.0);
    CHECK(model.roleNames().value(AudioVisualizationModel::LevelRole) == "level");
    CHECK(model.roleNames().value(AudioVisualizationModel::PeakRole) == "peakLevel");
    CHECK(model.roleNames().value(AudioVisualizationModel::FrequencyHzRole) == "frequencyHz");
}

} // namespace yaap
