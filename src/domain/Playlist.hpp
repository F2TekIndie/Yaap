#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace yaap {

struct Playlist final {
    std::int64_t id{};
    std::string name;
    std::vector<std::string> trackIds;
};

} // namespace yaap
