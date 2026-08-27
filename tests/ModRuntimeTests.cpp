#include "mods/ExtensionRegistry.hpp"
#include "mods/ModManager.hpp"
#include "mods/ModManifestParser.hpp"
#include "mods/PermissionStore.hpp"
#include "mods/ThemeManager.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
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

QPoint firstTransparentPixel(const QImage& image, const QRect& rectangle)
{
    constexpr int alphaThreshold = 8;
    if (rectangle.isEmpty() || !image.rect().contains(rectangle)) {
        return rectangle.topLeft();
    }
    for (int y = rectangle.top(); y <= rectangle.bottom(); ++y) {
        for (int x = rectangle.left(); x <= rectangle.right(); ++x) {
            if (qAlpha(image.pixel(x, y)) < alphaThreshold) {
                return {x, y};
            }
        }
    }
    return {-1, -1};
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
            : package == "org.yaap.synthwave-theme"
                ? QString{"spectrum"}
                : package == "org.yaap.paper-theme" ? QString{"paperPlanes"}
                                                      : QString{"none"};
        CHECK(themes.backgroundEffect() == expectedEffect);
        if (package == "org.yaap.paper-theme") {
            CHECK(themes.backgroundImageSource().fileName() == "paper-plane-skin.svg");
            CHECK(themes.backgroundImageFit() == "stretch");
            CHECK(themes.backgroundImageAlignment() == "center");
            CHECK(themes.backgroundImageOpacity() == 0.9);
            CHECK(themes.backgroundImageShapesWindow());
            CHECK(themes.controlAreaLeftInset() == 64);
            CHECK(themes.controlAreaTopInset() == 84);
            CHECK(themes.controlAreaRightInset() == 216);
            CHECK(themes.controlAreaBottomInset() == 84);
            CHECK(themes.closeButtonRightInset() == 170);
            CHECK(themes.closeButtonTopInset() == 48);
            CHECK(themes.closeButtonWidth() == 44);
            CHECK(themes.closeButtonHeight() == 36);
            CHECK(themes.miniPlayerWidth() == 480);
            CHECK(themes.miniPlayerHeight() == 112);
            CHECK(themes.miniBackgroundImageSource().fileName()
                == "miniplayer-paper-plane.svg");
            CHECK(themes.miniBackgroundImageShapesWindow());
        }
        if (package == "org.yaap.synthwave-theme") {
            CHECK(themes.backgroundImageSource().fileName() == "neon-horizon.svg");
            CHECK(themes.backgroundImageFit() == "stretch");
            CHECK(themes.backgroundImageAlignment() == "center");
            CHECK(themes.backgroundImageOpacity() == 0.68);
            CHECK(themes.backgroundImageShapesWindow());
            CHECK(themes.controlAreaLeftInset() == 90);
            CHECK(themes.controlAreaTopInset() == 50);
            CHECK(themes.controlAreaRightInset() == 90);
            CHECK(themes.controlAreaBottomInset() == 50);
            CHECK(themes.closeButtonRightInset() == 42);
            CHECK(themes.closeButtonTopInset() == 30);
            CHECK(themes.miniPlayerWidth() == 480);
            CHECK(themes.miniPlayerHeight() == 112);
            CHECK(themes.miniBackgroundImageSource().fileName()
                == "miniplayer-neon.svg");
            CHECK(themes.miniBackgroundImageShapesWindow());
            CHECK(themes.spectrumColumns() == 48);
            CHECK(themes.spectrumMirror());
            CHECK(themes.spectrumOpacity() == 0.32);
            CHECK(themes.spectrumAttackMilliseconds() == 45);
            CHECK(themes.spectrumReleaseMilliseconds() == 260);
            CHECK(themes.spectrumGradientStart() == QColor{"#ff2bd6"});
            CHECK(themes.spectrumGradientMiddle() == QColor{"#9b5de5"});
            CHECK(themes.spectrumGradientEnd() == QColor{"#35f2d0"});
            CHECK(themes.spectrumHueShiftAdjustable());
            CHECK(themes.spectrumHueShiftDegrees() == 0.0);
            themes.setSpectrumHueShiftDegrees(120.0);
            CHECK(themes.spectrumHueShiftDegrees() == 120.0);
            CHECK(themes.spectrumGradientStart() != QColor{"#ff2bd6"});
            themes.setSpectrumHueShiftDegrees(999.0);
            CHECK(themes.spectrumHueShiftDegrees() == 180.0);

            ThemeManager restoredThemes;
            REQUIRE(restoredThemes.registerTheme(parsed.manifest, error));
            REQUIRE(restoredThemes.selectTheme(parsed.manifest.id, error));
            CHECK(restoredThemes.spectrumHueShiftDegrees() == 180.0);
            restoredThemes.setSpectrumHueShiftDegrees(0.0);
        }
    }
}

