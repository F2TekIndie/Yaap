#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace yaap {

struct DecodedAudio final {
    static constexpr std::uint32_t outputSampleRate = 48'000;
    static constexpr std::uint32_t outputChannels = 2;

    std::vector<float> interleavedSamples;
    std::string title;
    std::int64_t durationMilliseconds{};
};

} // namespace yaap

