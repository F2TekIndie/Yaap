#pragma once

#include <QObject>
#include <QTimer>

class QQuickWindow;

namespace yaap {

class ThemeManager;
class WindowPresentationController;

class WindowShapeController final : public QObject {
    Q_OBJECT

public:
    WindowShapeController(
        ThemeManager& themes,
        WindowPresentationController& presentation,
        QQuickWindow& window,
        QObject* parent = nullptr);

private:
    void scheduleUpdate();
    void applyShape();

    ThemeManager& m_themes;
    WindowPresentationController& m_presentation;
    QQuickWindow& m_window;
    QTimer m_updateTimer;
};

} // namespace yaap
