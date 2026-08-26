#pragma once

#include <QObject>
#include <QTimer>

class QQuickWindow;

namespace yaap {

class ThemeManager;

class WindowShapeController final : public QObject {
    Q_OBJECT

public:
    WindowShapeController(
        ThemeManager& themes, QQuickWindow& window, QObject* parent = nullptr);

private:
    void scheduleUpdate();
    void applyShape();

    ThemeManager& m_themes;
    QQuickWindow& m_window;
    QTimer m_updateTimer;
};

} // namespace yaap
