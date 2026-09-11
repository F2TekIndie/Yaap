#include "app/ApplicationSession.hpp"

#include <QGuiApplication>
#include <QSettings>
#include <QTimer>

namespace yaap {
ApplicationSession::ApplicationSession(bool backgroundAvailable, QObject* parent)
    : QObject(parent), m_backgroundAvailable(backgroundAvailable)
    , m_background(QSettings{}.value("application/keepPlayingInBackground", true).toBool())
{
}

void ApplicationSession::setKeepPlayingInBackground(bool enabled)
{
    if (m_background == enabled) return;
    m_background = enabled;
    QSettings{}.setValue("application/keepPlayingInBackground", enabled);
    emit backgroundChanged();
}

bool ApplicationSession::handleClose()
{
    if (m_quitting) return true;
    if (!keepPlayingInBackground()) {
        quit();
        return true;
    }
    // Persistent dialogs are separate top-level windows. Hide those as well.
    for (auto* window : QGuiApplication::topLevelWindows()) window->hide();
    return false;
}

void ApplicationSession::activate()
{
    if (!m_window) return;
    emit activationRequested();
    showWindow();
}

void ApplicationSession::activateMiniPlayer()
{
    if (!m_window) return;
    emit miniPlayerActivationRequested();
    showWindow();
}

void ApplicationSession::showWindow()
{
    if (m_window->visibility() == QWindow::Minimized) m_window->showNormal();
    else m_window->show();
    m_window->raise();
    m_window->requestActivate();
}

void ApplicationSession::quit()
{
    if (m_quitting) return;
    m_quitting = true;
    emit quitRequested();
    // Let the incoming D-Bus method return before releasing the service.
    QTimer::singleShot(0, qApp, &QCoreApplication::quit);
}
}
