#include "app/WindowPresentationController.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QSettings>

namespace yaap {
namespace {

const QRect availableScreen{100, 50, 1'600, 900};

void clearPresentationSettings()
{
    QSettings settings;
    settings.remove("window");
    settings.sync();
}

} // namespace

TEST_CASE("Window presentation defaults to a centered normal player")
{
    clearPresentationSettings();
    WindowPresentationController controller;
    CHECK_FALSE(controller.isMiniPlayer());
    CHECK(controller.normalGeometryFor(availableScreen) == QRect{450, 220, 900, 560});
}

TEST_CASE("Window presentation records normal geometry once and restores it")
{
    clearPresentationSettings();
    WindowPresentationController controller;
    const QRect normal{220, 140, 1'040, 650};
    controller.enterMiniPlayer(normal, availableScreen);
    REQUIRE(controller.isMiniPlayer());
    CHECK(controller.normalGeometry() == normal);

    controller.enterMiniPlayer(QRect{300, 200, 800, 500}, availableScreen);
    CHECK(controller.normalGeometry() == normal);
    controller.restoreFullPlayer();
    CHECK_FALSE(controller.isMiniPlayer());
    CHECK(controller.normalGeometryFor(availableScreen) == normal);
}

TEST_CASE("Window presentation persists mode and geometries")
{
    clearPresentationSettings();
    {
        WindowPresentationController controller;
        controller.enterMiniPlayer(QRect{240, 160, 1'000, 620}, availableScreen);
        controller.recordMiniPlayerPosition({1'100, 700}, availableScreen);
    }
    WindowPresentationController restored;
    CHECK(restored.isMiniPlayer());
    CHECK(restored.normalGeometry() == QRect{240, 160, 1'000, 620});
    CHECK(restored.miniPlayerGeometryFor(availableScreen)
        == QRect{1'100, 700, 480, 112});
}

TEST_CASE("Entering miniplayer shrinks beside the current normal window")
{
    clearPresentationSettings();
    {
        WindowPresentationController controller;
        controller.enterMiniPlayer(QRect{1'100, 700, 800, 500}, availableScreen);
        controller.recordMiniPlayerPosition({200, 100}, availableScreen);
        controller.restoreFullPlayer();
    }

    WindowPresentationController controller;
    controller.enterMiniPlayer(QRect{320, 180, 900, 560}, availableScreen);
    CHECK(controller.miniPlayerGeometryFor(availableScreen)
        == QRect{320, 180, 480, 112});
}

TEST_CASE("Window presentation clamps off-screen and rejects malformed state")
{
    clearPresentationSettings();
    QSettings settings;
    settings.setValue("window/presentation-schema-version", 1);
    settings.setValue("window/normal-geometry-v1", QRect{-50'000, 70'000, 10, 10});
    settings.setValue("window/miniplayer-position-v1", QPoint{50'000, 50'000});
    settings.sync();

    WindowPresentationController controller;
    CHECK(controller.normalGeometryFor(availableScreen) == QRect{450, 220, 900, 560});
    CHECK(controller.miniPlayerGeometryFor(availableScreen)
        == QRect{1'220, 838, 480, 112});
}

TEST_CASE("Unknown presentation settings versions fall back safely")
{
    clearPresentationSettings();
    QSettings settings;
    settings.setValue("window/presentation-schema-version", 99);
    settings.setValue("window/presentation-mode-v1", 1);
    settings.setValue("window/normal-geometry-v1", QRect{200, 100, 1'200, 700});
    settings.sync();

    WindowPresentationController controller;
    CHECK_FALSE(controller.isMiniPlayer());
    CHECK(controller.normalGeometryFor(availableScreen) == QRect{450, 220, 900, 560});
    CHECK(QSettings{}.value("window/presentation-schema-version").toInt() == 1);
}

} // namespace yaap
