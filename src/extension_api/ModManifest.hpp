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

struct ThemeDefinition final {
    QString dataPath;
};

struct ModManifest final {
    int schemaVersion{};
    QString id;
    QString name;
    QString version;
    QString contentDigest;
    QString publisherId;
    QString publisherName;
    ApiVersion minimumApi;
    ApiVersion maximumExclusiveApi;
    std::vector<ModKind> kinds;
    ThemeDefinition theme;
    QString packageRoot;

    [[nodiscard]] bool hasKind(ModKind kind) const noexcept;
    [[nodiscard]] QStringList kindNames() const;
};

} // namespace yaap
