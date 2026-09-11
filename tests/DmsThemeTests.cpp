#include "mods/ThemeManager.hpp"
#include "mods/ModManager.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QSaveFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QEventLoop>
#include <QTimer>

namespace {
struct Environment {
    QByteArray key, previous;
    bool existed;
    Environment(const char* name, const QString& value)
        : key(name), previous(qgetenv(name)), existed(qEnvironmentVariableIsSet(name))
    { qputenv(name, value.toUtf8()); }
    ~Environment() { if (existed) qputenv(key.constData(), previous); else qunsetenv(key.constData()); }
};
void write(const QString& path, const QByteArray& bytes)
{
    QDir{}.mkpath(QFileInfo{path}.absolutePath());
    QSaveFile file{path};
    REQUIRE(file.open(QIODevice::WriteOnly));
    REQUIRE(file.write(bytes) == bytes.size());
    REQUIRE(file.commit());
}
void waitForRefresh()
{
    QEventLoop loop;
    QTimer::singleShot(1200, &loop, &QEventLoop::quit);
    loop.exec();
}
}
TEST_CASE("DMS themes follow exported palettes and atomic updates without changing shell settings")
{
    QTemporaryDir root;
    Environment cache{"XDG_CACHE_HOME", root.filePath("cache")};
    Environment config{"XDG_CONFIG_HOME", root.filePath("config")};
    Environment state{"XDG_STATE_HOME", root.filePath("state")};
    const auto colorsPath = root.filePath("cache/DankMaterialShell/dms-colors.json");
    const auto settingsPath = root.filePath("config/DankMaterialShell/settings.json");
    const auto sessionPath = root.filePath("state/DankMaterialShell/session.json");
    yaap::ThemeManager themes;
    REQUIRE(themes.useTheme("builtin.dms").isEmpty());
    CHECK(themes.dmsStatus().contains("Waiting"));
    const QJsonObject dark{{"surface", "#111111"}, {"surface_container", "#222222"},
        {"on_surface", "#eeeeee"}, {"on_surface_variant", "#aaaaaa"},
        {"primary", "#aabbcc"}, {"error", "#ff5555"}};
    auto light = dark;
    light["surface"] = "#eeeeee";
    light["on_surface"] = "#111111";
    write(colorsPath, QJsonDocument{QJsonObject{{"mode", "dark"},
        {"colors", QJsonObject{{"dark", dark}, {"light", light}}}}}.toJson());
    write(settingsPath, R"({"cornerRadius":17,"popupTransparency":0.8})");
    write(sessionPath, R"({"isLightMode":false})");
    waitForRefresh();
    CHECK(themes.accent() == QColor{"#aabbcc"});
    CHECK(themes.cornerRadius() == 17);
    CHECK(themes.surface().alphaF() == Catch::Approx(0.8).margin(0.01));
    write(sessionPath, R"({"isLightMode":true})");
    waitForRefresh();
    CHECK(themes.primaryText() == QColor{"#111111"});
    CHECK(themes.dmsStatus().contains("light mode"));
    const auto valid = themes.customValues();
    write(colorsPath, "{incomplete");
    waitForRefresh();
    CHECK(themes.customValues() == valid);
    CHECK(themes.dmsStatus().contains("invalid"));
    REQUIRE(themes.useTheme("builtin.default").isEmpty());
    waitForRefresh();
    CHECK(themes.currentThemeId() == "builtin.default");
    QFile settings{settingsPath};
    REQUIRE(settings.open(QIODevice::ReadOnly));
    CHECK(settings.readAll() == QByteArray{R"({"cornerRadius":17,"popupTransparency":0.8})"});
    QSettings{}.setValue("mods/currentTheme", "builtin.default");
}
