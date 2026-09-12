#include "mods/ThemeManager.hpp"
#include "mods/ModManager.hpp"
#include "app/ApplicationSession.hpp"
#include "app/TrayController.hpp"
#include <QApplication>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QEventLoop>
#include <QImage>
#include <QDir>
#include <iostream>
#include <memory>

namespace {
QStringList warnings;
void messages(QtMsgType type, const QMessageLogContext&, const QString& message)
{
    if (type == QtWarningMsg || type == QtCriticalMsg) {
        std::cerr << message.toStdString() << '\n';
        if (message.contains("SettingsContent.qml") || message.contains("ReferenceError")
            || message.contains("TypeError")) warnings.append(message);
    }
}
void settle()
{
    QEventLoop loop;
    QTimer::singleShot(150, &loop, &QEventLoop::quit);
    loop.exec();
}
QQuickItem* visualChild(QQuickItem* parent, const QString& name)
{
    if (parent->objectName() == name) return parent;
    for (auto* child : parent->childItems())
        if (auto* found = visualChild(child, name)) return found;
    return nullptr;
}
}
int main(int argc, char** argv)
{
    QApplication app{argc, argv};
    app.setQuitOnLastWindowClosed(false);
    QTemporaryDir storage;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, storage.path());
    app.setOrganizationName("YaapSettingsTests");
    app.setApplicationName("YaapSettingsTests");
    QQuickStyle::setStyle("Fusion");
    qInstallMessageHandler(messages);
    yaap::ThemeManager theme;
    yaap::ModManager mods{theme, {QString::fromUtf8(YAAP_SAMPLE_MODS_PATH)}};
    yaap::ApplicationSession session{true};
    qmlRegisterSingletonInstance("Yaap.ModApi", 1, 0, "Theme", &theme);
    qmlRegisterSingletonInstance("Yaap.ModApi", 1, 0, "Mods", &mods);
    qmlRegisterSingletonInstance("Yaap.App", 1, 0, "Session", &session);
    QQmlEngine engine;
    QQmlComponent component{&engine, QUrl::fromLocalFile(QString::fromUtf8(YAAP_SETTINGS_QML))};
    if (component.isError()) { std::cerr << component.errorString().toStdString(); return 1; }
    QQuickWindow window;
    session.setWindow(&window);
    QPixmap trayImage{24, 24};
    trayImage.fill(Qt::green);
    yaap::TrayController tray{session, QIcon{trayImage}};
    window.setColor(theme.surface());
    std::unique_ptr<QObject> root{component.create()};
    auto* item = qobject_cast<QQuickItem*>(root.get());
    if (!item) { std::cerr << component.errorString().toStdString(); return 1; }
    item->setParentItem(window.contentItem());
    item->setPosition({16, 16});
    item->setSize({688, 548});
    window.resize(720, 580);
    window.show();
    settle();
    const auto check = [](bool condition, const char* message) {
        if (!condition) std::cerr << "FAIL: " << message << '\n';
        return condition;
    };
    if (qEnvironmentVariableIsSet("YAAP_REQUIRE_TRAY_HOST")
        && !check(QSystemTrayIcon::isSystemTrayAvailable(), "A live tray host is available")) return 1;
    auto* sections = root->findChild<QObject*>("settingsSections");
    auto* choice = root->findChild<QObject*>("themeChoice");
    auto* behavior = root->findChild<QObject*>("backgroundBehavior");
    auto* quit = root->findChild<QObject*>("quitNow");
    if (!check(sections && choice && behavior && quit, "Settings controls exist")) return 1;
    const auto screenshotDir = qEnvironmentVariable("YAAP_SETTINGS_SCREENSHOT_DIR");
    if (!screenshotDir.isEmpty()) window.grabWindow().save(screenshotDir + "/general.png");
    behavior->setProperty("checked", false);
    QMetaObject::invokeMethod(behavior, "toggled");
    if (!check(!session.keepPlayingInBackground(), "General changes runtime behavior")) return 1;
    auto* trayIcon = tray.findChild<QSystemTrayIcon*>("yaapTrayIcon");
    if (!check(trayIcon && !trayIcon->isVisible(), "Disabling background mode hides tray")) return 1;
    session.setKeepPlayingInBackground(true);
    if (!check(trayIcon->isVisible(), "Background mode shows tray")) return 1;
    bool fullRequested = false, miniRequested = false;
    QObject::connect(&session, &yaap::ApplicationSession::activationRequested, [&] { fullRequested = true; });
    QObject::connect(&session, &yaap::ApplicationSession::miniPlayerActivationRequested, [&] { miniRequested = true; });
    trayIcon->activated(QSystemTrayIcon::Trigger);
    if (!check(fullRequested, "Left click opens full player")) return 1;
    auto actions = trayIcon->contextMenu()->actions();
    if (!check(actions.size() == 4 && actions[0]->text() == "Open"
        && actions[1]->text() == "Open miniplayer" && actions[3]->text() == "Quit",
        "Tray menu has the requested actions")) return 1;
    actions[1]->trigger();
    if (!check(miniRequested, "Tray menu requests miniplayer")) return 1;
    sections->setProperty("currentIndex", 1);
    auto* miniChoice = root->findChild<QObject*>("miniBackgroundChoice");
    if (!check(miniChoice, "Miniplayer effect selector exists")) return 1;
    miniChoice->setProperty("currentIndex", 2);
    QMetaObject::invokeMethod(miniChoice, "activated", Q_ARG(int, 2));
    if (!check(theme.miniBackgroundEffect() == "waves", "Miniplayer selector applies its effect")) return 1;
    const int last = choice->property("count").toInt() - 1;
    choice->setProperty("currentIndex", last);
    QMetaObject::invokeMethod(choice, "activated", Q_ARG(int, last));
    settle();
    if (!check(theme.currentThemeId() == "builtin.custom", "Dropdown selects Custom")) return 1;
    const QStringList commonFields{"windowTop", "windowBottom", "surface", "primaryText",
        "secondaryText", "accent", "error", "cornerRadius", "spacing", "backgroundEffect", "miniBackgroundEffect"};
    for (const auto& key : commonFields) {
        auto* loader = visualChild(item, "field-" + key);
        if (!check(loader && !loader->childItems().isEmpty(), "Common appearance fields have editors")) return 1;
    }
    if (!check(!visualChild(item, "field-miniPlayerWidth")
        && !visualChild(item, "field-backgroundImageSource")
        && !visualChild(item, "field-spectrumOpacity"), "Skin internals and inactive spectrum controls are hidden")) return 1;
    QMetaObject::invokeMethod(root.get(), "change", Q_ARG(QVariant, "backgroundEffect"), Q_ARG(QVariant, "spectrum"));
    settle();
    if (!check(visualChild(item, "field-spectrumOpacity")
        && visualChild(item, "field-spectrumGradientStart"), "Spectrum exposes its appearance controls")) return 1;
    if (!screenshotDir.isEmpty()) window.grabWindow().save(screenshotDir + "/themes-before-edit.png");
    auto* spacing = visualChild(item, "editor-spacing");
    auto* apply = root->findChild<QObject*>("applyCustom");
    if (!check(spacing && apply, "Custom editors are instantiated")) return 1;
    spacing->setProperty("value", 24);
    QMetaObject::invokeMethod(spacing, "valueModified");
    QMetaObject::invokeMethod(apply, "clicked");
    settle();
    if (!check(theme.spacing() == 24, "Custom Apply uses the edited value")) return 1;
    if (!screenshotDir.isEmpty()) window.grabWindow().save(screenshotDir + "/themes.png");
    choice->setProperty("currentIndex", 0);
    QMetaObject::invokeMethod(choice, "activated", Q_ARG(int, 0));
    if (!check(theme.currentThemeId() == "builtin.default", "Dropdown selects Default")) return 1;
    bool quitRequested = false;
    QObject::connect(&session, &yaap::ApplicationSession::quitRequested, [&] { quitRequested = true; });
    QMetaObject::invokeMethod(quit, "clicked");
    if (!check(quitRequested && !trayIcon->isVisible(), "Quit Now quits and removes tray")
        || !check(warnings.isEmpty(), "No QML warnings")) return 1;
    std::cout << "PASS: Settings sections, theme dropdown, Custom editing, runtime behavior, Quit Now\n";
    return 0;
}
