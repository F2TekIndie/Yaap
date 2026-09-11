#include "mods/ModManager.hpp"

#include "mods/ModManifestParser.hpp"
#include "mods/ThemeManager.hpp"

#include <QDir>
#include <QFileInfo>
#include <QSet>

#include <algorithm>
#include <utility>

namespace yaap {

ModManager::ModManager(
    ThemeManager& themes,
    QStringList searchRoots,
    QObject* parent)
    : QAbstractListModel(parent)
    , m_themes(themes)
    , m_searchRoots(std::move(searchRoots))
{
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
    case DiagnosticRole: return entry.diagnostic;
    case IsThemeRole: return entry.manifest.hasKind(ModKind::Theme);
    case IsUiExtensionRole: return entry.manifest.hasKind(ModKind::UiExtension);
    case IsProviderRole: return entry.manifest.hasKind(ModKind::Provider);
    case ContentDigestRole: return entry.manifest.contentDigest;
    case PublisherRole: return entry.manifest.publisherName.isEmpty()
        ? QStringLiteral("Unverified publisher") : entry.manifest.publisherName;
    default: return {};
    }
}

QHash<int, QByteArray> ModManager::roleNames() const
{
    return {{IdRole, "modId"}, {NameRole, "name"}, {VersionRole, "version"},
        {KindsRole, "kinds"},
        {DiagnosticRole, "diagnostic"}, {IsThemeRole, "isTheme"},
        {IsUiExtensionRole, "isUiExtension"}, {IsProviderRole, "isProvider"},
        {ContentDigestRole, "contentDigest"}, {PublisherRole, "publisher"}};
}

QString ModManager::errorMessage() const
{
    return m_errorMessage;
}

void ModManager::refresh()
{
    // PROTOTYPE: Mod discovery is deliberately startup/manual only. Atomic
    // package installation must precede folder watching.
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

bool ModManager::activateTheme(const QString& modId)
{
    auto* entry = find(modId);
    if (entry == nullptr || !entry->diagnostic.isEmpty() || !entry->manifest.hasKind(ModKind::Theme)) {
        setError("Selected mod does not provide a theme.");
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
    }

    const auto selectedTheme = m_settings.value("mods/currentTheme", "builtin.default").toString();
    const auto* selected = find(selectedTheme);
    QString error;
    if (selectedTheme == "builtin.custom" || selectedTheme == "builtin.dms"
        || (selected != nullptr && selected->diagnostic.isEmpty())) {
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

} // namespace yaap
