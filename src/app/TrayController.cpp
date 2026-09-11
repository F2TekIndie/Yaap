#include "app/TrayController.hpp"
#include "app/ApplicationSession.hpp"

namespace yaap {
TrayController::TrayController(ApplicationSession& session, const QIcon& icon, QObject* parent)
    : QObject(parent), m_session(session), m_icon(icon, this)
{
    m_icon.setObjectName("yaapTrayIcon");
    m_icon.setToolTip("Yaap");
    m_menu.addAction(tr("Open"), &session, &ApplicationSession::activate);
    m_menu.addAction(tr("Open miniplayer"), &session, &ApplicationSession::activateMiniPlayer);
    m_menu.addSeparator();
    m_menu.addAction(tr("Quit"), &session, &ApplicationSession::quit);
    // Qt's Linux tray backend exports this menu over D-Bus. DMS renders it.
    m_icon.setContextMenu(&m_menu);
    connect(&m_icon, &QSystemTrayIcon::activated, this,
        [&session](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
                session.activate();
        });
    connect(&session, &ApplicationSession::backgroundChanged,
        this, &TrayController::updateVisibility);
    connect(&session, &ApplicationSession::quitRequested, this, [this] {
        m_quitting = true;
        m_icon.hide();
    });
    updateVisibility();
}

void TrayController::updateVisibility()
{
    // Leave the icon registered while background mode is enabled, even when
    // the window is visible. Qt also re-registers it when the tray host returns.
    m_icon.setVisible(!m_quitting && m_session.keepPlayingInBackground());
}
}
