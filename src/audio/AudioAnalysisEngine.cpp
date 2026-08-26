#include "audio/AudioAnalysisEngine.hpp"

#include "audio/SpscPcmRingBuffer.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <limits>
#include <mutex>
#include <numeric>
#include <thread>
#include <utility>
#include <vector>

extern "C" {
#include <libavutil/tx.h>
}

namespace yaap {
namespace {

constexpr std::size_t analysisHopSize = audioSpectrumFftSize / 2;
constexpr std::size_t analysisQueueFrames = audioSpectrumFftSize * 8;
constexpr float minimumFrequencyHz = 40.0F;
constexpr float maximumFrequencyHz = 16'000.0F;
constexpr float minimumDecibels = -72.0F;
constexpr auto idlePollInterval = std::chrono::milliseconds{5};
constexpr auto decayInterval = std::chrono::milliseconds{33};
constexpr auto inactiveAfter = std::chrono::milliseconds{180};

struct BandRange final {
    std::size_t firstBin{};
    std::size_t lastBinExclusive{};
};

[[nodiscard]] float normalizedMagnitude(const float magnitude) noexcept
{
    if (magnitude <= std::numeric_limits<float>::epsilon()) {
        return 0.0F;
    }
    const auto decibels = 20.0F * std::log10(magnitude);
    return std::clamp((decibels - minimumDecibels) / -minimumDecibels, 0.0F, 1.0F);
}

} // namespace

struct AudioSpectrumAnalyzer::Impl final {
    AVTXContext* transformContext{};
    av_tx_fn transform{};
    std::vector<float> windowedSamples = std::vector<float>(audioSpectrumFftSize);
    std::vector<AVComplexFloat> transformOutput =
        std::vector<AVComplexFloat>(audioSpectrumFftSize / 2 + 1);
    std::array<float, audioSpectrumFftSize> hannWindow{};
    std::array<BandRange, audioSpectrumBandCount> bandRanges{};
    std::array<float, audioSpectrumBandCount> centerFrequenciesHz{};
    bool initialized{};

    Impl()
    {
        constexpr double pi = 3.14159265358979323846;
        for (std::size_t index = 0; index < hannWindow.size(); ++index) {
            const auto phase = (2.0 * pi * static_cast<double>(index))
                / static_cast<double>(hannWindow.size() - 1);
            hannWindow[index] = static_cast<float>(0.5 * (1.0 - std::cos(phase)));
        }

        const auto frequencyRatio = static_cast<double>(maximumFrequencyHz)
            / static_cast<double>(minimumFrequencyHz);
        for (std::size_t band = 0; band < audioSpectrumBandCount; ++band) {
            const auto lowerRatio = static_cast<double>(band)
                / static_cast<double>(audioSpectrumBandCount);
            const auto upperRatio = static_cast<double>(band + 1)
                / static_cast<double>(audioSpectrumBandCount);
            const auto lowerFrequency = static_cast<double>(minimumFrequencyHz)
                * std::pow(frequencyRatio, lowerRatio);
            const auto upperFrequency = static_cast<double>(minimumFrequencyHz)
                * std::pow(frequencyRatio, upperRatio);
            centerFrequenciesHz[band] = static_cast<float>(
                std::sqrt(lowerFrequency * upperFrequency));

            const auto firstBin = static_cast<std::size_t>(std::ceil(
                lowerFrequency * static_cast<double>(audioSpectrumFftSize)
                / static_cast<double>(PcmFormat::sampleRate)));
            const auto lastBin = static_cast<std::size_t>(std::ceil(
                upperFrequency * static_cast<double>(audioSpectrumFftSize)
                / static_cast<double>(PcmFormat::sampleRate)));
            bandRanges[band] = {
                std::clamp<std::size_t>(firstBin, 1, transformOutput.size() - 1),
                std::clamp<std::size_t>(
                    std::max(lastBin, firstBin + 1), 2, transformOutput.size())};
            if (bandRanges[band].lastBinExclusive <= bandRanges[band].firstBin) {
                bandRanges[band].lastBinExclusive = bandRanges[band].firstBin + 1;
            }
        }

        initialized = av_tx_init(&transformContext, &transform, AV_TX_FLOAT_RDFT,
            0, static_cast<int>(audioSpectrumFftSize), nullptr, AV_TX_UNALIGNED) >= 0;
    }

