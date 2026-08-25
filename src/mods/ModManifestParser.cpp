#include "mods/ModManifestParser.hpp"

#include "extension_api/ModPermission.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSet>

namespace yaap {
namespace {

constexpr qint64 maximumManifestBytes = 256 * 1024;

[[nodiscard]] std::optional<ModKind> parseKind(const QString& value)
{
    if (value == "theme") {
        return ModKind::Theme;
    }
    if (value == "ui-extension") {
        return ModKind::UiExtension;
    }
    if (value == "provider") {
        return ModKind::Provider;
    }
    return std::nullopt;
}

[[nodiscard]] QString platformExecutable(const QJsonObject& executable)
{
#ifdef _WIN32
    constexpr auto platform = "windows-x64";
#elif defined(__APPLE__)
#if defined(__aarch64__) || defined(__arm64__)
    constexpr auto platform = "macos-arm64";
#else
    constexpr auto platform = "macos-x64";
#endif
#else
    constexpr auto platform = "linux-x64";
#endif
    return executable.value(platform).toString();
}

} // namespace

ManifestParseResult ModManifestParser::parsePackage(const QString& packageRoot)
{
    const QFileInfo rootInfo{packageRoot};
    const auto canonicalRoot = rootInfo.canonicalFilePath();
    if (canonicalRoot.isEmpty() || !rootInfo.isDir()) {
        return {.error = "Mod package directory does not exist."};
    }

    QFile file{QDir{canonicalRoot}.filePath("manifest.json")};
    if (!file.open(QIODevice::ReadOnly)) {
        return {.error = "Could not open manifest.json: " + file.errorString()};
    }
    if (file.size() <= 0 || file.size() > maximumManifestBytes) {
        return {.error = "manifest.json must be between 1 byte and 256 KiB."};
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return {.error = "Invalid manifest JSON: " + parseError.errorString()};
    }
    const auto root = document.object();
    ModManifest manifest;
    manifest.packageRoot = canonicalRoot;
    manifest.schemaVersion = root.value("schemaVersion").toInt(-1);
    manifest.id = root.value("id").toString().trimmed();
    manifest.name = root.value("name").toString().trimmed();
    manifest.version = root.value("version").toString().trimmed();

    if (manifest.schemaVersion != modManifestSchemaVersion) {
        return {.error = "Unsupported manifest schema version."};
    }
    static const QRegularExpression idPattern{
        R"(^[a-z][a-z0-9]*(?:\.[a-z][a-z0-9_-]*){2,}$)"};
    if (!idPattern.match(manifest.id).hasMatch()) {
        return {.error = "Mod ID must be a lowercase reverse-domain identifier."};
    }
    if (manifest.name.isEmpty() || manifest.version.isEmpty()) {
        return {.error = "Mod name and version are required."};
    }

    const auto api = root.value("api").toObject();
    const auto minimum = ApiVersion::parse(api.value("minimum").toString());
    const auto maximum = ApiVersion::parse(api.value("maximumExclusive").toString());
    if (!minimum || !maximum || *maximum <= *minimum) {
        return {.error = "Manifest API range is invalid."};
    }
    if (extensionApiVersion < *minimum || extensionApiVersion >= *maximum) {
        return {.error = "Mod is incompatible with Yaap extension API "
            + extensionApiVersion.toString() + '.'};
    }
    manifest.minimumApi = *minimum;
    manifest.maximumExclusiveApi = *maximum;

    QSet<int> uniqueKinds;
    for (const auto& value : root.value("kind").toArray()) {
        const auto kind = parseKind(value.toString());
        if (!kind) {
            return {.error = "Manifest contains an unknown mod kind."};
        }
        if (!uniqueKinds.contains(static_cast<int>(*kind))) {
            uniqueKinds.insert(static_cast<int>(*kind));
            manifest.kinds.push_back(*kind);
        }
    }
    if (manifest.kinds.empty()) {
        return {.error = "Manifest must declare at least one mod kind."};
    }

    for (const auto& value : root.value("permissions").toArray()) {
        const auto requested = value.toString().trimmed();
        if (!permission::isKnown(requested)) {
            return {.error = "Unknown permission: " + requested};
        }
        if (!manifest.permissions.contains(requested)) {
            manifest.permissions.push_back(requested);
        }
    }

    if (manifest.hasKind(ModKind::Theme)) {
        const auto relativePath = root.value("theme").toObject().value("data").toString("theme.json");
        QString error;
        manifest.theme.dataPath = safePackageFile(canonicalRoot, relativePath, error);
        if (!error.isEmpty()) {
            return {.error = "Invalid theme file: " + error};
        }
    }

    if (manifest.hasKind(ModKind::UiExtension)) {
        for (const auto& value : root.value("uiExtensions").toArray()) {
            const auto extension = value.toObject();
            UiExtensionDefinition definition{
                .slotId = extension.value("slot").toString().trimmed(),
                .order = extension.value("order").toInt()};
            QString error;
            definition.componentPath = safePackageFile(
                canonicalRoot, extension.value("component").toString(), error);
            if (definition.slotId.isEmpty() || !error.isEmpty()) {
                return {.error = "Invalid UI extension declaration: " + error};
            }
            const auto slotPermission = permission::uiSlot(definition.slotId);
            if (!manifest.permissions.contains(slotPermission)) {
                return {.error = "UI extension is missing permission " + slotPermission};
            }
            manifest.uiExtensions.push_back(std::move(definition));
        }
        if (manifest.uiExtensions.empty()) {
            return {.error = "UI extension mod declares no components."};
        }
    }

    if (manifest.hasKind(ModKind::Provider)) {
        const auto provider = root.value("provider").toObject();
        manifest.provider.providerId = provider.value("id").toString().trimmed();
        const auto relativeExecutable = platformExecutable(provider.value("executables").toObject());
        QString error;
        manifest.provider.executablePath = safePackageFile(
            canonicalRoot, relativeExecutable, error);
        if (manifest.provider.providerId.isEmpty() || !error.isEmpty()) {
            return {.error = "Invalid provider declaration: " + error};
        }
    }

    return {.manifest = std::move(manifest)};
}

QString ModManifestParser::safePackageFile(
    const QString& packageRoot,
    const QString& relativePath,
    QString& error)
{
    if (relativePath.trimmed().isEmpty() || QDir::isAbsolutePath(relativePath)) {
        error = "Package paths must be non-empty relative paths.";
        return {};
    }
    const auto canonicalRoot = QFileInfo{packageRoot}.canonicalFilePath();
    const auto candidate = QFileInfo{QDir{canonicalRoot}.filePath(relativePath)};
    const auto canonicalCandidate = candidate.canonicalFilePath();
    const auto prefix = QDir::cleanPath(canonicalRoot) + '/';
#ifdef _WIN32
    constexpr auto pathCaseSensitivity = Qt::CaseInsensitive;
#else
    constexpr auto pathCaseSensitivity = Qt::CaseSensitive;
#endif
    if (canonicalCandidate.isEmpty() || !candidate.isFile()
        || (!QDir::cleanPath(canonicalCandidate).startsWith(prefix, pathCaseSensitivity)
            && canonicalCandidate.compare(canonicalRoot, pathCaseSensitivity) != 0)) {
        error = "Path escapes the package or does not name a file: " + relativePath;
        return {};
    }
    return canonicalCandidate;
}

} // namespace yaap
