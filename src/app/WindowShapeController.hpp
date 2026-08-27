#pragma once

#include <QObject>
#include <QRegion>
#include <QSize>
#include <QTimer>
#include <QUrl>

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
    void invalidateShape();
    void applyShape();

    ThemeManager& m_themes;
    WindowPresentationController& m_presentation;
    QQuickWindow& m_window;
    QTimer m_updateTimer;
    QUrl m_cachedSource;
    QSize m_cachedWindowSize;
    QString m_cachedFit;
    QString m_cachedAlignment;
    QRegion m_cachedRegion;
    bool m_cachedMiniPlayer{};
    bool m_cacheValid{};
};

} // namespace yaap