    ~Impl()
    {
        av_tx_uninit(&transformContext);
    }
};

AudioSpectrumAnalyzer::AudioSpectrumAnalyzer()
    : m_impl(std::make_unique<Impl>())
{
}

AudioSpectrumAnalyzer::~AudioSpectrumAnalyzer() = default;

bool AudioSpectrumAnalyzer::available() const noexcept
{
    return m_impl->initialized;
}

AudioSpectrumFrame AudioSpectrumAnalyzer::analyzeMono(
    const std::span<const float> samples) noexcept
{
    AudioSpectrumFrame frame;
    frame.centerFrequenciesHz = m_impl->centerFrequenciesHz;
    if (!m_impl->initialized || samples.size() != audioSpectrumFftSize) {
        return frame;
    }

    double squareSum{};
    float peak{};
    for (std::size_t index = 0; index < samples.size(); ++index) {
        const auto sample = std::clamp(samples[index], -1.0F, 1.0F);
        squareSum += static_cast<double>(sample) * static_cast<double>(sample);
        peak = std::max(peak, std::abs(sample));
        m_impl->windowedSamples[index] = sample * m_impl->hannWindow[index];
    }
    frame.rms = static_cast<float>(std::sqrt(squareSum / static_cast<double>(samples.size())));
    frame.peak = peak;

    m_impl->transform(m_impl->transformContext, m_impl->transformOutput.data(),
        m_impl->windowedSamples.data(), static_cast<std::ptrdiff_t>(sizeof(float)));

    // A Hann window has a coherent gain of approximately 0.5. The 4/N
    // factor compensates for that gain and for the discarded negative half
    // of the real-valued spectrum.
    constexpr auto magnitudeScale = 4.0F / static_cast<float>(audioSpectrumFftSize);
    for (std::size_t band = 0; band < audioSpectrumBandCount; ++band) {
        const auto range = m_impl->bandRanges[band];
        float maximumMagnitude{};
        for (auto bin = range.firstBin; bin < range.lastBinExclusive; ++bin) {
            const auto& value = m_impl->transformOutput[bin];
            maximumMagnitude = std::max(maximumMagnitude,
                std::hypot(value.re, value.im) * magnitudeScale);
        }
        frame.levels[band] = normalizedMagnitude(maximumMagnitude);
        frame.peaks[band] = frame.levels[band];
    }
    frame.active = true;
    return frame;
}

struct AudioAnalysisEngine::Impl final {
    SpscPcmRingBuffer queue{analysisQueueFrames};
    AudioSpectrumAnalyzer analyzer;
    mutable std::mutex snapshotMutex;
    AudioSpectrumFrame currentFrame;
    std::atomic<std::size_t> droppedFrames{};
    std::atomic<std::uint64_t> resetGeneration{};
    std::uint64_t nextSequence{};
    std::jthread worker;

    [[nodiscard]] static AudioSpectrumFrame initialFrame(AudioSpectrumAnalyzer& analyzer)
    {
        std::array<float, audioSpectrumFftSize> silence{};
        return analyzer.analyzeMono(silence);
    }

    Impl()
        : currentFrame(initialFrame(analyzer))
        , worker([this](const std::stop_token stopToken) { run(stopToken); })
    {
    }

    void publish(AudioSpectrumFrame frame)
    {
        frame.sequence = ++nextSequence;
        std::scoped_lock lock{snapshotMutex};
        currentFrame = std::move(frame);
    }

