#include "mods/ExtensionRegistry.hpp"

#include "extension_api/ModPermission.hpp"
#include "mods/PermissionStore.hpp"

#include <algorithm>

namespace yaap {

ExtensionRegistry::ExtensionRegistry(PermissionStore& permissions, QObject* parent)
    : QAbstractListModel(parent)
    , m_permissions(permissions)
{
}

int ExtensionRegistry::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant ExtensionRegistry::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0
        || index.row() >= static_cast<int>(m_entries.size())) {
        return {};
    }
    const auto& entry = m_entries[static_cast<std::size_t>(index.row())];
    switch (role) {
    case ModIdRole: return entry.modId;
    case SlotIdRole: return entry.slotId;
    case SourceRole: return entry.source;
    case OrderRole: return entry.order;
    default: return {};
    }
}

QHash<int, QByteArray> ExtensionRegistry::roleNames() const
{
    return {{ModIdRole, "modId"}, {SlotIdRole, "slotId"},
        {SourceRole, "componentSource"}, {OrderRole, "extensionOrder"}};
}

void ExtensionRegistry::rebuild(const std::vector<ModManifest>& enabledMods)
{
    std::vector<Entry> entries;
    for (const auto& manifest : enabledMods) {
        for (const auto& extension : manifest.uiExtensions) {
            if (!m_permissions.isGranted(
                    manifest.id, permission::uiSlot(extension.slotId))) {
                continue;
            }
            entries.push_back({manifest.id, extension.slotId,
                QUrl::fromLocalFile(extension.componentPath), extension.order});
        }
    }
    std::ranges::sort(entries, [](const Entry& left, const Entry& right) {
        return left.slotId == right.slotId
            ? std::tie(left.order, left.modId) < std::tie(right.order, right.modId)
            : left.slotId < right.slotId;
    });
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}

} // namespace yaap
