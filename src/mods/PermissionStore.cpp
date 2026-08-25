#include "mods/PermissionStore.hpp"

#include "extension_api/ModPermission.hpp"

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
    return m_settings.value(settingsKey(modId)).toStringList();
}

bool PermissionStore::grantDeclared(const ModManifest& manifest, QString& error)
{
    for (const auto& requested : manifest.permissions) {
        if (!permission::isKnown(requested)) {
            error = "Cannot grant unknown permission: " + requested;
            return false;
        }
    }
    m_settings.setValue(settingsKey(manifest.id), manifest.permissions);
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
    return "mods/permissions/" + modId;
}

} // namespace yaap
