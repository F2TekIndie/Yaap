#pragma once

#include <QObject>
#include <QMenu>
#include <QSystemTrayIcon>

namespace yaap {
class ApplicationSession;

class TrayController final : public QObject {
public:
    TrayController(ApplicationSession& session, const QIcon& icon, QObject* parent = nullptr);

private:
    ApplicationSession& m_session;
    QMenu m_menu;
    QSystemTrayIcon m_icon;
    bool m_quitting{};
    void updateVisibility();
};
}
