#include "app/ApplicationSession.hpp"
#include "app/TrayController.hpp"
#include <QApplication>
#ifdef YAAP_HAVE_MPRIS
#include "app/MprisService.hpp"
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#endif
#include "app/AudioVisualizationModel.hpp"
#include "app/PlayerController.hpp"
#include "app/LibraryController.hpp"
#include "app/RadioController.hpp"
#include "app/RadioBrowserDirectoryModel.hpp"
#include "app/WindowPresentationController.hpp"
#include "app/WindowShapeController.hpp"
#include "library/LibraryDatabase.hpp"
#include "library/LibraryIndexer.hpp"
#include "mods/ModApiInfo.hpp"
#include "mods/ModManager.hpp"
#include "mods/ThemeManager.hpp"
#include "provider_runtime/ProviderCache.hpp"
#include "radio/RadioBrowserClient.hpp"
#include "audio/AudioAnalysisEngine.hpp"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <qqml.h>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QDebug>

int main(int argc, char* argv[])
{
    QQuickWindow::setDefaultAlphaBuffer(true);
    QApplication application(argc, argv);
    QCoreApplication::setApplicationName("Yaap");
    QCoreApplication::setApplicationVersion("0.2.0-prototype");
    QCoreApplication::setOrganizationName("Yaap");

    QGuiApplication::setDesktopFileName("org.yaap.Yaap");
    QGuiApplication::setQuitOnLastWindowClosed(false);
    QCommandLineParser arguments;
    arguments.setApplicationDescription("Yaap music player");
    arguments.addHelpOption();
    arguments.addVersionOption();
    arguments.addOption({"background", "Start without showing the main window (Linux session bus required)."});
    arguments.addOption({"quit", "Quit the running instance."});
    arguments.addPositionalArgument("uri", "Optional audio file or HTTP(S) URL to play.", "[uri]");
    arguments.process(application);
    if (arguments.positionalArguments().size() > 1) arguments.showHelp(EXIT_FAILURE);
    const auto uri = arguments.positionalArguments().isEmpty() ? QUrl{}
        : QUrl::fromUserInput(arguments.positionalArguments().front(), QDir::currentPath(),
            QUrl::AssumeLocalFile);
    bool desktopIntegration = false;
#ifdef YAAP_HAVE_MPRIS
    auto bus = QDBusConnection::sessionBus();
    if (bus.isConnected()) {
        desktopIntegration = bus.registerService(yaap::MprisService::serviceName);
        if (!desktopIntegration) {
            const QDBusReply<bool> existing = bus.interface()->isServiceRegistered(
                yaap::MprisService::serviceName);
            if (!existing.isValid() || !existing.value()) {
                qCritical() << "Could not claim the Yaap session service:" << bus.lastError().message();
                return EXIT_FAILURE;
            }
            const auto call = [&](const char* interface, const char* method, const QVariantList& values = {}) {
                auto message = QDBusMessage::createMethodCall(yaap::MprisService::serviceName,
                    yaap::MprisService::objectPath, interface, method);
                message.setArguments(values);
                const auto reply = bus.call(message, QDBus::Block, 5000);
                if (reply.type() == QDBusMessage::ErrorMessage) {
                    qCritical() << reply.errorMessage();
                    return false;
                }
                return true;
            };
            if (arguments.isSet("quit"))
                return call("org.mpris.MediaPlayer2", "Quit") ? EXIT_SUCCESS : EXIT_FAILURE;
            if (!uri.isEmpty() && !call("org.mpris.MediaPlayer2.Player", "OpenUri",
                    {uri.toString(QUrl::FullyEncoded)})) return EXIT_FAILURE;
            if (!arguments.isSet("background") && !call("org.mpris.MediaPlayer2", "Raise"))
                return EXIT_FAILURE;
            return EXIT_SUCCESS;
        }
    } else {
        qWarning() << "No session bus: MPRIS and background mode are unavailable.";
    }
#endif
    if (arguments.isSet("quit")) return EXIT_SUCCESS;
    if (arguments.isSet("background") && !desktopIntegration) {
        qCritical() << "Background mode requires Linux session-bus integration.";
        return EXIT_FAILURE;
    }
    yaap::ApplicationSession session{desktopIntegration};
    if (arguments.isSet("background")) session.setKeepPlayingInBackground(true);
    QQuickStyle::setStyle("Fusion");

    const auto dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const auto cachePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir{}.mkpath(dataPath);
    QDir{}.mkpath(cachePath + "/artwork");
    QDir{}.mkpath(dataPath + "/mods");
    yaap::ProviderCache providerCache;
    QString providerCacheError;
    if (!providerCache.open(cachePath + "/providers.sqlite3", providerCacheError)) {
        qWarning() << providerCacheError;
    }

    yaap::LibraryDatabase libraryDatabase;
    QString databaseError;
    if (!libraryDatabase.open(dataPath + "/library.sqlite3", databaseError)) {
        qCritical() << databaseError;
    }
    yaap::LibraryIndexer libraryIndexer{libraryDatabase, cachePath + "/artwork"};
    yaap::LibraryController library{libraryDatabase, libraryIndexer};
    yaap::AudioAnalysisEngine audioAnalysis;
    yaap::PlayerController player{audioAnalysis};
#ifdef YAAP_HAVE_MPRIS
    std::unique_ptr<yaap::MprisService> mpris;
    if (desktopIntegration) {
        mpris = std::make_unique<yaap::MprisService>(player, session, bus);
        if (!mpris->isRegistered()) {
            qCritical() << "Could not register the MPRIS object.";
            return EXIT_FAILURE;
        }
    }
#endif
    yaap::AudioVisualizationModel audioVisualization{audioAnalysis};
    yaap::RadioBrowserClient radioBrowserClient;
    yaap::RadioBrowserDirectoryModel radioDirectory{radioBrowserClient, providerCache};
    yaap::RadioController radio{&radioBrowserClient};
    QObject::connect(&library, &yaap::LibraryController::playbackRequested,
        &player, [&player](const QUrl& url, const QString&, const QUrl& artwork) {
            player.openFile(url, artwork);
        });
    QObject::connect(&radio, &yaap::RadioController::playbackRequested,
        &player, [&player](const QUrl& url, const QString& title) {
            player.openStream(url, title, true);
        });
    QObject::connect(&radioDirectory,
        &yaap::RadioBrowserDirectoryModel::playbackRequested,
        &player, [&player](const QUrl& url, const QString& title) {
            player.openStream(url, title, true);
        });
    QObject::connect(&radioDirectory,
        &yaap::RadioBrowserDirectoryModel::saveRequested,
        &radio, [&radio](const yaap::RadioBrowserStation& station) {
            radio.addDirectoryStation(station);
        });
    yaap::ThemeManager themes;
    yaap::WindowPresentationController presentation;
    yaap::ModApiInfo modApi;
    yaap::ModManager mods{themes,
        {QCoreApplication::applicationDirPath() + "/mods", dataPath + "/mods"}};
    qmlRegisterSingletonInstance("Yaap.App", 1, 0, "Session", &session);
    qmlRegisterSingletonInstance("Yaap.App", 1, 0, "Player", &player);
    qmlRegisterSingletonInstance("Yaap.App", 1, 0, "MusicLibrary", &library);
    qmlRegisterSingletonInstance("Yaap.App", 1, 0, "Radio", &radio);
    qmlRegisterSingletonInstance(
        "Yaap.App", 1, 0, "RadioDirectory", &radioDirectory);
    qmlRegisterSingletonInstance("Yaap.ModApi", 1, 0, "Theme", &themes);
    qmlRegisterSingletonInstance("Yaap.ModApi", 1, 0, "Mods", &mods);
    qmlRegisterSingletonInstance("Yaap.ModApi", 1, 0, "ModApi", &modApi);
    qmlRegisterSingletonInstance(
        "Yaap.ModApi", 1, 1, "AudioVisualization", &audioVisualization);
    qmlRegisterSingletonInstance("Yaap.App", 1, 0, "Presentation", &presentation);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &application,
        [] { QCoreApplication::exit(EXIT_FAILURE); },
        Qt::QueuedConnection);
    engine.loadFromModule("Yaap.App", "Main");

    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0));
    if (window == nullptr) {
        qCritical() << "Yaap root object is not a QQuickWindow.";
        return EXIT_FAILURE;
    }
    yaap::WindowShapeController windowShape{themes, presentation, *window};
    // Main.qml starts hidden so the persisted mode, geometry, and shaped input
    // mask can be applied before the first visible frame.
    session.setWindow(window);
    const QIcon appIcon{":/yaap/org.yaap.Yaap.svg"};
    application.setWindowIcon(appIcon);
    yaap::TrayController tray{session, appIcon};
    if (!arguments.isSet("background")) window->show();
    if (!uri.isEmpty()) {
        if (uri.isLocalFile()) player.openFile(uri, {}, true);
        else player.openStream(uri);
    }

    QObject::connect(&application, &QCoreApplication::aboutToQuit, &player,
        &yaap::PlayerController::shutdown);

    return application.exec();
}
