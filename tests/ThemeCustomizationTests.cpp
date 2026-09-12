#include "mods/ThemeManager.hpp"
#include "mods/ModManager.hpp"
#include <catch2/catch_test_macros.hpp>
#include <QSettings>
#include <QTemporaryDir>
#include <QImage>
#include <QSet>
#include <limits>

namespace yaap {
TEST_CASE("Custom themes persist every field and survive theme reloads")
{
    QSettings{}.remove("themes/custom");
    ThemeManager themes;
    ModManager mods{themes, {QString::fromUtf8(YAAP_SAMPLE_MODS_PATH)}};
    REQUIRE(themes.availableThemes().size() >= 6);
    REQUIRE(themes.useTheme("org.yaap.ocean-theme").isEmpty());
    REQUIRE(themes.useTheme("builtin.custom").isEmpty());
    CHECK(themes.backgroundEffect() == "waves");
    auto draft = themes.customValues();
    QSet<QString> keys;
    for (const auto& field : themes.customFields()) keys.insert(field.toMap().value("key").toString());
    CHECK(keys.size() == draft.size());
    for (auto it = draft.cbegin(); it != draft.cend(); ++it) CHECK(keys.contains(it.key()));
    draft["accent"] = "#804488cc";
    draft["backgroundEffect"] = "spectrum";
    draft["spectrumColumns"] = 32;
    draft["spectrumHueShiftDegrees"] = 37.5;
    draft["miniControlAreaLeftInset"] = 20;
    draft["miniBackgroundEffect"] = "paperPlanes";
    REQUIRE(themes.applyCustom(draft).isEmpty());
    const auto saved = themes.customValues();
    REQUIRE(themes.useTheme("builtin.default").isEmpty());
    REQUIRE(themes.useTheme("builtin.custom").isEmpty());
    CHECK(themes.customValues() == saved);
    mods.refresh();
    CHECK(themes.currentThemeId() == "builtin.custom");
    CHECK(themes.customValues() == saved);
    ThemeManager restarted;
    ModManager restored{restarted, {QString::fromUtf8(YAAP_SAMPLE_MODS_PATH)}};
    CHECK(restarted.currentThemeId() == "builtin.custom");
    CHECK(restarted.customValues() == saved);
    QSettings{}.remove("themes/custom");
    QSettings{}.setValue("mods/currentTheme", "builtin.default");
}

TEST_CASE("Invalid custom drafts leave both active theme and saved values untouched")
{
    QSettings{}.remove("themes/custom");
    ThemeManager themes;
    REQUIRE(themes.useTheme("builtin.custom").isEmpty());
    const auto original = themes.customValues();
    auto draft = original;
    SECTION("Invalid color") { draft["accent"] = "not-a-color"; }
    SECTION("Nonfinite number") { draft["spectrumOpacity"] = std::numeric_limits<double>::quiet_NaN(); }
    SECTION("Fractional integer") { draft["spacing"] = 2.5; }
    SECTION("Invalid effect") { draft["backgroundEffect"] = "executable"; }
    SECTION("Invalid miniplayer effect") { draft["miniBackgroundEffect"] = "executable"; }
    SECTION("No room for controls") { draft["miniControlAreaTopInset"] = 100; }
    SECTION("Missing attribute") { draft.remove("accent"); }
    SECTION("Remote image") { draft["backgroundImageSource"] = "https://example.com/image.png"; }
    SECTION("Missing local image") { draft["backgroundImageSource"] = "file:///missing/theme.png"; }
    REQUIRE_FALSE(themes.applyCustom(draft).isEmpty());
    CHECK(themes.customValues() == original);
    CHECK(QSettings{}.value("themes/custom").toMap() == original);
    QSettings{}.remove("themes/custom");
    QSettings{}.setValue("mods/currentTheme", "builtin.default");
}

TEST_CASE("Miniplayer effect overrides persist independently per theme")
{
    QSettings{}.remove("mods/themeSettings");
    ThemeManager themes;
    ModManager mods{themes, {QString::fromUtf8(YAAP_SAMPLE_MODS_PATH)}};
    REQUIRE(themes.useTheme("org.yaap.ocean-theme").isEmpty());
    CHECK(themes.miniBackgroundEffect() == "followTheme");
    themes.setMiniBackgroundEffect("paperPlanes");
    themes.setMiniBackgroundEffect("invalid");
    CHECK(themes.miniBackgroundEffect() == "paperPlanes");
    REQUIRE(themes.useTheme("builtin.default").isEmpty());
    CHECK(themes.miniBackgroundEffect() == "followTheme");
    REQUIRE(themes.useTheme("org.yaap.ocean-theme").isEmpty());
    CHECK(themes.miniBackgroundEffect() == "paperPlanes");
    mods.refresh();
    CHECK(themes.miniBackgroundEffect() == "paperPlanes");
    ThemeManager restarted;
    ModManager restored{restarted, {QString::fromUtf8(YAAP_SAMPLE_MODS_PATH)}};
    CHECK(restarted.miniBackgroundEffect() == "paperPlanes");
    QSettings{}.remove("mods/themeSettings");
    QSettings{}.setValue("mods/currentTheme", "builtin.default");
}

TEST_CASE("Existing custom themes default to following the main animation")
{
    ThemeManager themes;
    auto draft = themes.customValues();
    draft.remove("miniBackgroundEffect");
    REQUIRE(themes.applyCustom(draft).isEmpty());
    CHECK(themes.miniBackgroundEffect() == "followTheme");
    QSettings{}.remove("themes/custom");
    QSettings{}.setValue("mods/currentTheme", "builtin.default");
}

TEST_CASE("Custom theme images support local selection and clearing")
{
    ThemeManager themes;
    QTemporaryDir directory;
    const auto path = directory.filePath("background.png");
    QImage image{16, 16, QImage::Format_ARGB32};
    image.fill(Qt::blue);
    REQUIRE(image.save(path));
    auto draft = themes.customValues();
    draft["backgroundImageSource"] = QUrl::fromLocalFile(path).toString();
    REQUIRE(themes.applyCustom(draft).isEmpty());
    CHECK(themes.backgroundImageSource().toLocalFile() == path);
    draft["backgroundImageSource"] = "";
    REQUIRE(themes.applyCustom(draft).isEmpty());
    CHECK(themes.backgroundImageSource().isEmpty());
    QSettings{}.remove("themes/custom");
    QSettings{}.setValue("mods/currentTheme", "builtin.default");
}
}
