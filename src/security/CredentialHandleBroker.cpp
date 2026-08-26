#include "security/CredentialHandleBroker.hpp"

#include "security/CredentialStore.hpp"

#include <QUuid>

namespace yaap {

CredentialHandleBroker::CredentialHandleBroker(CredentialStore& store)
    : m_store(store)
{
}

QString CredentialHandleBroker::issue(
    const QString& accountId,
    const QString& providerId,
    const QString& credentialKey,
    const int lifetimeSeconds)
{
    if (accountId.isEmpty() || providerId.isEmpty() || credentialKey.isEmpty()
        || lifetimeSeconds < 1 || lifetimeSeconds > 300) {
        return {};
    }
    purgeExpired();
    const auto handle = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_grants.insert(handle, {accountId, providerId, credentialKey,
        QDateTime::currentDateTimeUtc().addSecs(lifetimeSeconds)});
    return handle;
}

std::optional<QByteArray> CredentialHandleBroker::consume(
    const QString& handle,
    const QString& accountId,
    const QString& providerId,
    QString& error)
{
    purgeExpired();
    const auto iterator = m_grants.find(handle);
    if (iterator == m_grants.end() || iterator->accountId != accountId
        || iterator->providerId != providerId) {
        error = "Credential handle is invalid, expired, or outside its provider scope.";
        return std::nullopt;
    }
    const auto credentialKey = iterator->credentialKey;
    m_grants.erase(iterator); // Handles are intentionally one-shot.
    return m_store.load(credentialKey, error);
}

void CredentialHandleBroker::revokeAccount(const QString& accountId)
{
    for (auto iterator = m_grants.begin(); iterator != m_grants.end();) {
        iterator = iterator->accountId == accountId ? m_grants.erase(iterator) : ++iterator;
    }
}

void CredentialHandleBroker::purgeExpired()
{
    const auto now = QDateTime::currentDateTimeUtc();
    for (auto iterator = m_grants.begin(); iterator != m_grants.end();) {
        iterator = iterator->expiresAtUtc <= now ? m_grants.erase(iterator) : ++iterator;
    }
}

} // namespace yaap
