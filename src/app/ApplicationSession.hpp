#pragma once

#include <QObject>
#include <QPointer>
#include <QWindow>

namespace yaap {

class ApplicationSession final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool keepPlayingInBackground READ keepPlayingInBackground
        WRITE setKeepPlayingInBackground NOTIFY backgroundChanged)
    Q_PROPERTY(bool backgroundAvailable READ backgroundAvailable CONSTANT)
public:
    explicit ApplicationSession(bool backgroundAvailable, QObject* parent = nullptr);
    void setWindow(QWindow* window) { m_window = window; }
    bool backgroundAvailable() const { return m_backgroundAvailable; }
    bool keepPlayingInBackground() const { return m_backgroundAvailable && m_background; }
    void setKeepPlayingInBackground(bool enabled);
    Q_INVOKABLE bool handleClose();
    Q_INVOKABLE void activate();
    Q_INVOKABLE void activateMiniPlayer();
    Q_INVOKABLE void quit();
signals:
    void backgroundChanged();
    void activationRequested();
    void miniPlayerActivationRequested();
    void quitRequested();
private:
    void showWindow();
    QPointer<QWindow> m_window;
    bool m_backgroundAvailable{};
    bool m_background{};
    bool m_quitting{};
};
}