TEST_CASE("Theme control layout metadata is strictly bounded")
{
    QTemporaryDir directory;
    writeFile(directory.filePath("theme.json"), R"json({
      "schemaVersion":1,
      "palette":{"windowTop":"#111111","windowBottom":"#000000","surface":"#222222",
        "primaryText":"#ffffff","secondaryText":"#bbbbbb","accent":"#00ffff","error":"#ff0000"},
      "metrics":{"cornerRadius":5,"spacing":9},
      "layout":{
        "controlArea":{"leftInset":100,"rightInset":100},
        "closeButton":{"rightInset":12,"topInset":4.5,"width":44,"height":36}
      }
    })json");
    ModManifest theme{.id = "org.example.invalid-layout", .name = "Invalid layout",
        .version = "1.0", .contentDigest = "invalid-layout-digest",
        .kinds = {ModKind::Theme}, .permissions = {"theme.install"},
        .theme = {.dataPath = directory.filePath("theme.json")}};

    ThemeManager themes;
    QString error;
    CHECK_FALSE(themes.registerTheme(theme, error));
    CHECK((error.contains("layout") || error.contains("usable window space")));
}

TEST_CASE("Shaped sample themes keep controls inside their opaque window region")
{
    const QDir sampleMods{QString::fromUtf8(YAAP_SAMPLE_MODS_PATH)};
    for (const auto& package : {QString{"org.yaap.paper-theme"},
             QString{"org.yaap.synthwave-theme"}}) {
        CAPTURE(package.toStdString());
        const auto parsed = ModManifestParser::parsePackage(sampleMods.filePath(package));
        REQUIRE(parsed.succeeded());

        ThemeManager themes;
        QString error;
        REQUIRE(themes.registerTheme(parsed.manifest, error));
        REQUIRE(themes.selectTheme(parsed.manifest.id, error));
        REQUIRE(themes.backgroundImageShapesWindow());
        REQUIRE(themes.backgroundImageFit() == "stretch");

        for (const auto& size : {QSize{900, 560}, QSize{680, 420}}) {
            CAPTURE(size.width(), size.height());
            QImageReader reader{themes.backgroundImageSource().toLocalFile()};
            reader.setScaledSize(size);
            const auto image = reader.read().convertToFormat(QImage::Format_ARGB32);
            REQUIRE_FALSE(image.isNull());

            const QRect controlArea{themes.controlAreaLeftInset(),
                themes.controlAreaTopInset(),
                size.width() - themes.controlAreaLeftInset()
                    - themes.controlAreaRightInset(),
                size.height() - themes.controlAreaTopInset()
                    - themes.controlAreaBottomInset()};
            const QRect closeButton{size.width() - themes.closeButtonRightInset()
                    - themes.closeButtonWidth(),
                themes.closeButtonTopInset(), themes.closeButtonWidth(),
                themes.closeButtonHeight()};
            const auto transparentControlPixel = firstTransparentPixel(image, controlArea);
            CAPTURE(transparentControlPixel.x(), transparentControlPixel.y());
            CHECK((transparentControlPixel == QPoint{-1, -1}));
            const auto transparentClosePixel = firstTransparentPixel(image, closeButton);
            CAPTURE(transparentClosePixel.x(), transparentClosePixel.y());
            CHECK((transparentClosePixel == QPoint{-1, -1}));
        }

        REQUIRE(themes.miniBackgroundImageShapesWindow());
        const QSize miniSize{themes.miniPlayerWidth(), themes.miniPlayerHeight()};
        QImageReader miniReader{themes.miniBackgroundImageSource().toLocalFile()};
        miniReader.setScaledSize(miniSize);
        const auto miniImage = miniReader.read().convertToFormat(
            QImage::Format_ARGB32);
        REQUIRE_FALSE(miniImage.isNull());
        const QRect miniControlArea{themes.miniControlAreaLeftInset(),
            themes.miniControlAreaTopInset(),
            miniSize.width() - themes.miniControlAreaLeftInset()
                - themes.miniControlAreaRightInset(),
            miniSize.height() - themes.miniControlAreaTopInset()
                - themes.miniControlAreaBottomInset()};
        const auto controlsWidth = themes.miniWindowControlWidth() * 2
            + themes.miniWindowControlSpacing();
        const QRect miniWindowControls{
            miniSize.width() - themes.miniWindowControlsRightInset()
                - controlsWidth,
            themes.miniWindowControlsTopInset(), controlsWidth,
            themes.miniWindowControlHeight()};
        CHECK((firstTransparentPixel(miniImage, miniControlArea)
            == QPoint{-1, -1}));
        CHECK((firstTransparentPixel(miniImage, miniWindowControls)
            == QPoint{-1, -1}));
    }
}

