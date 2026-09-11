#pragma once

#include "extension_api/ModManifest.hpp"

#include <QAbstractListModel>
#include <QSettings>
#include <QStringList>

#include <vector>

namespace yaap {

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
        DiagnosticRole,
        IsThemeRole,
        IsUiExtensionRole,
        IsProviderRole,
        ContentDigestRole,
        PublisherRole,
    };
    Q_ENUM(Role)

    ModManager(
        ThemeManager& themes,
        QStringList searchRoots,
        QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] QString errorMessage() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool activateTheme(const QString& modId);

signals:
    void errorMessageChanged();
    void runtimeChanged();

private:
    struct Entry final {
        ModManifest manifest;
        QString packageName;
        QString diagnostic;
    };

    [[nodiscard]] Entry* find(const QString& modId);
    [[nodiscard]] const Entry* find(const QString& modId) const;
    void rebuildRuntime();
    void setError(QString error);

    ThemeManager& m_themes;
    QStringList m_searchRoots;
    std::vector<Entry> m_entries;
    QString m_errorMessage;
    QSettings m_settings;
};

} // namespace yaap
