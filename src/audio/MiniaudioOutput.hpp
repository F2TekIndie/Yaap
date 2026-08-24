#pragma once

#include "audio/DecodedAudio.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace yaap {

class MiniaudioOutput final {
public:
    MiniaudioOutput();
    ~MiniaudioOutput();

    MiniaudioOutput(const MiniaudioOutput&) = delete;
    MiniaudioOutput& operator=(const MiniaudioOutput&) = delete;
    MiniaudioOutput(MiniaudioOutput&&) = delete;
    MiniaudioOutput& operator=(MiniaudioOutput&&) = delete;

    [[nodiscard]] bool load(DecodedAudio audio, std::string& error);
    [[nodiscard]] bool play(std::string& error);
    void pause() noexcept;
    void stop() noexcept;

    [[nodiscard]] bool hasAudio() const noexcept;
    [[nodiscard]] bool isPlaying() const noexcept;
    [[nodiscard]] bool isFinished() const noexcept;
    [[nodiscard]] std::int64_t positionMilliseconds() const noexcept;
    [[nodiscard]] std::int64_t durationMilliseconds() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace yaap

