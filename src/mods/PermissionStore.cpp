#include "mods/PermissionStore.hpp"

#include "extension_api/ModPermission.hpp"

#include <QDateTime>

namespace yaap {

PermissionStore::PermissionStore(QObject* parent)
    : QObject(parent)
{
}

bool PermissionStore::isGranted(const QString& modId, const QString& requested) const
{
    return granted(modId).contains(requested);
}

bool PermissionStore::hasAllDeclared(const ModManifest& manifest) const
{
    const auto prefix = settingsKey(manifest.id);
    if (manifest.contentDigest.isEmpty()
        || m_settings.value(prefix + "/digest").toString() != manifest.contentDigest
        || m_settings.value(prefix + "/version").toString() != manifest.version) {
        return false;
    }
    const auto current = granted(manifest.id);
    for (const auto& requested : manifest.permissions) {
        if (!current.contains(requested)) {
            return false;
        }
    }
    return true;
}

QStringList PermissionStore::granted(const QString& modId) const
{
    return m_settings.value(settingsKey(modId) + "/permissions").toStringList();
}

bool PermissionStore::grantDeclared(const ModManifest& manifest, QString& error)
{
    if (manifest.contentDigest.isEmpty() || manifest.version.trimmed().isEmpty()) {
        error = "Cannot grant a package without a verified content digest and version.";
        return false;
    }
    for (const auto& requested : manifest.permissions) {
        if (!permission::isKnown(requested)) {
            error = "Cannot grant unknown permission: " + requested;
            return false;
        }
    }
    const auto prefix = settingsKey(manifest.id);
    m_settings.setValue(prefix + "/permissions", manifest.permissions);
    m_settings.setValue(prefix + "/digest", manifest.contentDigest);
    m_settings.setValue(prefix + "/version", manifest.version);
    m_settings.setValue(prefix + "/acceptedAtUtc",
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    m_settings.sync();
    if (m_settings.status() != QSettings::NoError) {
        error = "Could not persist mod permissions.";
        return false;
    }
    emit permissionsChanged(manifest.id);
    return true;
}

void PermissionStore::revokeAll(const QString& modId)
{
    m_settings.remove(settingsKey(modId));
    m_settings.sync();
    emit permissionsChanged(modId);
}

QString PermissionStore::settingsKey(const QString& modId) const
{
    return "mods/grants/" + modId;
}

} // namespace yaap
