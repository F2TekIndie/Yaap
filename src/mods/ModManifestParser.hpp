#pragma once

#include "extension_api/ModManifest.hpp"

#include <QString>

namespace yaap {

struct ManifestParseResult final {
    ModManifest manifest;
    QString error;

    [[nodiscard]] bool succeeded() const noexcept { return error.isEmpty(); }
};

class ModManifestParser final {
public:
    [[nodiscard]] static ManifestParseResult parsePackage(const QString& packageRoot);

private:
    [[nodiscard]] static QString safePackageFile(
        const QString& packageRoot,
        const QString& relativePath,
        QString& error);
};

} // namespace yaap
