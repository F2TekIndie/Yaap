#pragma once

#include <cstddef>
#include <cstdint>

namespace yaap {

struct PcmFormat final {
    static constexpr std::uint32_t sampleRate = 48'000;
    static constexpr std::size_t channels = 2;
};

} // namespace yaap
