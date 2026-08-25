#pragma once

#include "extension_api/ApiVersion.hpp"

#include <QString>
#include <QStringList>

#include <vector>

namespace yaap {

enum class ModKind {
    Theme,
    UiExtension,
    Provider,
};

struct UiExtensionDefinition final {
    QString slotId;
    QString componentPath;
    int order{};
};

struct ThemeDefinition final {
    QString dataPath;
};

struct ProviderDefinition final {
    QString executablePath;
    QString providerId;
};

struct ModManifest final {
    int schemaVersion{};
    QString id;
    QString name;
    QString version;
    ApiVersion minimumApi;
    ApiVersion maximumExclusiveApi;
    std::vector<ModKind> kinds;
    QStringList permissions;
    std::vector<UiExtensionDefinition> uiExtensions;
    ThemeDefinition theme;
    ProviderDefinition provider;
    QString packageRoot;

    [[nodiscard]] bool hasKind(ModKind kind) const noexcept;
    [[nodiscard]] QStringList kindNames() const;
};

} // namespace yaap
