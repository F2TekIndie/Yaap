#pragma once

#include "audio/PcmFormat.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace yaap {

inline constexpr std::size_t audioSpectrumFftSize = 2'048;
inline constexpr std::size_t audioSpectrumBandCount = 48;

struct AudioSpectrumFrame final {
    std::array<float, audioSpectrumBandCount> levels{};
    std::array<float, audioSpectrumBandCount> peaks{};
    std::array<float, audioSpectrumBandCount> centerFrequenciesHz{};
    float rms{};
    float peak{};
    std::uint64_t sequence{};
    bool active{};
};

class AudioSpectrumAnalyzer final {
public:
    AudioSpectrumAnalyzer();
    ~AudioSpectrumAnalyzer();

    AudioSpectrumAnalyzer(const AudioSpectrumAnalyzer&) = delete;
    AudioSpectrumAnalyzer& operator=(const AudioSpectrumAnalyzer&) = delete;
    AudioSpectrumAnalyzer(AudioSpectrumAnalyzer&&) = delete;
    AudioSpectrumAnalyzer& operator=(AudioSpectrumAnalyzer&&) = delete;

    [[nodiscard]] bool available() const noexcept;
    [[nodiscard]] AudioSpectrumFrame analyzeMono(
        std::span<const float> samples) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

class AudioAnalysisEngine final {
public:
    AudioAnalysisEngine();
    ~AudioAnalysisEngine();

    AudioAnalysisEngine(const AudioAnalysisEngine&) = delete;
    AudioAnalysisEngine& operator=(const AudioAnalysisEngine&) = delete;
    AudioAnalysisEngine(AudioAnalysisEngine&&) = delete;
    AudioAnalysisEngine& operator=(AudioAnalysisEngine&&) = delete;

    // Called by the audio device thread. This operation never allocates,
    // locks, blocks, or performs spectral analysis. Samples are dropped when
    // the bounded analysis queue is full rather than delaying audio output.
    void submitInterleaved(std::span<const float> samples) noexcept;
    void reset() noexcept;

    [[nodiscard]] AudioSpectrumFrame snapshot() const;
    [[nodiscard]] bool available() const noexcept;
    [[nodiscard]] std::size_t droppedFrameCount() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace yaap
