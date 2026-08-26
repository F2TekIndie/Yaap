#pragma once

#include <QByteArray>
#include <QSqlDatabase>
#include <QString>

#include <optional>

namespace yaap {

struct ProviderCacheEntry final {
    QByteArray payload;
    QString etag;
    bool stale{};
};

class ProviderCache final {
public:
    ProviderCache();
    ~ProviderCache();

    ProviderCache(const ProviderCache&) = delete;
    ProviderCache& operator=(const ProviderCache&) = delete;

    bool open(const QString& path, QString& error);
    bool put(
        const QString& nameSpace,
        const QString& key,
        const QByteArray& payload,
        const QString& etag,
        qint64 lifetimeSeconds,
        QString& error);
    [[nodiscard]] std::optional<ProviderCacheEntry> get(
        const QString& nameSpace,
        const QString& key,
        bool allowStale,
        QString& error);
    [[nodiscard]] QStringList namespaces(QString& error) const;
    bool removeNamespace(const QString& nameSpace, QString& error);
    bool prune(qint64 maximumBytes, QString& error);

private:
    QString m_connectionName;
    QSqlDatabase m_database;
};

} // namespace yaap
