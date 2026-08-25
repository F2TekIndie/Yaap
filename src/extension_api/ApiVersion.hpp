#pragma once

#include <QString>

#include <compare>
#include <optional>

namespace yaap {

struct ApiVersion final {
    int major{};
    int minor{};

    [[nodiscard]] QString toString() const;
    [[nodiscard]] static std::optional<ApiVersion> parse(const QString& value);

    auto operator<=>(const ApiVersion&) const = default;
};

inline constexpr ApiVersion extensionApiVersion{1, 0};
inline constexpr int modManifestSchemaVersion = 1;
inline constexpr int providerProtocolVersion = 1;

} // namespace yaap
