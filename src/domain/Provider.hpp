#pragma once

#include <cstdint>
#include <string>

namespace yaap {

enum class ProviderKind {
    LocalLibrary,
    InternetRadio,
    OpenSubsonic,
    Jellyfin,
};

enum class ProviderCapability : std::uint32_t {
    None = 0,
    Browse = 1U << 0U,
    Search = 1U << 1U,
    Stream = 1U << 2U,
    Playlists = 1U << 3U,
    Artwork = 1U << 4U,
};

[[nodiscard]] constexpr ProviderCapability operator|(
    const ProviderCapability left,
    const ProviderCapability right) noexcept
{
    return static_cast<ProviderCapability>(
        static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
}

[[nodiscard]] constexpr bool hasCapability(
    const ProviderCapability capabilities,
    const ProviderCapability expected) noexcept
{
    return (static_cast<std::uint32_t>(capabilities)
        & static_cast<std::uint32_t>(expected)) != 0;
}

struct ProviderDescriptor final {
    std::string id;
    std::string displayName;
    ProviderKind kind{ProviderKind::LocalLibrary};
    ProviderCapability capabilities{ProviderCapability::None};

    [[nodiscard]] bool isValid() const noexcept
    {
        return !id.empty() && !displayName.empty();
    }
};

} // namespace yaap
