#pragma once

#include "domain/NowPlayingMetadata.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace yaap {

class StreamMetadataParser final {
public:
    using Field = std::pair<std::string, std::string>;

    [[nodiscard]] static std::optional<NowPlayingMetadata> parseIcy(
        std::string_view rawMetadata);
    [[nodiscard]] static std::optional<NowPlayingMetadata> parseFields(
        const std::vector<Field>& fields,
        NowPlayingMetadataSource source);
};

} // namespace yaap