    void run(const std::stop_token stopToken)
    {
        std::array<float, analysisHopSize * PcmFormat::channels> interleaved{};
        std::array<float, audioSpectrumFftSize> monoWindow{};
        std::size_t collectedSamples{};
        auto lastInput = std::chrono::steady_clock::now();
        auto lastPublish = lastInput;
        AudioSpectrumFrame lastFrame;
        lastFrame.centerFrequenciesHz = currentFrame.centerFrequenciesHz;
        bool publishedAudioFrame{};
        std::uint64_t observedResetGeneration{};

        while (!stopToken.stop_requested()) {
            const auto requestedResetGeneration = resetGeneration.load(std::memory_order_acquire);
            if (requestedResetGeneration != observedResetGeneration) {
                while (queue.bufferedFrames() > 0) {
                    const auto framesToDiscard = std::min(
                        queue.bufferedFrames(), analysisHopSize);
                    [[maybe_unused]] const auto discarded = queue.read(
                        interleaved, framesToDiscard);
                }
                monoWindow.fill(0.0F);
                collectedSamples = 0;
                lastFrame = {};
                lastFrame.centerFrequenciesHz = currentFrame.centerFrequenciesHz;
                lastInput = std::chrono::steady_clock::now();
                lastPublish = lastInput;
                publishedAudioFrame = false;
                publish(lastFrame);
                observedResetGeneration = requestedResetGeneration;
            }

            bool processedInput{};
            while (queue.bufferedFrames() >= analysisHopSize) {
                const auto readFrames = queue.read(interleaved, analysisHopSize);
                if (readFrames != analysisHopSize) {
                    break;
                }
                processedInput = true;
                lastInput = std::chrono::steady_clock::now();

                if (collectedSamples == audioSpectrumFftSize) {
                    std::move(monoWindow.begin() + static_cast<std::ptrdiff_t>(analysisHopSize),
                        monoWindow.end(), monoWindow.begin());
                    collectedSamples -= analysisHopSize;
                }
                for (std::size_t frame = 0; frame < analysisHopSize; ++frame) {
                    const auto sampleOffset = frame * PcmFormat::channels;
                    monoWindow[collectedSamples + frame] =
                        (interleaved[sampleOffset] + interleaved[sampleOffset + 1]) * 0.5F;
                }
                collectedSamples += analysisHopSize;

                if (collectedSamples == audioSpectrumFftSize) {
                    auto analyzed = analyzer.analyzeMono(monoWindow);
                    for (std::size_t band = 0; band < audioSpectrumBandCount; ++band) {
                        analyzed.peaks[band] = std::max(analyzed.levels[band],
                            std::max(0.0F, lastFrame.peaks[band] - 0.025F));
                    }
                    analyzed.active = true;
                    lastFrame = analyzed;
                    publish(analyzed);
                    lastPublish = std::chrono::steady_clock::now();
                    publishedAudioFrame = true;
                }
            }

            const auto now = std::chrono::steady_clock::now();
            if (!processedInput && publishedAudioFrame
                && now - lastPublish >= decayInterval) {
                const auto wasActive = lastFrame.active;
                lastFrame.active = now - lastInput < inactiveAfter;
                lastFrame.rms *= 0.80F;
                lastFrame.peak *= 0.80F;
                bool hasVisibleLevel{};
                for (std::size_t band = 0; band < audioSpectrumBandCount; ++band) {
                    lastFrame.levels[band] *= 0.80F;
                    lastFrame.peaks[band] = std::max(lastFrame.levels[band],
                        std::max(0.0F, lastFrame.peaks[band] - 0.025F));
                    hasVisibleLevel = hasVisibleLevel
                        || lastFrame.levels[band] > 0.001F
                        || lastFrame.peaks[band] > 0.001F;
                }
                if (hasVisibleLevel || wasActive || lastFrame.active) {
                    publish(lastFrame);
                    lastPublish = now;
                } else {
                    publishedAudioFrame = false;
                }
            }

            std::this_thread::sleep_for(idlePollInterval);
        }
    }
};

AudioAnalysisEngine::AudioAnalysisEngine()
    : m_impl(std::make_unique<Impl>())
{
}

AudioAnalysisEngine::~AudioAnalysisEngine() = default;

void AudioAnalysisEngine::submitInterleaved(const std::span<const float> samples) noexcept
{
    const auto offeredFrames = samples.size() / PcmFormat::channels;
    const auto writtenFrames = m_impl->queue.write(samples);
    if (writtenFrames < offeredFrames) {
        m_impl->droppedFrames.fetch_add(
            offeredFrames - writtenFrames, std::memory_order_relaxed);
    }
}

void AudioAnalysisEngine::reset() noexcept
{
    m_impl->resetGeneration.fetch_add(1, std::memory_order_release);
}

AudioSpectrumFrame AudioAnalysisEngine::snapshot() const
{
    std::scoped_lock lock{m_impl->snapshotMutex};
    return m_impl->currentFrame;
}

bool AudioAnalysisEngine::available() const noexcept
{
    return m_impl->analyzer.available();
}

std::size_t AudioAnalysisEngine::droppedFrameCount() const noexcept
{
    return m_impl->droppedFrames.load(std::memory_order_acquire);
}

} // namespace yaap
