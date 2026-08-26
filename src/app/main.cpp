#include "app/AudioVisualizationModel.hpp"
#include "app/PlayerController.hpp"
#include "app/LibraryController.hpp"
#include "app/RadioController.hpp"
#include "app/RadioBrowserDirectoryModel.hpp"
#include "app/WindowShapeController.hpp"
#include "library/LibraryDatabase.hpp"
#include "library/LibraryIndexer.hpp"
#include "mods/ExtensionRegistry.hpp"
#include "mods/ModApiInfo.hpp"
#include "mods/ModManager.hpp"
#include "mods/PermissionStore.hpp"
#include "mods/ThemeManager.hpp"
#include "provider_runtime/ProviderExtensionManager.hpp"
#include "provider_runtime/ProviderGateway.hpp"
#include "provider_runtime/ProviderCache.hpp"
#include "providers/ProviderAccountStore.hpp"
#include "radio/RadioBrowserClient.hpp"
#include "security/CredentialHandleBroker.hpp"
#include "security/CredentialStore.hpp"
#include "audio/AudioAnalysisEngine.hpp"

#include <QCoreApplication>
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
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName("Yaap");
    QCoreApplication::setApplicationVersion("0.2.0-prototype");
    QCoreApplication::setOrganizationName("Yaap");

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
    yaap::AudioVisualizationModel audioVisualization{audioAnalysis};
    yaap::RadioBrowserClient radioBrowserClient;
    yaap::RadioBrowserDirectoryModel radioDirectory{radioBrowserClient, providerCache};
    yaap::RadioController radio{&radioBrowserClient};
    QObject::connect(&library, &yaap::LibraryController::playbackRequested,
        &player, [&player](const QUrl& url, const QString&) { player.openFile(url); });
    QObject::connect(&radio, &yaap::RadioController::playbackRequested,
        &player, [&player](const QUrl& url, const QString& title) {
            player.openStream(url, title);
        });
    QObject::connect(&radioDirectory,
        &yaap::RadioBrowserDirectoryModel::playbackRequested,
        &player, [&player](const QUrl& url, const QString& title) {
            player.openStream(url, title);
        });
    QObject::connect(&radioDirectory,
        &yaap::RadioBrowserDirectoryModel::saveRequested,
        &radio, [&radio](const yaap::RadioBrowserStation& station) {
            radio.addDirectoryStation(station);
        });
    auto credentialStore = yaap::CredentialStore::createPlatformStore();
    yaap::CredentialHandleBroker credentialHandles{*credentialStore};
    yaap::ProviderAccountStore providerAccounts{*credentialStore, credentialHandles};

    yaap::PermissionStore permissions;
    yaap::ThemeManager themes;
    yaap::ExtensionRegistry extensions{permissions};
    yaap::ModApiInfo modApi;
    yaap::ModManager mods{permissions, themes, extensions,
        {QCoreApplication::applicationDirPath() + "/mods", dataPath + "/mods"}};
    yaap::ProviderExtensionManager providerExtensions{
        mods, permissions, providerAccounts, credentialHandles};
    yaap::ProviderGateway providerGateway{
        providerExtensions, providerCache, providerAccounts};
    QObject::connect(&providerAccounts, &yaap::ProviderAccountStore::accountRemoved,
        [&providerCache](const QString& accountId) {
            QString error;
            providerCache.removeNamespace(accountId, error);
        });
    QObject::connect(&providerGateway, &yaap::ProviderGateway::playbackRequested,
        &player, [&player](const QUrl& url, const QString& title) {
            if (url.isLocalFile()) {
                player.openFile(url);
            } else {
                player.openStream(url, title);
            }
        });

    qmlRegisterSingletonInstance("Yaap.App", 1, 0, "Player", &player);
    qmlRegisterSingletonInstance("Yaap.App", 1, 0, "MusicLibrary", &library);
    qmlRegisterSingletonInstance("Yaap.App", 1, 0, "Radio", &radio);
    qmlRegisterSingletonInstance(
        "Yaap.App", 1, 0, "RadioDirectory", &radioDirectory);
    qmlRegisterSingletonInstance("Yaap.ModApi", 1, 0, "Theme", &themes);
    qmlRegisterSingletonInstance("Yaap.ModApi", 1, 0, "Mods", &mods);
    qmlRegisterSingletonInstance("Yaap.ModApi", 1, 0, "Extensions", &extensions);
    qmlRegisterSingletonInstance("Yaap.ModApi", 1, 0, "ModApi", &modApi);
    qmlRegisterSingletonInstance(
        "Yaap.ModApi", 1, 1, "AudioVisualization", &audioVisualization);
    qmlRegisterSingletonInstance(
        "Yaap.ModApi", 1, 0, "ProviderExtensions", &providerExtensions);
    qmlRegisterSingletonInstance("Yaap.App", 1, 0, "Providers", &providerGateway);
    qmlRegisterSingletonInstance("Yaap.App", 1, 0, "ProviderAccounts", &providerAccounts);

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
    yaap::WindowShapeController windowShape{themes, *window};

    // The frameless dialogs are persistent top-level QML windows. Once one has
    // been shown, relying on quitOnLastWindowClosed would leave the process
    // alive after the main window closes.
    QObject::connect(window, &QQuickWindow::closing, &application,
        [&application, &player, window] {
            window->setProperty("applicationClosing", true);
            player.shutdown();
            application.quit();
        });
    QObject::connect(&application, &QCoreApplication::aboutToQuit, &player,
        &yaap::PlayerController::shutdown);

    return application.exec();
}
