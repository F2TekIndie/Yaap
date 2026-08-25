#include "extension_api/ModManifest.hpp"

#include <algorithm>

namespace yaap {

bool ModManifest::hasKind(const ModKind kind) const noexcept
{
    return std::ranges::find(kinds, kind) != kinds.end();
}

QStringList ModManifest::kindNames() const
{
    QStringList result;
    for (const auto kind : kinds) {
        switch (kind) {
        case ModKind::Theme: result.push_back("theme"); break;
        case ModKind::UiExtension: result.push_back("ui-extension"); break;
        case ModKind::Provider: result.push_back("provider"); break;
        }
    }
    return result;
}

} // namespace yaap