TEST_CASE("Miniplayer theme effects are forbidden")
{
    QTemporaryDir directory;
    writeFile(directory.filePath("theme.json"), R"json({
      "schemaVersion":1,
      "palette":{"windowTop":"#111111","windowBottom":"#000000","surface":"#222222",
        "primaryText":"#ffffff","secondaryText":"#bbbbbb","accent":"#00ffff","error":"#ff0000"},
      "metrics":{"cornerRadius":5,"spacing":9},
      "miniPlayer":{
        "size":{"width":480,"height":112},
        "layout":{"controlArea":{"leftInset":10,"rightInset":100}},
        "background":{"effect":"spectrum"}
      }
    })json");
    ModManifest theme{.id = "org.example.invalid-mini", .name = "Invalid mini",
        .version = "1.0", .contentDigest = "invalid-mini-digest",
        .kinds = {ModKind::Theme}, .permissions = {"theme.install"},
        .theme = {.dataPath = directory.filePath("theme.json")}};

    ThemeManager themes;
    QString error;
    CHECK_FALSE(themes.registerTheme(theme, error));
    CHECK(error.contains("effects are not supported"));
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

TEST_CASE("Theme background image composes underneath one host-owned effect")
{
    QTemporaryDir directory;
    writeFile(directory.filePath("assets/shape.svg"), R"svg(
      <svg xmlns="http://www.w3.org/2000/svg" width="120" height="48"
           viewBox="0 0 120 48">
        <path d="M4 44 L60 4 L116 44 Z" fill="#35f2d0"/>
      </svg>
    )svg");
    writeFile(directory.filePath("theme.json"), R"json({
      "schemaVersion":1,
      "palette":{"windowTop":"#111111","windowBottom":"#000000","surface":"#222222",
        "primaryText":"#ffffff","secondaryText":"#bbbbbb","accent":"#00ffff","error":"#ff0000"},
      "metrics":{"cornerRadius":5,"spacing":9},
      "background":{
        "image":{"asset":"assets/shape.svg","fit":"preserveAspectCrop",
          "alignment":"bottom-right","opacity":0.65},
        "effect":"waves"
      }
    })json");
    ModManifest theme{.id = "org.example.image-theme", .name = "Image theme",
        .version = "1.0", .contentDigest = "image-theme-digest",
        .kinds = {ModKind::Theme}, .permissions = {"theme.install"},
        .theme = {.dataPath = directory.filePath("theme.json")},
        .packageRoot = directory.path()};

    ThemeManager themes;
    QString error;
    const auto registered = themes.registerTheme(theme, error);
    INFO(error.toStdString());
    REQUIRE(registered);
    REQUIRE(themes.selectTheme(theme.id, error));
    CHECK(themes.backgroundImageSource()
        == QUrl::fromLocalFile(QFileInfo{directory.filePath("assets/shape.svg")}
                .canonicalFilePath()));
    CHECK(themes.backgroundImageFit() == "preserveAspectCrop");
    CHECK(themes.backgroundImageAlignment() == "bottom-right");
    CHECK(themes.backgroundImageOpacity() == 0.65);
    CHECK(themes.backgroundEffect() == "waves");
}

