#pragma once

#include "providers/ProviderClients.hpp"

#include <QAbstractListModel>
#include <QSettings>
#include <QUrl>

#include <vector>
#include <functional>

namespace yaap {

class CredentialHandleBroker;
class CredentialStore;

class ProviderAccountStore final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

public:
    enum Role {
        AccountIdRole = Qt::UserRole + 1,
        ProviderIdRole,
        DisplayNameRole,
        ServerUrlRole,
        UsernameRole,
        EnabledRole,
        StatusRole,
    };
    Q_ENUM(Role)

    ProviderAccountStore(
        CredentialStore& credentials,
        CredentialHandleBroker& handles,
        QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] int count() const noexcept;
    [[nodiscard]] QString errorMessage() const;

    Q_INVOKABLE bool addAccount(
        const QString& providerId,
        const QString& displayName,
        const QString& serverUrl,
        const QString& username,
        const QString& password);
    Q_INVOKABLE bool removeAccount(const QString& accountId);
    Q_INVOKABLE bool setEnabled(const QString& accountId, bool enabled);
    Q_INVOKABLE void testConnection(const QString& accountId);
    [[nodiscard]] QString issueCredentialHandle(
        const QString& accountId,
        const QString& requestingProviderId);
    [[nodiscard]] QStringList enabledAccountIds() const;
    void searchAccount(
        const QString& accountId,
        const QString& query,
        std::function<void(ProviderTracksResult)> callback);

signals:
    void countChanged();
    void errorMessageChanged();
    void accountRemoved(const QString& accountId);

private:
    struct Account final {
        QString id;
        QString providerId;
        QString displayName;
        QUrl serverUrl;
        QString username;
        QString credentialKey;
        QString status;
        bool enabled{true};
    };

    void load();
    bool persist();
    void setError(QString error);
    [[nodiscard]] Account* find(const QString& accountId);

    CredentialStore& m_credentials;
    CredentialHandleBroker& m_handles;
    QSettings m_settings;
    std::vector<Account> m_accounts;
    QString m_errorMessage;
};

} // namespace yaap
