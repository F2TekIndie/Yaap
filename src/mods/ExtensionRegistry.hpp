#pragma once

#include "extension_api/ModManifest.hpp"

#include <QAbstractListModel>
#include <QUrl>

#include <vector>

namespace yaap {

class PermissionStore;

class ExtensionRegistry final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        ModIdRole = Qt::UserRole + 1,
        SlotIdRole,
        SourceRole,
        OrderRole,
    };
    Q_ENUM(Role)

    explicit ExtensionRegistry(PermissionStore& permissions, QObject* parent = nullptr);
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    void rebuild(const std::vector<ModManifest>& enabledMods);

private:
    struct Entry final {
        QString modId;
        QString slotId;
        QUrl source;
        int order{};
    };

    PermissionStore& m_permissions;
    std::vector<Entry> m_entries;
};

} // namespace yaap
