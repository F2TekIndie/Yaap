#include "providers/ProviderAccountStore.hpp"

#include "providers/ProviderClients.hpp"
#include "security/CredentialHandleBroker.hpp"
#include "security/CredentialStore.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

#include <algorithm>
#include <utility>

namespace yaap {
namespace {
constexpr auto accountsSettingsKey = "providers/accounts-v1";
}

ProviderAccountStore::ProviderAccountStore(
    CredentialStore& credentials,
    CredentialHandleBroker& handles,
    QObject* parent)
    : QAbstractListModel(parent)
    , m_credentials(credentials)
    , m_handles(handles)
{
    load();
}

int ProviderAccountStore::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_accounts.size());
}

QVariant ProviderAccountStore::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0
        || index.row() >= static_cast<int>(m_accounts.size())) {
        return {};
    }
    const auto& account = m_accounts[static_cast<std::size_t>(index.row())];
    switch (role) {
    case AccountIdRole: return account.id;
    case ProviderIdRole: return account.providerId;
    case DisplayNameRole: return account.displayName;
    case ServerUrlRole: return account.serverUrl;
    case UsernameRole: return account.username;
    case EnabledRole: return account.enabled;
    case StatusRole: return account.status;
    default: return {};
    }
}

QHash<int, QByteArray> ProviderAccountStore::roleNames() const
{
    return {{AccountIdRole, "accountId"}, {ProviderIdRole, "providerId"},
        {DisplayNameRole, "displayName"}, {ServerUrlRole, "serverUrl"},
        {UsernameRole, "username"}, {EnabledRole, "accountEnabled"},
        {StatusRole, "status"}};
}

int ProviderAccountStore::count() const noexcept { return static_cast<int>(m_accounts.size()); }
QString ProviderAccountStore::errorMessage() const { return m_errorMessage; }

bool ProviderAccountStore::addAccount(
    const QString& providerId,
    const QString& displayName,
    const QString& serverUrl,
    const QString& username,
    const QString& password)
{
    const QUrl url{serverUrl.trimmed()};
    const auto supported = providerId == "opensubsonic" || providerId == "jellyfin";
    if (!supported || displayName.trimmed().isEmpty() || username.trimmed().isEmpty()
        || password.isEmpty() || !url.isValid()
        || (url.scheme() != "https" && url.scheme() != "http")) {
        setError("Provider, name, HTTP(S) server URL, username, and password are required.");
        return false;
    }
    Account account{.id = QUuid::createUuid().toString(QUuid::WithoutBraces),
        .providerId = providerId,
        .displayName = displayName.trimmed(),
        .serverUrl = url,
        .username = username.trimmed()};
    account.credentialKey = "provider-account/" + account.id;
    QString error;
    auto secret = password.toUtf8();
    const auto saved = m_credentials.save(account.credentialKey, secret, error);
    secret.fill('\0');
    if (!saved) {
        setError(std::move(error));
        return false;
    }
    const auto row = static_cast<int>(m_accounts.size());
    beginInsertRows({}, row, row);
    m_accounts.push_back(std::move(account));
    endInsertRows();
    if (!persist()) {
        auto removed = m_accounts.back();
        beginRemoveRows({}, row, row);
        m_accounts.pop_back();
        endRemoveRows();
        m_credentials.remove(removed.credentialKey, error);
        return false;
    }
    emit countChanged();
    setError({});
    return true;
}

bool ProviderAccountStore::removeAccount(const QString& accountId)
{
    const auto iterator = std::ranges::find_if(m_accounts,
        [&](const Account& account) { return account.id == accountId; });
    if (iterator == m_accounts.end()) {
        setError("Provider account was not found.");
        return false;
    }
    QString error;
    if (!m_credentials.remove(iterator->credentialKey, error)) {
        setError(std::move(error));
        return false;
    }
    m_handles.revokeAccount(accountId);
    emit accountRemoved(accountId);
    const auto row = static_cast<int>(iterator - m_accounts.begin());
    beginRemoveRows({}, row, row);
    m_accounts.erase(iterator);
    endRemoveRows();
    const auto saved = persist();
    emit countChanged();
    return saved;
}

bool ProviderAccountStore::setEnabled(const QString& accountId, const bool enabled)
{
    auto* account = find(accountId);
    if (account == nullptr) {
        setError("Provider account was not found.");
        return false;
    }
    account->enabled = enabled;
    const auto row = static_cast<int>(account - m_accounts.data());
    emit dataChanged(index(row), index(row), {EnabledRole});
    return persist();
}

