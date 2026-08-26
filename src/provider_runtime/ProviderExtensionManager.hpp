#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QJsonObject>
#include <QJsonValue>

#include <memory>
#include <vector>

namespace yaap {

class ModManager;
class PermissionStore;
class ProviderProcessSupervisor;
class ProviderAccountStore;
class CredentialHandleBroker;

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
        ProviderAccountStore& accounts,
        CredentialHandleBroker& credentialHandles,
        QObject* parent = nullptr);
    ~ProviderExtensionManager() override;

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] int count() const noexcept;
    [[nodiscard]] QStringList readyProviderIds() const;
    quint64 request(
        const QString& providerId,
        const QString& method,
        const QJsonObject& parameters = {});
    void cancel(const QString& providerId, quint64 requestId);

signals:
    void countChanged();
    void providerAvailabilityChanged();
    void providerResponse(
        const QString& providerId,
        quint64 requestId,
        const QJsonValue& result,
        const QJsonObject& error);

private:
    struct Session final {
        QString modId;
        QString providerId;
        QString executable;
        QStringList permissions;
        QString runtimeError;
        QList<qint64> crashTimesUtc;
        std::unique_ptr<ProviderProcessSupervisor> supervisor;
    };

    void reconcile();

    ModManager& m_mods;
    PermissionStore& m_permissions;
    ProviderAccountStore& m_accounts;
    CredentialHandleBroker& m_credentialHandles;
    std::vector<Session> m_sessions;
    std::vector<std::unique_ptr<ProviderProcessSupervisor>> m_retiringSupervisors;
};

} // namespace yaap
