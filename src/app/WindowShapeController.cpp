#include "app/WindowShapeController.hpp"

#include "app/WindowPresentationController.hpp"
#include "mods/ThemeManager.hpp"

#include <QDebug>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QQuickWindow>
#include <QRegion>

namespace yaap {
namespace {

QSize renderedSize(const QSize& source, const QSize& window, const QString& fit)
{
    if (fit == "stretch") {
        return window;
    }
    auto result = source;
    result.scale(window, fit == "preserveAspectCrop"
            ? Qt::KeepAspectRatioByExpanding
            : Qt::KeepAspectRatio);
    return result;
}

QPoint alignedTopLeft(
    const QSize& rendered, const QSize& window, const QString& alignment)
{
    int x = (window.width() - rendered.width()) / 2;
    int y = (window.height() - rendered.height()) / 2;
    if (alignment == "left" || alignment == "top-left"
        || alignment == "bottom-left") {
        x = 0;
    } else if (alignment == "right" || alignment == "top-right"
        || alignment == "bottom-right") {
        x = window.width() - rendered.width();
    }
    if (alignment == "top" || alignment == "top-left"
        || alignment == "top-right") {
        y = 0;
    } else if (alignment == "bottom" || alignment == "bottom-left"
        || alignment == "bottom-right") {
        y = window.height() - rendered.height();
    }
    return {x, y};
}

QRegion alphaRegion(const QImage& image)
{
    constexpr int alphaThreshold = 8;
    QRegion region;
    for (int y = 0; y < image.height(); ++y) {
        const auto* pixels = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        int runStart = -1;
        for (int x = 0; x <= image.width(); ++x) {
            const auto included = x < image.width() && qAlpha(pixels[x]) >= alphaThreshold;
            if (included && runStart < 0) {
                runStart = x;
            } else if (!included && runStart >= 0) {
                region += QRect{runStart, y, x - runStart, 1};
                runStart = -1;
            }
        }
    }
    return region;
}

} // namespace

WindowShapeController::WindowShapeController(
    ThemeManager& themes,
    WindowPresentationController& presentation,
    QQuickWindow& window,
    QObject* parent)
    : QObject(parent)
    , m_themes(themes)
    , m_presentation(presentation)
    , m_window(window)
{
    m_updateTimer.setSingleShot(true);
    m_updateTimer.setInterval(75);
    connect(&m_updateTimer, &QTimer::timeout,
        this, &WindowShapeController::applyShape);
    connect(&m_themes, &ThemeManager::windowShapeChanged,
        this, &WindowShapeController::invalidateShape);
    connect(&m_presentation, &WindowPresentationController::modeChanged,
        this, [this] {
            // A presentation change can also resize the native window. Drop the
            // previous mask immediately so it cannot clip the transition frame.
            m_window.setMask({});
            scheduleUpdate();
        });
    const auto windowResized = [this] {
        m_window.setMask({});
        scheduleUpdate();
    };
    connect(&m_window, &QQuickWindow::widthChanged, this, windowResized);
    connect(&m_window, &QQuickWindow::heightChanged, this, windowResized);
    connect(&m_window, &QQuickWindow::devicePixelRatioChanged,
        this, &WindowShapeController::scheduleUpdate);
    // Main.qml applies the persisted startup geometry before engine.load()
    // returns. Apply the matching input mask before the initially hidden window
    // is shown to avoid a rectangular first frame on shaped themes.
    applyShape();
}

void WindowShapeController::scheduleUpdate()
{
    m_updateTimer.start();
}

void WindowShapeController::invalidateShape()
{
    m_cacheValid = false;
    m_window.setMask({});
    scheduleUpdate();
}

void WindowShapeController::applyShape()
{
    const auto miniPlayer = m_presentation.isMiniPlayer();
    const auto shapesWindow = miniPlayer
        ? m_themes.miniBackgroundImageShapesWindow()
        : m_themes.backgroundImageShapesWindow();
    if (!shapesWindow) {
        m_window.setMask({});
        return;
    }
    const auto source = miniPlayer
        ? m_themes.miniBackgroundImageSource()
        : m_themes.backgroundImageSource();
    const auto windowSize = m_window.size();
    if (!source.isLocalFile() || windowSize.isEmpty()) {
        m_window.setMask({});
        return;
    }

    QImageReader reader{source.toLocalFile()};
    const auto sourceSize = reader.size();
    if (!sourceSize.isValid()) {
        qWarning() << "Could not read shaped-window image size:" << reader.errorString();
        m_window.setMask({});
        return;
    }
    const auto fit = miniPlayer
        ? m_themes.miniBackgroundImageFit()
        : m_themes.backgroundImageFit();
    const auto alignment = miniPlayer
        ? m_themes.miniBackgroundImageAlignment()
        : m_themes.backgroundImageAlignment();
    if (m_cacheValid && m_cachedSource == source
        && m_cachedWindowSize == windowSize && m_cachedFit == fit
        && m_cachedAlignment == alignment && m_cachedMiniPlayer == miniPlayer) {
        m_window.setMask(m_cachedRegion);
        return;
    }
    const auto imageSize = renderedSize(sourceSize, windowSize, fit);
    reader.setScaledSize(imageSize);
    const auto image = reader.read();
    if (image.isNull()) {
        qWarning() << "Could not decode shaped-window image:" << reader.errorString();
        m_window.setMask({});
        return;
    }

    QImage canvas{windowSize, QImage::Format_ARGB32_Premultiplied};
    canvas.fill(Qt::transparent);
    QPainter painter{&canvas};
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(alignedTopLeft(imageSize, windowSize, alignment), image);
    painter.end();

    const auto region = alphaRegion(canvas);
    if (region.isEmpty()) {
        qWarning() << "Shaped-window image produced an empty input region.";
        m_window.setMask({});
        return;
    }
    m_cachedSource = source;
    m_cachedWindowSize = windowSize;
    m_cachedFit = fit;
    m_cachedAlignment = alignment;
    m_cachedMiniPlayer = miniPlayer;
    m_cachedRegion = region;
    m_cacheValid = true;
    m_window.setMask(region);
}

} // namespace yaap
