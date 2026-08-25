#include "app/PlayerController.hpp"
#include "app/LibraryController.hpp"
#include "library/LibraryDatabase.hpp"
#include "library/LibraryIndexer.hpp"
#include "mods/ExtensionRegistry.hpp"
#include "mods/ModApiInfo.hpp"
#include "mods/ModManager.hpp"
#include "mods/PermissionStore.hpp"
#include "mods/ThemeManager.hpp"
#include "provider_runtime/ProviderExtensionManager.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <qqml.h>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QDebug>

int main(int argc, char* argv[])
{
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
    yaap::LibraryDatabase libraryDatabase;
    QString databaseError;
    if (!libraryDatabase.open(dataPath + "/library.sqlite3", databaseError)) {
        qCritical() << databaseError;
    }
    yaap::LibraryIndexer libraryIndexer{libraryDatabase, cachePath + "/artwork"};
    yaap::LibraryController library{libraryDatabase, libraryIndexer};
    yaap::PlayerController player;

    yaap::PermissionStore permissions;
    yaap::ThemeManager themes;
    yaap::ExtensionRegistry extensions{permissions};
    yaap::ModApiInfo modApi;
    yaap::ModManager mods{permissions, themes, extensions,
        {QCoreApplication::applicationDirPath() + "/mods", dataPath + "/mods"}};
    yaap::ProviderExtensionManager providerExtensions{mods, permissions};

    qmlRegisterSingletonInstance("Yaap.App", 1, 0, "Player", &player);
    qmlRegisterSingletonInstance("Yaap.App", 1, 0, "MusicLibrary", &library);
    qmlRegisterSingletonInstance("Yaap.ModApi", 1, 0, "Theme", &themes);
    qmlRegisterSingletonInstance("Yaap.ModApi", 1, 0, "Mods", &mods);
    qmlRegisterSingletonInstance("Yaap.ModApi", 1, 0, "Extensions", &extensions);
    qmlRegisterSingletonInstance("Yaap.ModApi", 1, 0, "ModApi", &modApi);
    qmlRegisterSingletonInstance(
        "Yaap.ModApi", 1, 0, "ProviderExtensions", &providerExtensions);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &application,
        [] { QCoreApplication::exit(EXIT_FAILURE); },
        Qt::QueuedConnection);
    engine.loadFromModule("Yaap.App", "Main");

    return application.exec();
}
