#pragma once

#include "extension_api/ModManifest.hpp"

#include <QObject>
#include <QSettings>
#include <QStringList>

namespace yaap {

class PermissionStore final : public QObject {
    Q_OBJECT

public:
    explicit PermissionStore(QObject* parent = nullptr);

    [[nodiscard]] bool isGranted(const QString& modId, const QString& permission) const;
    [[nodiscard]] bool hasAllDeclared(const ModManifest& manifest) const;
    [[nodiscard]] QStringList granted(const QString& modId) const;
    bool grantDeclared(const ModManifest& manifest, QString& error);
    void revokeAll(const QString& modId);

signals:
    void permissionsChanged(const QString& modId);

private:
    [[nodiscard]] QString settingsKey(const QString& modId) const;

    mutable QSettings m_settings;
};

} // namespace yaap
