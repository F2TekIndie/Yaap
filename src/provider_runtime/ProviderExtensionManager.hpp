#pragma once

#include <QAbstractListModel>

#include <memory>
#include <vector>

namespace yaap {

class ModManager;
class PermissionStore;
class ProviderProcessSupervisor;

class ProviderExtensionManager final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        ModIdRole = Qt::UserRole + 1,
        ProviderIdRole,
        ReadyRole,
        ErrorRole,
    };
    Q_ENUM(Role)

    ProviderExtensionManager(
        ModManager& mods,
        PermissionStore& permissions,
        QObject* parent = nullptr);
    ~ProviderExtensionManager() override;

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] int count() const noexcept;

signals:
    void countChanged();

private:
    struct Session final {
        QString modId;
        QString providerId;
        std::unique_ptr<ProviderProcessSupervisor> supervisor;
    };

    void reconcile();

    ModManager& m_mods;
    PermissionStore& m_permissions;
    std::vector<Session> m_sessions;
    std::vector<std::unique_ptr<ProviderProcessSupervisor>> m_retiringSupervisors;
};

} // namespace yaap