TEST_CASE("Theme background images cannot escape their package")
{
    QTemporaryDir directory;
    QDir{}.mkpath(directory.filePath("package"));
    writeFile(directory.filePath("outside.svg"), R"svg(
      <svg xmlns="http://www.w3.org/2000/svg" width="8" height="8"/>
    )svg");
    writeFile(directory.filePath("package/theme.json"), R"json({
      "schemaVersion":1,
      "palette":{"windowTop":"#111111","windowBottom":"#000000","surface":"#222222",
        "primaryText":"#ffffff","secondaryText":"#bbbbbb","accent":"#00ffff","error":"#ff0000"},
      "metrics":{"cornerRadius":5,"spacing":9},
      "background":{"image":{"asset":"../outside.svg"},"effect":"spectrum"}
    })json");
    ModManifest theme{.id = "org.example.escape-image", .name = "Escape image",
        .version = "1.0", .contentDigest = "escape-image-digest",
        .kinds = {ModKind::Theme}, .permissions = {"theme.install"},
        .theme = {.dataPath = directory.filePath("package/theme.json")},
        .packageRoot = directory.filePath("package")};

    ThemeManager themes;
    QString error;
    CHECK_FALSE(themes.registerTheme(theme, error));
    CHECK(error.contains("escapes the package"));
}

TEST_CASE("Theme background image accepts bounded transparent PNG files")
{
    QTemporaryDir directory;
    QImage image{7, 5, QImage::Format_ARGB32_Premultiplied};
    image.fill(Qt::transparent);
    REQUIRE(image.save(directory.filePath("shape.png"), "PNG"));
    writeFile(directory.filePath("theme.json"), R"json({
      "schemaVersion":1,
      "palette":{"windowTop":"#111111","windowBottom":"#000000","surface":"#222222",
        "primaryText":"#ffffff","secondaryText":"#bbbbbb","accent":"#00ffff","error":"#ff0000"},
      "metrics":{"cornerRadius":5,"spacing":9},
      "background":{"image":{"asset":"shape.png"},"effect":"spectrum"}
    })json");
    ModManifest theme{.id = "org.example.png-image", .name = "PNG image",
        .version = "1.0", .contentDigest = "png-image-digest",
        .kinds = {ModKind::Theme}, .permissions = {"theme.install"},
        .theme = {.dataPath = directory.filePath("theme.json")},
        .packageRoot = directory.path()};

    ThemeManager themes;
    QString error;
    REQUIRE(themes.registerTheme(theme, error));
    REQUIRE(themes.selectTheme(theme.id, error));
    CHECK(themes.backgroundImageFit() == "preserveAspectFit");
    CHECK(themes.backgroundImageAlignment() == "center");
    CHECK(themes.backgroundImageOpacity() == 1.0);
    CHECK(themes.backgroundEffect() == "spectrum");
    CHECK(themes.controlAreaLeftInset() == 36);
    CHECK(themes.controlAreaTopInset() == 36);
    CHECK(themes.controlAreaRightInset() == 36);
    CHECK(themes.controlAreaBottomInset() == 36);
    CHECK(themes.closeButtonRightInset() == 0);
    CHECK(themes.closeButtonTopInset() == 0);
    CHECK(themes.closeButtonWidth() == 44);
    CHECK(themes.closeButtonHeight() == 36);
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
