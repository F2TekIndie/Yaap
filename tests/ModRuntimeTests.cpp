#include "mods/ExtensionRegistry.hpp"
#include "mods/ModManager.hpp"
#include "mods/ModManifestParser.hpp"
#include "mods/PermissionStore.hpp"
#include "mods/ThemeManager.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

namespace yaap {
namespace {

void writeFile(const QString& path, const QByteArray& contents)
{
    QDir{}.mkpath(QFileInfo{path}.absolutePath());
    QFile file{path};
    REQUIRE(file.open(QIODevice::WriteOnly));
    REQUIRE(file.write(contents) == contents.size());
}

} // namespace

TEST_CASE("Manifest parser validates API permissions and package-contained paths")
{
    QTemporaryDir directory;
    writeFile(directory.filePath("qml/Badge.qml"), "import QtQuick\nItem {}\n");
    writeFile(directory.filePath("manifest.json"), R"json({
        "schemaVersion":1,
        "id":"org.example.sample-ui",
        "name":"Sample",
        "version":"1.0.0",
        "api":{"minimum":"1.0","maximumExclusive":"2.0"},
        "kind":["ui-extension"],
        "permissions":["ui.extend:nowPlaying.aboveTransport"],
        "uiExtensions":[{"slot":"nowPlaying.aboveTransport","component":"qml/Badge.qml"}]
    })json");

    const auto result = ModManifestParser::parsePackage(directory.path());
    INFO(result.error.toStdString());
    REQUIRE(result.succeeded());
    CHECK(result.manifest.id == "org.example.sample-ui");
    CHECK(result.manifest.uiExtensions.size() == 1);
}

TEST_CASE("Manifest parser rejects component paths outside the package")
{
    QTemporaryDir parent;
    QDir{}.mkpath(parent.filePath("package"));
    writeFile(parent.filePath("outside.qml"), "import QtQuick\nItem {}\n");
    writeFile(parent.filePath("package/manifest.json"), R"json({
        "schemaVersion":1,
        "id":"org.example.escape-ui",
        "name":"Escape",
        "version":"1.0.0",
        "api":{"minimum":"1.0","maximumExclusive":"2.0"},
        "kind":["ui-extension"],
        "permissions":["ui.extend:settings.pages"],
        "uiExtensions":[{"slot":"settings.pages","component":"../outside.qml"}]
    })json");
    CHECK_FALSE(ModManifestParser::parsePackage(parent.filePath("package")).succeeded());
}

TEST_CASE("Theme activation is atomic and extension activation requires permission")
{
    QTemporaryDir directory;
    writeFile(directory.filePath("theme.json"), R"json({
      "schemaVersion":1,
      "palette":{"windowTop":"#111111","windowBottom":"#000000","surface":"#222222",
        "primaryText":"#ffffff","secondaryText":"#bbbbbb","accent":"#00ffff","error":"#ff0000"},
      "metrics":{"cornerRadius":5,"spacing":9},
      "background":{"effect":"waves"}
    })json");
    ModManifest theme{.id = "org.example.theme", .name = "Theme", .version = "1.0",
        .contentDigest = "test-theme-digest",
        .kinds = {ModKind::Theme}, .permissions = {"theme.install"},
        .theme = {.dataPath = directory.filePath("theme.json")}};
    ThemeManager themes;
    QString error;
    REQUIRE(themes.registerTheme(theme, error));
    REQUIRE(themes.selectTheme(theme.id, error));
    CHECK(themes.accent() == QColor{"#00ffff"});
    CHECK(themes.backgroundEffect() == "waves");

    PermissionStore permissions;
    ExtensionRegistry extensions{permissions};
    ModManifest ui{.id = "org.example.ui", .name = "UI", .version = "1.0",
        .contentDigest = "test-ui-digest",
        .kinds = {ModKind::UiExtension},
        .permissions = {"ui.extend:nowPlaying.aboveTransport"},
        .uiExtensions = {{"nowPlaying.aboveTransport", directory.filePath("Badge.qml"), 0}}};
    extensions.rebuild({ui});
    CHECK(extensions.rowCount() == 0);
    REQUIRE(permissions.grantDeclared(ui, error));
    extensions.rebuild({ui});
    CHECK(extensions.rowCount() == 1);
}

