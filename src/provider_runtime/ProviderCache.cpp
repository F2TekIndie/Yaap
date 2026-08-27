#include "provider_runtime/ProviderCache.hpp"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace yaap {
namespace {
[[nodiscard]] QString sqlError(const QSqlQuery& query)
{
    return "Provider cache database error: " + query.lastError().text();
}
}

ProviderCache::ProviderCache()
    : m_connectionName{"yaap-provider-cache-" + QUuid::createUuid().toString(QUuid::WithoutBraces)}
{
}

ProviderCache::~ProviderCache()
{
    if (m_database.isValid()) {
        m_database.close();
        m_database = {};
    }
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool ProviderCache::open(const QString& path, QString& error)
{
    QDir{}.mkpath(QFileInfo{path}.absolutePath());
    m_database = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    m_database.setDatabaseName(path);
    if (!m_database.open()) {
        error = "Could not open provider cache: " + m_database.lastError().text();
        return false;
    }
    QSqlQuery query{m_database};
    if (!query.exec("CREATE TABLE IF NOT EXISTS provider_cache ("
            "namespace TEXT NOT NULL, cache_key TEXT NOT NULL, payload BLOB NOT NULL, "
            "etag TEXT NOT NULL DEFAULT '', expires_utc INTEGER NOT NULL, "
            "accessed_utc INTEGER NOT NULL, payload_bytes INTEGER NOT NULL, "
            "PRIMARY KEY(namespace, cache_key))")) {
        error = sqlError(query);
        return false;
    }
    return true;
}

bool ProviderCache::put(
    const QString& nameSpace,
    const QString& key,
    const QByteArray& payload,
    const QString& etag,
    const qint64 lifetimeSeconds,
    QString& error)
{
    if (!m_database.isOpen() || nameSpace.isEmpty() || key.isEmpty()
        || payload.isEmpty() || payload.size() > 8 * 1024 * 1024
        || lifetimeSeconds < 1 || lifetimeSeconds > 30 * 24 * 60 * 60) {
        error = "Provider cache entry is invalid or outside supported bounds.";
        return false;
    }
    // Millisecond precision avoids entries with a one-second TTL expiring
    // immediately when insertion straddles a wall-clock second boundary.
    const auto now = QDateTime::currentMSecsSinceEpoch();
    QSqlQuery query{m_database};
    query.prepare("INSERT INTO provider_cache(namespace,cache_key,payload,etag,expires_utc,"
        "accessed_utc,payload_bytes) VALUES(?,?,?,?,?,?,?) ON CONFLICT(namespace,cache_key) "
        "DO UPDATE SET payload=excluded.payload,etag=excluded.etag,"
        "expires_utc=excluded.expires_utc,accessed_utc=excluded.accessed_utc,"
        "payload_bytes=excluded.payload_bytes");
    query.addBindValue(nameSpace);
    query.addBindValue(key);
    query.addBindValue(payload);
    query.addBindValue(etag);
    query.addBindValue(now + lifetimeSeconds * 1'000);
    query.addBindValue(now);
    query.addBindValue(payload.size());
    if (!query.exec()) {
        error = sqlError(query);
        return false;
    }
    return true;
}

std::optional<ProviderCacheEntry> ProviderCache::get(
    const QString& nameSpace,
    const QString& key,
    const bool allowStale,
    QString& error)
{
    QSqlQuery query{m_database};
    query.prepare("SELECT payload,etag,expires_utc FROM provider_cache "
        "WHERE namespace=? AND cache_key=?");
    query.addBindValue(nameSpace);
    query.addBindValue(key);
    if (!query.exec()) {
        error = sqlError(query);
        return std::nullopt;
    }
    if (!query.next()) {
        return std::nullopt;
    }
    const auto now = QDateTime::currentMSecsSinceEpoch();
    const auto stale = query.value(2).toLongLong() <= now;
    if (stale && !allowStale) {
        return std::nullopt;
    }
    QSqlQuery touch{m_database};
    touch.prepare("UPDATE provider_cache SET accessed_utc=? WHERE namespace=? AND cache_key=?");
    touch.addBindValue(now);
    touch.addBindValue(nameSpace);
    touch.addBindValue(key);
    touch.exec();
    return ProviderCacheEntry{query.value(0).toByteArray(), query.value(1).toString(), stale};
}

QStringList ProviderCache::namespaces(QString& error) const
{
    QSqlQuery query{m_database};
    if (!query.exec("SELECT DISTINCT namespace FROM provider_cache ORDER BY namespace")) {
        error = sqlError(query);
        return {};
    }
    QStringList result;
    while (query.next()) {
        result.push_back(query.value(0).toString());
    }
    return result;
}

bool ProviderCache::removeNamespace(const QString& nameSpace, QString& error)
{
    QSqlQuery query{m_database};
    query.prepare("DELETE FROM provider_cache WHERE namespace=?");
    query.addBindValue(nameSpace);
    if (!query.exec()) {
        error = sqlError(query);
        return false;
    }
    return true;
}

bool ProviderCache::prune(const qint64 maximumBytes, QString& error)
{
    if (maximumBytes < 0) {
        error = "Provider cache byte limit cannot be negative.";
        return false;
    }
    QSqlQuery size{m_database};
    if (!size.exec("SELECT COALESCE(SUM(payload_bytes),0) FROM provider_cache") || !size.next()) {
        error = sqlError(size);
        return false;
    }
    auto currentBytes = size.value(0).toLongLong();
    QSqlQuery oldest{m_database};
    if (!oldest.exec("SELECT namespace,cache_key,payload_bytes FROM provider_cache "
            "ORDER BY accessed_utc ASC")) {
        error = sqlError(oldest);
        return false;
    }
    while (currentBytes > maximumBytes && oldest.next()) {
        QSqlQuery remove{m_database};
        remove.prepare("DELETE FROM provider_cache WHERE namespace=? AND cache_key=?");
        remove.addBindValue(oldest.value(0));
        remove.addBindValue(oldest.value(1));
        if (!remove.exec()) {
            error = sqlError(remove);
            return false;
        }
        currentBytes -= oldest.value(2).toLongLong();
    }
    return true;
}

} // namespace yaap
