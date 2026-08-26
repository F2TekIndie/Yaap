#pragma once

#include <QDateTime>
#include <QHash>
#include <QString>

#include <optional>

namespace yaap {

class CredentialStore;

class CredentialHandleBroker final {
public:
    explicit CredentialHandleBroker(CredentialStore& store);

    [[nodiscard]] QString issue(
        const QString& accountId,
        const QString& providerId,
        const QString& credentialKey,
        int lifetimeSeconds = 60);
    [[nodiscard]] std::optional<QByteArray> consume(
        const QString& handle,
        const QString& accountId,
        const QString& providerId,
        QString& error);
    void revokeAccount(const QString& accountId);
    void purgeExpired();

private:
    struct Grant final {
        QString accountId;
        QString providerId;
        QString credentialKey;
        QDateTime expiresAtUtc;
    };

    CredentialStore& m_store;
    QHash<QString, Grant> m_grants;
};

} // namespace yaap
