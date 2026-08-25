#include "mods/ModManager.hpp"

#include "mods/ExtensionRegistry.hpp"
#include "mods/ModManifestParser.hpp"
#include "mods/PermissionStore.hpp"
#include "mods/ThemeManager.hpp"

#include <QDir>
#include <QFileInfo>
#include <QSet>

#include <algorithm>
#include <utility>

namespace yaap {

ModManager::ModManager(
    PermissionStore& permissions,
    ThemeManager& themes,
    ExtensionRegistry& extensions,
    QStringList searchRoots,
    QObject* parent)
    : QAbstractListModel(parent)
    , m_permissions(permissions)
    , m_themes(themes)
    , m_extensions(extensions)
    , m_searchRoots(std::move(searchRoots))
{
    connect(&m_permissions, &PermissionStore::permissionsChanged,
        this, [this](const QString&) { rebuildRuntime(); });
    refresh();
}

int ModManager::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant ModManager::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0
        || index.row() >= static_cast<int>(m_entries.size())) {
        return {};
    }
    const auto& entry = m_entries[static_cast<std::size_t>(index.row())];
    switch (role) {
    case IdRole: return entry.manifest.id.isEmpty() ? entry.packageName : entry.manifest.id;
    case NameRole: return entry.manifest.name.isEmpty() ? entry.packageName : entry.manifest.name;
    case VersionRole: return entry.manifest.version;
    case KindsRole: return entry.manifest.kindNames();
    case PermissionsRole: return entry.manifest.permissions;
    case EnabledRole: return entry.enabled;
    case PermissionsGrantedRole: return !entry.manifest.id.isEmpty()
        && m_permissions.hasAllDeclared(entry.manifest);
    case DiagnosticRole: return entry.diagnostic;
    case IsThemeRole: return entry.manifest.hasKind(ModKind::Theme);
    case IsUiExtensionRole: return entry.manifest.hasKind(ModKind::UiExtension);
    case IsProviderRole: return entry.manifest.hasKind(ModKind::Provider);
    default: return {};
    }
}

QHash<int, QByteArray> ModManager::roleNames() const
{
    return {{IdRole, "modId"}, {NameRole, "name"}, {VersionRole, "version"},
        {KindsRole, "kinds"}, {PermissionsRole, "permissions"},
        {EnabledRole, "modEnabled"}, {PermissionsGrantedRole, "permissionsGranted"},
        {DiagnosticRole, "diagnostic"}, {IsThemeRole, "isTheme"},
        {IsUiExtensionRole, "isUiExtension"}, {IsProviderRole, "isProvider"}};
}

QString ModManager::errorMessage() const
{
    return m_errorMessage;
}

std::vector<ModManifest> ModManager::enabledProviders() const
{
    std::vector<ModManifest> result;
    for (const auto& entry : m_entries) {
        if (entry.enabled && entry.diagnostic.isEmpty()
            && entry.manifest.hasKind(ModKind::Provider)
            && m_permissions.hasAllDeclared(entry.manifest)) {
            result.push_back(entry.manifest);
        }
    }
    return result;
}

void ModManager::refresh()
{
    // PROTOTYPE: Mod discovery is deliberately startup/manual only. Atomic
    // package installation and digest-bound trust must precede folder watching.
    std::vector<Entry> entries;
    QSet<QString> identifiers;
    for (const auto& root : m_searchRoots) {
        QDir directory{root};
        for (const auto& packageName : directory.entryList(
                 QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase)) {
            const auto packageRoot = directory.filePath(packageName);
            auto result = ModManifestParser::parsePackage(packageRoot);
            Entry entry{.manifest = std::move(result.manifest),
                .packageName = packageName, .diagnostic = std::move(result.error)};
            if (entry.diagnostic.isEmpty()) {
                if (identifiers.contains(entry.manifest.id)) {
                    entry.diagnostic = "Duplicate mod ID; only the first package is active.";
                } else {
                    identifiers.insert(entry.manifest.id);
                    entry.enabled = m_settings.value(enabledKey(entry.manifest.id), false).toBool();
                }
            }
            entries.push_back(std::move(entry));
        }
    }
    std::ranges::sort(entries, [](const Entry& left, const Entry& right) {
        return left.packageName.compare(right.packageName, Qt::CaseInsensitive) < 0;
    });
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
    rebuildRuntime();
}