TEST_CASE("Every bundled sample theme is a valid selectable package")
{
    const QDir sampleMods{QString::fromUtf8(YAAP_SAMPLE_MODS_PATH)};
    const auto themePackages = sampleMods.entryList(
        {"*-theme"}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    REQUIRE(themePackages.size() >= 4);

    ThemeManager themes;
    for (const auto& package : themePackages) {
        CAPTURE(package.toStdString());
        const auto parsed = ModManifestParser::parsePackage(sampleMods.filePath(package));
        INFO(parsed.error.toStdString());
        REQUIRE(parsed.succeeded());
        REQUIRE(parsed.manifest.hasKind(ModKind::Theme));

        QString error;
        REQUIRE(themes.registerTheme(parsed.manifest, error));
        INFO(error.toStdString());
        REQUIRE(themes.selectTheme(parsed.manifest.id, error));
        CHECK(themes.currentThemeId() == parsed.manifest.id);
        const auto expectedEffect = package == "org.yaap.ocean-theme"
            ? QString{"waves"}
            : package == "org.yaap.synthwave-theme" ? QString{"spectrum"} : QString{"none"};
        CHECK(themes.backgroundEffect() == expectedEffect);
        if (package == "org.yaap.synthwave-theme") {
            CHECK(themes.spectrumColumns() == 48);
            CHECK(themes.spectrumMirror());
            CHECK(themes.spectrumOpacity() == 0.32);
            CHECK(themes.spectrumAttackMilliseconds() == 45);
            CHECK(themes.spectrumReleaseMilliseconds() == 260);
            CHECK(themes.spectrumGradientStart() == QColor{"#ff2bd6"});
            CHECK(themes.spectrumGradientMiddle() == QColor{"#9b5de5"});
            CHECK(themes.spectrumGradientEnd() == QColor{"#35f2d0"});
        }
    }
}

TEST_CASE("Theme background effects are restricted to host-owned renderers")
{
    QTemporaryDir directory;
    writeFile(directory.filePath("theme.json"), R"json({
      "schemaVersion":1,
      "palette":{"windowTop":"#111111","windowBottom":"#000000","surface":"#222222",
        "primaryText":"#ffffff","secondaryText":"#bbbbbb","accent":"#00ffff","error":"#ff0000"},
      "metrics":{"cornerRadius":5,"spacing":9},
      "background":{"effect":"arbitrary-qml"}
    })json");
    ModManifest theme{.id = "org.example.invalid-effect", .name = "Invalid", .version = "1.0",
        .contentDigest = "invalid-effect-digest",
        .kinds = {ModKind::Theme}, .permissions = {"theme.install"},
        .theme = {.dataPath = directory.filePath("theme.json")}};

    ThemeManager themes;
    QString error;
    CHECK_FALSE(themes.registerTheme(theme, error));
    CHECK(error.contains("not supported"));
}

TEST_CASE("Spectrum theme parameters are strictly bounded")
{
    QTemporaryDir directory;
    writeFile(directory.filePath("theme.json"), R"json({
      "schemaVersion":1,
      "palette":{"windowTop":"#111111","windowBottom":"#000000","surface":"#222222",
        "primaryText":"#ffffff","secondaryText":"#bbbbbb","accent":"#00ffff","error":"#ff0000"},
      "metrics":{"cornerRadius":5,"spacing":9},
      "background":{"effect":"spectrum","parameters":{"columns":200,"opacity":2.0}}
    })json");
    ModManifest theme{.id = "org.example.invalid-spectrum", .name = "Invalid spectrum",
        .version = "1.0", .contentDigest = "invalid-spectrum-digest",
        .kinds = {ModKind::Theme}, .permissions = {"theme.install"},
        .theme = {.dataPath = directory.filePath("theme.json")}};

    ThemeManager themes;
    QString error;
    CHECK_FALSE(themes.registerTheme(theme, error));
    CHECK(error.contains("outside supported bounds"));
}

TEST_CASE("Mod manager can grant enable and select a bundled theme")
{
    PermissionStore permissions;
    ThemeManager themes;
    ExtensionRegistry extensions{permissions};
    ModManager mods{permissions, themes, extensions,
        {QString::fromUtf8(YAAP_SAMPLE_MODS_PATH)}};

    constexpr auto themeId = "org.yaap.paper-theme";
    REQUIRE(mods.grantDeclared(themeId));
    REQUIRE(mods.activateTheme(themeId));
    CHECK(themes.currentThemeId() == themeId);
    CHECK(themes.windowTop() == QColor{"#f8f1e4"});
}

TEST_CASE("Changing package content invalidates its permission grant")
{
    QTemporaryDir directory;
    writeFile(directory.filePath("theme.json"), R"json({
      "schemaVersion":1,
      "palette":{"windowTop":"#111111","windowBottom":"#000000","surface":"#222222",
        "primaryText":"#ffffff","secondaryText":"#bbbbbb","accent":"#00ffff","error":"#ff0000"},
      "metrics":{"cornerRadius":5,"spacing":9}
    })json");
    writeFile(directory.filePath("manifest.json"), R"json({
      "schemaVersion":1,"id":"org.example.digest-theme","name":"Digest","version":"1.0.0",
      "api":{"minimum":"1.0","maximumExclusive":"2.0"},"kind":["theme"],
      "permissions":["theme.install"],"theme":{"data":"theme.json"}
    })json");
    const auto original = ModManifestParser::parsePackage(directory.path());
    REQUIRE(original.succeeded());
    PermissionStore permissions;
    QString error;
    REQUIRE(permissions.grantDeclared(original.manifest, error));
    REQUIRE(permissions.hasAllDeclared(original.manifest));

    writeFile(directory.filePath("theme.json"), R"json({
      "schemaVersion":1,
      "palette":{"windowTop":"#222222","windowBottom":"#000000","surface":"#222222",
        "primaryText":"#ffffff","secondaryText":"#bbbbbb","accent":"#00ffff","error":"#ff0000"},
      "metrics":{"cornerRadius":5,"spacing":9}
    })json");
    const auto changed = ModManifestParser::parsePackage(directory.path());
    REQUIRE(changed.succeeded());
    CHECK(changed.manifest.contentDigest != original.manifest.contentDigest);
    CHECK_FALSE(permissions.hasAllDeclared(changed.manifest));
}

} // namespace yaap
