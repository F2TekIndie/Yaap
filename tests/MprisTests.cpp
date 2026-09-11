#include "app/ApplicationSession.hpp"
#include "app/MprisService.hpp"
#include "app/PlayerController.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusVariant>
#include <QEventLoop>
#include <QSettings>
#include <QTimer>
#include <limits>

namespace {
QDBusMessage call(QDBusConnection& client, const QString& service,
    const QString& interface, const QString& method, const QVariantList& arguments = {})
{
    auto message = QDBusMessage::createMethodCall(service, yaap::MprisService::objectPath,
        interface, method);
    message.setArguments(arguments);
    QDBusPendingCallWatcher pending{client.asyncCall(message, 3000)};
    QEventLoop loop;
    QObject::connect(&pending, &QDBusPendingCallWatcher::finished, &loop, &QEventLoop::quit);
    if (!pending.isFinished()) loop.exec();
    return pending.reply();
}
}

TEST_CASE("MPRIS exports correctly typed capabilities and controls volume over D-Bus", "[dbus]")
{
    const QString connectionName = "yaap-test-server";
    auto server = QDBusConnection::connectToBus(QDBusConnection::SessionBus, connectionName);
    REQUIRE(server.isConnected());
    const auto service = "org.mpris.MediaPlayer2.YaapTest.instance"
        + QString::number(QCoreApplication::applicationPid());
    REQUIRE(server.registerService(service));
    auto client = QDBusConnection::connectToBus(QDBusConnection::SessionBus, "yaap-test-client");
    yaap::PlayerController player;
    yaap::ApplicationSession session{true};
    yaap::MprisService mpris{player, session, server};
    REQUIRE(mpris.isRegistered());
    const auto all = call(client, service, "org.freedesktop.DBus.Properties", "GetAll",
        {QString{"org.mpris.MediaPlayer2.Player"}});
    REQUIRE(all.type() == QDBusMessage::ReplyMessage);
    const auto values = qdbus_cast<QVariantMap>(all.arguments().front());
    CHECK(values.value("PlaybackStatus").toString() == "Stopped");
    CHECK_FALSE(values.value("CanPlay").toBool());
    CHECK_FALSE(values.value("CanSeek").toBool());
    CHECK_FALSE(values.value("CanGoNext").toBool());
    CHECK_FALSE(values.value("CanGoPrevious").toBool());
    CHECK(values.value("Position").metaType().id() == QMetaType::LongLong);
    CHECK(values.value("Rate").toDouble() == 1.0);
    CHECK(qdbus_cast<QVariantMap>(values.value("Metadata")).isEmpty());

    player.setMuted(true);
    const auto set = call(client, service, "org.freedesktop.DBus.Properties", "Set",
        {QString{"org.mpris.MediaPlayer2.Player"}, QString{"Volume"},
            QVariant::fromValue(QDBusVariant{0.35})});
    REQUIRE(set.type() == QDBusMessage::ReplyMessage);
    CHECK(player.volume() == Catch::Approx(0.35));
    CHECK_FALSE(player.muted());
    auto* adaptor = mpris.findChild<yaap::MprisPlayerAdaptor*>();
    REQUIRE(adaptor);
    adaptor->setVolume(std::numeric_limits<double>::quiet_NaN());
    CHECK(player.volume() == Catch::Approx(0.35));
    adaptor->setVolume(-1);
    CHECK(player.volume() == 0.0);
    adaptor->SetPosition(QDBusObjectPath{"/stale"}, 1000);
    CHECK(player.positionMilliseconds() == 0);
    const auto introspection = call(client, service, "org.freedesktop.DBus.Introspectable", "Introspect");
    REQUIRE(introspection.type() == QDBusMessage::ReplyMessage);
    CHECK(introspection.arguments().front().toString().contains("org.mpris.MediaPlayer2.Player"));
    CHECK(introspection.arguments().front().toString().contains("org.mpris.MediaPlayer2"));
    server.unregisterService(service);
    QSettings{}.remove("playback/volume");
    QSettings{}.remove("playback/muted");
}

TEST_CASE("Background playback preference persists but needs an activation backend")
{
    QSettings{}.remove("application/keepPlayingInBackground");
    yaap::ApplicationSession available{true};
    CHECK(available.keepPlayingInBackground());
    available.setKeepPlayingInBackground(false);
    yaap::ApplicationSession restored{true};
    CHECK_FALSE(restored.keepPlayingInBackground());
    restored.setKeepPlayingInBackground(true);
    yaap::ApplicationSession unavailable{false};
    CHECK_FALSE(unavailable.keepPlayingInBackground());
    CHECK_FALSE(unavailable.backgroundAvailable());
    QSettings{}.remove("application/keepPlayingInBackground");
}