bool ModManager::grantDeclared(const QString& modId)
{
    const auto* entry = find(modId);
    if (entry == nullptr || !entry->diagnostic.isEmpty()) {
        setError("Cannot grant permissions for an unavailable mod.");
        return false;
    }
    QString error;
    if (!m_permissions.grantDeclared(entry->manifest, error)) {
        setError(std::move(error));
        return false;
    }
    const auto row = static_cast<int>(entry - m_entries.data());
    emit dataChanged(index(row), index(row), {PermissionsGrantedRole});
    rebuildRuntime();
    return true;
}

void ModManager::revokeAll(const QString& modId)
{
    if (auto* entry = find(modId); entry != nullptr) {
        entry->enabled = false;
        m_settings.setValue(enabledKey(modId), false);
        m_permissions.revokeAll(modId);
        const auto row = static_cast<int>(entry - m_entries.data());
        emit dataChanged(index(row), index(row), {EnabledRole, PermissionsGrantedRole});
        rebuildRuntime();
    }
}

bool ModManager::setEnabled(const QString& modId, const bool enabled)
{
    auto* entry = find(modId);
    if (entry == nullptr || !entry->diagnostic.isEmpty()) {
        setError("Cannot change an unavailable mod.");
        return false;
    }
    if (enabled && !m_permissions.hasAllDeclared(entry->manifest)) {
        setError("Grant the mod's declared permissions before enabling it.");
        return false;
    }
    entry->enabled = enabled;
    m_settings.setValue(enabledKey(modId), enabled);
    m_settings.sync();
    const auto row = static_cast<int>(entry - m_entries.data());
    emit dataChanged(index(row), index(row), {EnabledRole});
    rebuildRuntime();
    return true;
}

bool ModManager::activateTheme(const QString& modId)
{
    auto* entry = find(modId);
    if (entry == nullptr || !entry->manifest.hasKind(ModKind::Theme)) {
        setError("Selected mod does not provide a theme.");
        return false;
    }
    if (!m_permissions.hasAllDeclared(entry->manifest)) {
        setError("Grant the theme's declared permissions before activating it.");
        return false;
    }
    if (!setEnabled(modId, true)) {
        return false;
    }
    QString error;
    if (!m_themes.selectTheme(modId, error)) {
        setError(std::move(error));
        return false;
    }
    return true;
}

ModManager::Entry* ModManager::find(const QString& modId)
{
    const auto iterator = std::ranges::find_if(m_entries,
        [&](const Entry& entry) { return entry.manifest.id == modId; });
    return iterator == m_entries.end() ? nullptr : &*iterator;
}

const ModManager::Entry* ModManager::find(const QString& modId) const
{
    const auto iterator = std::ranges::find_if(m_entries,
        [&](const Entry& entry) { return entry.manifest.id == modId; });
    return iterator == m_entries.end() ? nullptr : &*iterator;
}

void ModManager::rebuildRuntime()
{
    m_themes.resetAvailableThemes();
    std::vector<ModManifest> enabled;
    for (auto& entry : m_entries) {
        if (!entry.diagnostic.isEmpty()) {
            continue;
        }
        if (entry.manifest.hasKind(ModKind::Theme)) {
            QString error;
            if (!m_themes.registerTheme(entry.manifest, error)) {
                entry.diagnostic = std::move(error);
                continue;
            }
        }
        if (entry.enabled && m_permissions.hasAllDeclared(entry.manifest)) {
            enabled.push_back(entry.manifest);
        }
    }
    m_extensions.rebuild(enabled);

    const auto selectedTheme = m_settings.value("mods/currentTheme", "builtin.default").toString();
    const auto* selected = find(selectedTheme);
    QString error;
    if (selected != nullptr && selected->enabled
        && m_permissions.hasAllDeclared(selected->manifest)) {
        m_themes.selectTheme(selectedTheme, error);
    } else {
        m_themes.selectTheme("builtin.default", error);
    }
    emit runtimeChanged();
}

void ModManager::setError(QString error)
{
    m_errorMessage = std::move(error);
    emit errorMessageChanged();
}

QString ModManager::enabledKey(const QString& modId) const
{
    return "mods/enabled/" + modId;
}

} // namespace yaap
