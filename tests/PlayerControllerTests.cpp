#include "app/PlayerController.hpp"
#include "audio/AudioAnalysisEngine.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QThread>
#include <QUrl>
#include <QSettings>
#include <QTcpServer>
#include <QTcpSocket>
#include <QDataStream>

#include <catch2/catch_approx.hpp>

TEST_CASE("Player volume and mute controls are bounded and observable")
{
    QSettings settings;
    settings.remove("playback/volume");
    settings.remove("playback/muted");
    yaap::AudioAnalysisEngine analysis;
    yaap::PlayerController player{analysis};

    player.setVolume(2.0);
    CHECK(player.volume() == Catch::Approx(1.0));
    player.setVolume(0.35);
    CHECK(player.volume() == Catch::Approx(0.35));
    player.setMuted(true);
    CHECK(player.muted());
    player.toggleMuted();
    CHECK_FALSE(player.muted());

    settings.remove("playback/volume");
    settings.remove("playback/muted");
}

TEST_CASE("Player controller starts an opted-in live radio stream",
    "[.audio-device][.live-radio]")
{
    const auto radioUrl = qEnvironmentVariable("YAAP_TEST_RADIO_URL");
    if (radioUrl.isEmpty()) {
        SKIP("Set YAAP_TEST_RADIO_URL to run the live radio controller test.");
    }

    yaap::AudioAnalysisEngine analysis;
    yaap::PlayerController player{analysis};
    player.openStream(QUrl{radioUrl}, "Live radio test", true);

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

    while (analysis.snapshot().sequence == 0 && timer.elapsed() < 24'000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        QThread::msleep(5);
    }
    const auto spectrum = analysis.snapshot();
    REQUIRE(spectrum.sequence > 0);
    REQUIRE(spectrum.active);
}

TEST_CASE("Only live radio reconnects after a finite HTTP response ends", "[.audio-device]")
{
    bool live = false;
    SECTION("Remote track stays finished") { live = false; }
    SECTION("Live station reconnects") { live = true; }
    QByteArray wav;
    QDataStream data{&wav, QIODevice::WriteOnly};
    data.setByteOrder(QDataStream::LittleEndian);
    data.writeRawData("RIFF", 4);
    data << quint32(36 + 19200);
    data.writeRawData("WAVEfmt ", 8);
    data << quint32(16) << quint16(1) << quint16(2) << quint32(48000)
         << quint32(192000) << quint16(4) << quint16(16);
    data.writeRawData("data", 4);
    data << quint32(19200);
    wav.append(QByteArray(19200, '\0'));
    QTcpServer server;
    REQUIRE(server.listen(QHostAddress::LocalHost));
    int requests = 0;
    QObject::connect(&server, &QTcpServer::newConnection, [&] {
        auto* socket = server.nextPendingConnection();
        QObject::connect(socket, &QTcpSocket::readyRead, [&, socket] {
            socket->readAll();
            if (socket->property("responded").toBool()) return;
            socket->setProperty("responded", true);
            ++requests;
            socket->write("HTTP/1.1 200 OK\r\nContent-Type: audio/wav\r\nContent-Length: "
                + QByteArray::number(wav.size()) + "\r\nConnection: close\r\n\r\n" + wav);
            socket->disconnectFromHost();
        });
    });
    yaap::PlayerController player;
    player.openStream(QUrl{"http://127.0.0.1:" + QString::number(server.serverPort())
        + "/song.wav"}, "Test", live);
    QElapsedTimer timer;
    timer.start();
    bool finished = false;
    while (timer.elapsed() < 2500) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        finished |= player.stateName() == "Finished";
        QThread::msleep(5);
    }
    INFO(player.errorMessage().toStdString());
    if (live) {
        CHECK(requests >= 2);
    } else {
        CHECK(finished);
        CHECK(player.stateName() == "Finished");
        CHECK(requests == 1);
    }
    player.stop();
}
