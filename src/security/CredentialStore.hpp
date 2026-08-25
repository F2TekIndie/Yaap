#pragma once

#include <QByteArray>
#include <QString>

#include <memory>
#include <optional>

namespace yaap {

class CredentialStore {
public:
    virtual ~CredentialStore() = default;

    virtual bool save(const QString& key, const QByteArray& secret, QString& error) = 0;
    [[nodiscard]] virtual std::optional<QByteArray> load(
        const QString& key,
        QString& error) const = 0;
    virtual bool remove(const QString& key, QString& error) = 0;

    [[nodiscard]] static std::unique_ptr<CredentialStore> createPlatformStore();
};

} // namespace yaap
