#include "app/PlayerController.hpp"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

int main(int argc, char* argv[])
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName("Yaap");
    QCoreApplication::setApplicationVersion("0.1.0-prototype");
    QCoreApplication::setOrganizationName("Yaap");

    QQuickStyle::setStyle("Fusion");

    yaap::PlayerController player;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("player", &player);
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &application,
        [] { QCoreApplication::exit(EXIT_FAILURE); },
        Qt::QueuedConnection);
    engine.loadFromModule("Yaap", "Main");

    return application.exec();
}

