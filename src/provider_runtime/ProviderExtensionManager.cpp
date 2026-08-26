#include "provider_runtime/ProviderExtensionManager.hpp"

#include "mods/ModManager.hpp"
#include "mods/PermissionStore.hpp"
#include "extension_api/ModPermission.hpp"
#include "extension_api/ProviderProtocol.hpp"
#include "provider_runtime/ProviderProcessSupervisor.hpp"
#include "providers/ProviderAccountStore.hpp"
#include "security/CredentialHandleBroker.hpp"

#include <QTimer>
#include <QDateTime>

#include <algorithm>

namespace yaap {

ProviderExtensionManager::ProviderExtensionManager(
    ModManager& mods,
    PermissionStore& permissions,
    ProviderAccountStore& accounts,
    CredentialHandleBroker& credentialHandles,
    QObject* parent)
    : QAbstractListModel(parent)
    , m_mods(mods)
    , m_permissions(permissions)
    , m_accounts(accounts)
    , m_credentialHandles(credentialHandles)
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
    case ErrorRole: return session.runtimeError.isEmpty()
        ? session.supervisor->errorMessage() : session.runtimeError;
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

QStringList ProviderExtensionManager::readyProviderIds() const
{
    QStringList result;
    for (const auto& session : m_sessions) {
        if (session.supervisor->isReady()) {
            result.push_back(session.providerId);
        }
    }
    return result;
}

quint64 ProviderExtensionManager::request(
    const QString& providerId,
    const QString& method,
    const QJsonObject& parameters)
{
    const auto iterator = std::ranges::find_if(m_sessions,
        [&](const Session& session) { return session.providerId == providerId; });
    return iterator == m_sessions.end()
        ? 0 : iterator->supervisor->sendRequest(method, parameters);
}

void ProviderExtensionManager::cancel(const QString& providerId, const quint64 requestId)
{
    const auto iterator = std::ranges::find_if(m_sessions,
        [&](const Session& session) { return session.providerId == providerId; });
    if (iterator != m_sessions.end()) {
        iterator->supervisor->cancel(requestId);
    }
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
        connect(supervisorPointer, &ProviderProcessSupervisor::readyChanged,
            this, &ProviderExtensionManager::providerAvailabilityChanged);
        connect(supervisorPointer, &ProviderProcessSupervisor::errorMessageChanged,
            this, updateSession);
        connect(supervisorPointer, &ProviderProcessSupervisor::responseReceived,
            this, [this, providerId = manifest.provider.providerId](
                const quint64 requestId, const QJsonValue& result, const QJsonObject& error) {
                emit providerResponse(providerId, requestId, result, error);
            });
        connect(supervisorPointer, &ProviderProcessSupervisor::hostRequestReceived,
            this, [this, supervisorPointer, providerId = manifest.provider.providerId,
                      modId = manifest.id](
                const quint64 requestId, const QString& method, const QJsonObject& parameters) {
                if (method != provider_protocol::hostCredentialRead) {
                    supervisorPointer->sendHostResponse(requestId, {},
                        QJsonObject{{"code", provider_protocol::error_code::invalidRequest},
                            {"message", "Host method is not supported."}});
                    return;
                }
                if (!m_permissions.isGranted(modId, permission::providerAccountRead)) {
                    supervisorPointer->sendHostResponse(requestId, {},
                        QJsonObject{{"code", provider_protocol::error_code::permissionDenied},
                            {"message", "Provider was not granted provider.account.read."}});
                    return;
                }
                QString error;
                auto secret = m_credentialHandles.consume(parameters.value("handle").toString(),
                    parameters.value("accountId").toString(), providerId, error);
                if (!secret) {
                    supervisorPointer->sendHostResponse(requestId, {},
                        QJsonObject{{"code", provider_protocol::error_code::permissionDenied},
                            {"message", error}});
                    return;
                }
                const auto encoded = QString::fromLatin1(secret->toBase64());
                secret->fill('\0');
                supervisorPointer->sendHostResponse(requestId,
                    QJsonObject{{"secretBase64", encoded}}, {});
            });
        connect(supervisorPointer, &ProviderProcessSupervisor::processStopped,
            this, [this, supervisorPointer] {
                const auto iterator = std::ranges::find_if(m_sessions,
                    [supervisorPointer](const Session& session) {
                        return session.supervisor.get() == supervisorPointer;
                    });
                if (iterator == m_sessions.end() || supervisorPointer->errorMessage().isEmpty()) {
                    return;
                }
                const auto now = QDateTime::currentSecsSinceEpoch();
                while (!iterator->crashTimesUtc.isEmpty()
                    && iterator->crashTimesUtc.front() < now - 60) {
                    iterator->crashTimesUtc.pop_front();
                }
                iterator->crashTimesUtc.push_back(now);
                const auto row = static_cast<int>(iterator - m_sessions.begin());
                if (iterator->crashTimesUtc.size() >= 3) {
                    iterator->runtimeError = "Provider restart suppressed after 3 failures in 60 seconds.";
                    emit dataChanged(index(row), index(row), {ReadyRole, ErrorRole});
                    return;
                }
                const auto delay = 250 * (1 << (iterator->crashTimesUtc.size() - 1));
                QTimer::singleShot(delay, this, [this, supervisorPointer] {
                    const auto current = std::ranges::find_if(m_sessions,
                        [supervisorPointer](const Session& session) {
                            return session.supervisor.get() == supervisorPointer;
                        });
                    if (current == m_sessions.end()) {
                        return;
                    }
                    QString error;
                    current->supervisor->start(current->executable, current->providerId,
                        current->permissions, error);
                });
            });
        QString error;
        const auto granted = m_permissions.granted(manifest.id);
        supervisor->start(manifest.provider.executablePath,
            manifest.provider.providerId, granted, error);
        m_sessions.push_back({manifest.id, manifest.provider.providerId,
            manifest.provider.executablePath, granted, {}, {}, std::move(supervisor)});
    }
    endResetModel();
    emit countChanged();
    emit providerAvailabilityChanged();
}

} // namespace yaap
