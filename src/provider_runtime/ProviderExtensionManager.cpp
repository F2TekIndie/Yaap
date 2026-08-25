#include "provider_runtime/ProviderExtensionManager.hpp"

#include "mods/ModManager.hpp"
#include "mods/PermissionStore.hpp"
#include "provider_runtime/ProviderProcessSupervisor.hpp"

#include <QTimer>

#include <algorithm>

namespace yaap {

ProviderExtensionManager::ProviderExtensionManager(
    ModManager& mods,
    PermissionStore& permissions,
    QObject* parent)
    : QAbstractListModel(parent)
    , m_mods(mods)
    , m_permissions(permissions)
{
    connect(&m_mods, &ModManager::runtimeChanged,
        this, &ProviderExtensionManager::reconcile);
    reconcile();
}

ProviderExtensionManager::~ProviderExtensionManager() = default;

int ProviderExtensionManager::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_sessions.size());
}

QVariant ProviderExtensionManager::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0
        || index.row() >= static_cast<int>(m_sessions.size())) {
        return {};
    }
    const auto& session = m_sessions[static_cast<std::size_t>(index.row())];
    switch (role) {
    case ModIdRole: return session.modId;
    case ProviderIdRole: return session.providerId;
    case ReadyRole: return session.supervisor->isReady();
    case ErrorRole: return session.supervisor->errorMessage();
    default: return {};
    }
}

QHash<int, QByteArray> ProviderExtensionManager::roleNames() const
{
    return {{ModIdRole, "modId"}, {ProviderIdRole, "providerId"},
        {ReadyRole, "ready"}, {ErrorRole, "errorMessage"}};
}

int ProviderExtensionManager::count() const noexcept
{
    return static_cast<int>(m_sessions.size());
}

void ProviderExtensionManager::reconcile()
{
    const auto manifests = m_mods.enabledProviders();
    beginResetModel();
    for (auto& session : m_sessions) {
        auto* retiring = session.supervisor.get();
        connect(retiring, &ProviderProcessSupervisor::processStopped, this,
            [this, retiring] {
                QTimer::singleShot(0, this, [this, retiring] {
                    std::erase_if(m_retiringSupervisors,
                        [retiring](const auto& supervisor) {
                            return supervisor.get() == retiring;
                        });
                });
            });
        retiring->stop();
        m_retiringSupervisors.push_back(std::move(session.supervisor));
    }
    m_sessions.clear();
    for (const auto& manifest : manifests) {
        auto supervisor = std::make_unique<ProviderProcessSupervisor>();
        auto* supervisorPointer = supervisor.get();
        const auto updateSession = [this, supervisorPointer] {
            const auto iterator = std::ranges::find_if(m_sessions,
                [supervisorPointer](const Session& session) {
                    return session.supervisor.get() == supervisorPointer;
                });
            if (iterator == m_sessions.end()) {
                return;
            }
            const auto row = static_cast<int>(iterator - m_sessions.begin());
            emit dataChanged(index(row), index(row), {ReadyRole, ErrorRole});
        };
        connect(supervisorPointer, &ProviderProcessSupervisor::readyChanged,
            this, updateSession);
        connect(supervisorPointer, &ProviderProcessSupervisor::errorMessageChanged,
            this, updateSession);
        QString error;
        supervisor->start(manifest.provider.executablePath,
            manifest.provider.providerId, m_permissions.granted(manifest.id), error);
        m_sessions.push_back({manifest.id, manifest.provider.providerId, std::move(supervisor)});
    }
    endResetModel();
    emit countChanged();
}

} // namespace yaap
