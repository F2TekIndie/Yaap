#include "mods/ModManifestParser.hpp"


#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
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
constexpr qint64 maximumPackageBytes = 128 * 1024 * 1024;
constexpr qsizetype maximumPackageFiles = 4'096;

[[nodiscard]] QString packageDigest(const QString& packageRoot, QString& error)
{
    QStringList relativeFiles;
    QDirIterator iterator{packageRoot, QDir::Files | QDir::Hidden,
        QDirIterator::Subdirectories};
    while (iterator.hasNext()) {
        relativeFiles.push_back(QDir::fromNativeSeparators(
            QDir{packageRoot}.relativeFilePath(iterator.next())));
        if (relativeFiles.size() > maximumPackageFiles) {
            error = "Mod package contains too many files.";
            return {};
        }
    }
    relativeFiles.sort(Qt::CaseSensitive);

    const auto canonicalRoot = QDir::fromNativeSeparators(
        QFileInfo{packageRoot}.canonicalFilePath());
    const auto rootPrefix = canonicalRoot + '/';
    QCryptographicHash hash{QCryptographicHash::Sha256};
    qint64 totalBytes{};
    for (const auto& relativePath : relativeFiles) {
        QFile file{QDir{packageRoot}.filePath(relativePath)};
        const auto canonicalFile = QDir::fromNativeSeparators(
            QFileInfo{file}.canonicalFilePath());
#ifdef _WIN32
        constexpr auto pathCaseSensitivity = Qt::CaseInsensitive;
#else
        constexpr auto pathCaseSensitivity = Qt::CaseSensitive;
#endif
        if (!canonicalFile.startsWith(rootPrefix, pathCaseSensitivity)
            || !file.open(QIODevice::ReadOnly)) {
            error = "Could not safely hash package file " + relativePath + '.';
            return {};
        }
        totalBytes += file.size();
        if (totalBytes > maximumPackageBytes) {
            error = "Mod package exceeds the 128 MiB trust-verification limit.";
            return {};
        }
        hash.addData(relativePath.toUtf8());
        hash.addData(QByteArrayView{"\0", 1});
        hash.addData(QByteArray::number(file.size()));
        hash.addData(QByteArrayView{"\0", 1});
        while (!file.atEnd()) {
            hash.addData(file.read(64 * 1024));
        }
    }
    return QString::fromLatin1(hash.result().toHex());
}

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
    const auto publisher = root.value("publisher").toObject();
    manifest.publisherId = publisher.value("id").toString().trimmed();
    manifest.publisherName = publisher.value("name").toString().trimmed();

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
        if (kind == ModKind::UiExtension) {
            return {.error = "UI extensions are no longer supported."};
        }
        if (kind == ModKind::Provider) {
            return {.error = "Custom providers are no longer supported."};
        }
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

    if (manifest.hasKind(ModKind::Theme)) {
        const auto relativePath = root.value("theme").toObject().value("data").toString("theme.json");
        QString error;
        manifest.theme.dataPath = safePackageFile(canonicalRoot, relativePath, error);
        if (!error.isEmpty()) {
            return {.error = "Invalid theme file: " + error};
        }
    }

    QString digestError;
    manifest.contentDigest = packageDigest(canonicalRoot, digestError);
    if (!digestError.isEmpty()) {
        return {.error = std::move(digestError)};
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
