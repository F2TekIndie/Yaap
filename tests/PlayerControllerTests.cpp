#include "app/PlayerController.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QThread>
#include <QUrl>

TEST_CASE("Player controller starts an opted-in live radio stream",
    "[.audio-device][.live-radio]")
{
    const auto radioUrl = qEnvironmentVariable("YAAP_TEST_RADIO_URL");
    if (radioUrl.isEmpty()) {
        SKIP("Set YAAP_TEST_RADIO_URL to run the live radio controller test.");
    }

    yaap::PlayerController player;
    player.openStream(QUrl{radioUrl}, "Live radio test");

    QElapsedTimer timer;
    timer.start();
    while (!player.isPlaying() && player.errorMessage().isEmpty()
        && timer.elapsed() < 20'000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        QThread::msleep(5);
    }

    INFO(player.errorMessage().toStdString());
    REQUIRE(player.isPlaying());
    REQUIRE(player.hasAudio());

    while (player.positionMilliseconds() == 0 && timer.elapsed() < 22'000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        QThread::msleep(5);
    }
    REQUIRE(player.positionMilliseconds() > 0);
}
