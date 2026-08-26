#pragma once

#include "audio/PcmStream.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace yaap {

class AudioAnalysisEngine;

class MiniaudioOutput final {
public:
    explicit MiniaudioOutput(AudioAnalysisEngine* analysisEngine = nullptr);
    ~MiniaudioOutput();

    MiniaudioOutput(const MiniaudioOutput&) = delete;
    MiniaudioOutput& operator=(const MiniaudioOutput&) = delete;
    MiniaudioOutput(MiniaudioOutput&&) = delete;
    MiniaudioOutput& operator=(MiniaudioOutput&&) = delete;

    [[nodiscard]] bool attach(std::shared_ptr<PcmStream> stream, std::string& error);
    [[nodiscard]] bool play(std::string& error);
    void pause() noexcept;
    void clear() noexcept;

    [[nodiscard]] bool hasAudio() const noexcept;
    [[nodiscard]] bool isPlaying() const noexcept;
    [[nodiscard]] bool isEndOfStream() const noexcept;
    [[nodiscard]] bool isFinished() const noexcept;
    [[nodiscard]] std::size_t underrunCount() const noexcept;
    [[nodiscard]] std::int64_t bufferedMilliseconds() const noexcept;
    [[nodiscard]] std::int64_t positionMilliseconds() const noexcept;
    [[nodiscard]] std::int64_t durationMilliseconds() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace yaap
