#pragma once

#include "extension_api/ModManifest.hpp"

#include <QAbstractListModel>
#include <QSettings>
#include <QStringList>

#include <vector>

namespace yaap {

class ExtensionRegistry;
class PermissionStore;
class ThemeManager;

class ModManager final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        NameRole,
        VersionRole,
        KindsRole,
        PermissionsRole,
        EnabledRole,
        PermissionsGrantedRole,
        DiagnosticRole,
        IsThemeRole,
        IsUiExtensionRole,
        IsProviderRole,
        ContentDigestRole,
        PublisherRole,
    };
    Q_ENUM(Role)

    ModManager(
        PermissionStore& permissions,
        ThemeManager& themes,
        ExtensionRegistry& extensions,
        QStringList searchRoots,
        QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] QString errorMessage() const;
    [[nodiscard]] std::vector<ModManifest> enabledProviders() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool grantDeclared(const QString& modId);
    Q_INVOKABLE void revokeAll(const QString& modId);
    Q_INVOKABLE bool setEnabled(const QString& modId, bool enabled);
    Q_INVOKABLE bool activateTheme(const QString& modId);

signals:
    void errorMessageChanged();
    void runtimeChanged();

private:
    struct Entry final {
        ModManifest manifest;
        QString packageName;
        QString diagnostic;
        bool enabled{};
    };

    [[nodiscard]] Entry* find(const QString& modId);
    [[nodiscard]] const Entry* find(const QString& modId) const;
    void rebuildRuntime();
    void setError(QString error);
    [[nodiscard]] QString enabledKey(const QString& modId) const;

    PermissionStore& m_permissions;
    ThemeManager& m_themes;
    ExtensionRegistry& m_extensions;
    QStringList m_searchRoots;
    std::vector<Entry> m_entries;
    QString m_errorMessage;
    QSettings m_settings;
};

} // namespace yaap