void ProviderAccountStore::testConnection(const QString& accountId)
{
    auto* account = find(accountId);
    if (account == nullptr) {
        setError("Provider account was not found.");
        return;
    }
    account->status = "Testing…";
    const auto row = static_cast<int>(account - m_accounts.data());
    emit dataChanged(index(row), index(row), {StatusRole});
    const auto finish = [this, accountId](const bool ok, QString error) {
        auto* current = find(accountId);
        if (current == nullptr) {
            return;
        }
        current->status = ok ? "Connected" : std::move(error);
        const auto currentRow = static_cast<int>(current - m_accounts.data());
        emit dataChanged(index(currentRow), index(currentRow), {StatusRole});
    };
    if (account->providerId == "opensubsonic") {
        auto* client = new OpenSubsonicClient{m_credentials, this};
        client->setConfiguration({account->serverUrl, account->username,
            account->credentialKey});
        client->ping([client, finish](const bool ok, QString error) mutable {
            client->deleteLater();
            finish(ok, std::move(error));
        });
    } else {
        auto* client = new JellyfinClient{m_credentials, this};
        client->setConfiguration({account->serverUrl, account->username,
            account->credentialKey});
        client->authenticate([client, finish](const bool ok, QString error) mutable {
            client->deleteLater();
            finish(ok, std::move(error));
        });
    }
}

QString ProviderAccountStore::issueCredentialHandle(
    const QString& accountId,
    const QString& requestingProviderId)
{
    auto* account = find(accountId);
    if (account == nullptr || !account->enabled
        || account->providerId != requestingProviderId) {
        return {};
    }
    return m_handles.issue(account->id, account->providerId, account->credentialKey);
}

QStringList ProviderAccountStore::enabledAccountIds() const
{
    QStringList result;
    for (const auto& account : m_accounts) {
        if (account.enabled) {
            result.push_back(account.id);
        }
    }
    return result;
}

void ProviderAccountStore::searchAccount(
    const QString& accountId,
    const QString& query,
    std::function<void(ProviderTracksResult)> callback)
{
    auto* account = find(accountId);
    if (account == nullptr || !account->enabled) {
        callback({.error = "Provider account is unavailable."});
        return;
    }
    if (account->providerId == "opensubsonic") {
        auto* client = new OpenSubsonicClient{m_credentials, this};
        client->setConfiguration({account->serverUrl, account->username,
            account->credentialKey});
        client->search(query, [client, callback = std::move(callback)](
                                  ProviderTracksResult result) mutable {
            client->deleteLater();
            callback(std::move(result));
        });
        return;
    }
    auto* client = new JellyfinClient{m_credentials, this};
    client->setConfiguration({account->serverUrl, account->username,
        account->credentialKey});
    client->authenticate([client, query, callback = std::move(callback)](
                             const bool ok, QString error) mutable {
        if (!ok) {
            client->deleteLater();
            callback({.error = std::move(error)});
            return;
        }
        client->fetchTracks([client, query, callback = std::move(callback)](
                                ProviderTracksResult result) mutable {
            client->deleteLater();
            if (result.succeeded() && !query.trimmed().isEmpty()) {
                const auto needle = query.trimmed().toStdString();
                std::erase_if(result.tracks, [&](const Track& track) {
                    const auto matches = [&](const std::string& value) {
                        return QString::fromStdString(value).contains(
                            QString::fromStdString(needle), Qt::CaseInsensitive);
                    };
                    return !matches(track.title) && !matches(track.artist)
                        && !matches(track.album);
                });
            }
            callback(std::move(result));
        });
    });
}

void ProviderAccountStore::load()
{
    const auto document = QJsonDocument::fromJson(
        m_settings.value(accountsSettingsKey).toByteArray());
    for (const auto& value : document.array()) {
        const auto object = value.toObject();
        Account account{.id = object.value("id").toString(),
            .providerId = object.value("providerId").toString(),
            .displayName = object.value("displayName").toString(),
            .serverUrl = QUrl{object.value("serverUrl").toString()},
            .username = object.value("username").toString(),
            .credentialKey = object.value("credentialKey").toString(),
            .enabled = object.value("enabled").toBool(true)};
        if (!account.id.isEmpty() && !account.credentialKey.isEmpty()) {
            m_accounts.push_back(std::move(account));
        }
    }
}

bool ProviderAccountStore::persist()
{
    QJsonArray values;
    for (const auto& account : m_accounts) {
        values.push_back(QJsonObject{{"id", account.id}, {"providerId", account.providerId},
            {"displayName", account.displayName},
            {"serverUrl", account.serverUrl.toString(QUrl::FullyEncoded)},
            {"username", account.username}, {"credentialKey", account.credentialKey},
            {"enabled", account.enabled}});
    }
    m_settings.setValue(accountsSettingsKey,
        QJsonDocument{values}.toJson(QJsonDocument::Compact));
    m_settings.sync();
    if (m_settings.status() != QSettings::NoError) {
        setError("Could not persist provider accounts.");
        return false;
    }
    setError({});
    return true;
}

void ProviderAccountStore::setError(QString error)
{
    m_errorMessage = std::move(error);
    emit errorMessageChanged();
}

ProviderAccountStore::Account* ProviderAccountStore::find(const QString& accountId)
{
    const auto iterator = std::ranges::find_if(m_accounts,
        [&](const Account& account) { return account.id == accountId; });
    return iterator == m_accounts.end() ? nullptr : &*iterator;
}

} // namespace yaap
