#include "app/WindowPresentationController.hpp"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QVariant>

#include <algorithm>

namespace yaap {
namespace {

constexpr int settingsSchemaVersion = 1;
constexpr auto schemaVersionKey = "window/presentation-schema-version";
constexpr auto presentationModeKey = "window/presentation-mode-v1";
constexpr auto normalGeometryKey = "window/normal-geometry-v1";
constexpr auto miniPlayerPositionKey = "window/miniplayer-position-v1";
constexpr int maximumPersistedDimension = 16'384;

QRect fallbackAvailableGeometry()
{
    return {0, 0, 1'920, 1'080};
}

} // namespace

WindowPresentationController::WindowPresentationController(QObject* parent)
    : QObject(parent)
{
    load();
}

WindowPresentationController::PresentationMode
WindowPresentationController::mode() const noexcept
{
    return m_mode;
}

bool WindowPresentationController::isMiniPlayer() const noexcept
{
    return m_mode == PresentationMode::MiniPlayer;
}

QRect WindowPresentationController::normalGeometry() const noexcept
{
    return m_normalGeometry;
}

QPoint WindowPresentationController::miniPlayerPosition() const noexcept
{
    return m_miniPlayerPosition;
}

QRect WindowPresentationController::initialGeometry() const
{
    if (isMiniPlayer()) {
        const auto desiredCenter = m_hasMiniPlayerPosition
            ? m_miniPlayerPosition + QPoint{defaultMiniPlayerSize().width() / 2,
                  defaultMiniPlayerSize().height() / 2}
            : m_normalGeometry.center();
        return miniPlayerGeometryFor(availableGeometryFor(desiredCenter));
    }
    return normalGeometryFor(availableGeometryFor(m_normalGeometry.center()));
}

QRect WindowPresentationController::availableGeometryForWindow(
    const QRect& windowGeometry) const
{
    return availableGeometryFor(windowGeometry.isValid()
        ? windowGeometry.center() : m_normalGeometry.center());
}

QRect WindowPresentationController::normalGeometryFor(
    const QRect& availableGeometry) const
{
    const auto available = availableGeometry.isValid()
        ? availableGeometry : fallbackAvailableGeometry();
    const auto candidate = m_hasNormalGeometry
            && isPlausibleNormalGeometry(m_normalGeometry)
        ? m_normalGeometry : centered(defaultNormalSize(), available);
    return clampGeometry(candidate, minimumNormalSize(), available);
}

QRect WindowPresentationController::miniPlayerGeometryFor(
    const QRect& availableGeometry) const
{
    const auto available = availableGeometry.isValid()
        ? availableGeometry : fallbackAvailableGeometry();
    const auto candidate = m_hasMiniPlayerPosition
        ? QRect{m_miniPlayerPosition, defaultMiniPlayerSize()}
        : centered(defaultMiniPlayerSize(), available);
    return clampGeometry(candidate, defaultMiniPlayerSize(), available);
}

void WindowPresentationController::enterMiniPlayer(
    const QRect& currentGeometry, const QRect& availableGeometry)
{
    if (isMiniPlayer()) {
        return;
    }
    recordNormalGeometry(currentGeometry, availableGeometry);
    const auto available = availableGeometry.isValid()
        ? availableGeometry : fallbackAvailableGeometry();
    const auto compactGeometry = clampGeometry(
        QRect{currentGeometry.topLeft(), defaultMiniPlayerSize()},
        defaultMiniPlayerSize(), available);
    if (!m_hasMiniPlayerPosition
        || m_miniPlayerPosition != compactGeometry.topLeft()) {
        // Enter compact mode beside the content the user is looking at. A stale
        // compact position on another part of a large/multi-monitor desktop can
        // otherwise make the window appear to have vanished. The resulting
        // position is still persisted for compact-mode startup.
        m_miniPlayerPosition = compactGeometry.topLeft();
        m_hasMiniPlayerPosition = true;
        m_settings.setValue(miniPlayerPositionKey, m_miniPlayerPosition);
        m_settings.sync();
        emit miniPlayerPositionChanged();
    }
    m_mode = PresentationMode::MiniPlayer;
    persistMode();
    emit modeChanged();
}

void WindowPresentationController::restoreFullPlayer()
{
    if (!isMiniPlayer()) {
        return;
    }
    m_mode = PresentationMode::Normal;
    persistMode();
    emit modeChanged();
}

void WindowPresentationController::recordNormalGeometry(
    const QRect& geometry, const QRect& availableGeometry)
{
    if (!isPlausibleNormalGeometry(geometry)) {
        return;
    }
    const auto bounded = clampGeometry(geometry, minimumNormalSize(),
        availableGeometry.isValid() ? availableGeometry
                                    : fallbackAvailableGeometry());
    if (m_hasNormalGeometry && bounded == m_normalGeometry) {
        return;
    }
    m_normalGeometry = bounded;
    m_hasNormalGeometry = true;
    m_settings.setValue(normalGeometryKey, m_normalGeometry);
    m_settings.sync();
    emit normalGeometryChanged();
}

void WindowPresentationController::recordMiniPlayerPosition(
    const QPoint& position, const QRect& availableGeometry)
{
    const auto bounded = clampGeometry(
        QRect{position, defaultMiniPlayerSize()}, defaultMiniPlayerSize(),
        availableGeometry.isValid() ? availableGeometry
                                    : fallbackAvailableGeometry()).topLeft();
    if (m_hasMiniPlayerPosition && bounded == m_miniPlayerPosition) {
        return;
    }
    m_miniPlayerPosition = bounded;
    m_hasMiniPlayerPosition = true;
    m_settings.setValue(miniPlayerPositionKey, m_miniPlayerPosition);
    m_settings.sync();
    emit miniPlayerPositionChanged();
}

void WindowPresentationController::resetToDefaults()
{
    const auto modeChangedValue = m_mode != PresentationMode::Normal;
    const auto geometryChanged = m_normalGeometry != QRect{{0, 0}, defaultNormalSize()};
    const auto positionChanged = m_hasMiniPlayerPosition;
    m_mode = PresentationMode::Normal;
    m_normalGeometry = QRect{{0, 0}, defaultNormalSize()};
    m_miniPlayerPosition = {};
    m_hasNormalGeometry = false;
    m_hasMiniPlayerPosition = false;
    m_settings.setValue(schemaVersionKey, settingsSchemaVersion);
    m_settings.setValue(presentationModeKey, static_cast<int>(m_mode));
    m_settings.remove(normalGeometryKey);
    m_settings.remove(miniPlayerPositionKey);
    m_settings.sync();
    if (modeChangedValue) {
        emit modeChanged();
    }
    if (geometryChanged) {
        emit normalGeometryChanged();
    }
    if (positionChanged) {
        emit miniPlayerPositionChanged();
    }
}

QSize WindowPresentationController::defaultNormalSize() noexcept { return {900, 560}; }
QSize WindowPresentationController::minimumNormalSize() noexcept { return {680, 420}; }
QSize WindowPresentationController::defaultMiniPlayerSize() noexcept { return {480, 112}; }

void WindowPresentationController::load()
{
    const auto schema = m_settings.value(schemaVersionKey, settingsSchemaVersion).toInt();
    if (schema != settingsSchemaVersion) {
        resetToDefaults();
        return;
    }
    m_settings.setValue(schemaVersionKey, settingsSchemaVersion);

    const auto storedMode = m_settings.value(presentationModeKey,
        static_cast<int>(PresentationMode::Normal)).toInt();
    m_mode = storedMode == static_cast<int>(PresentationMode::MiniPlayer)
        ? PresentationMode::MiniPlayer : PresentationMode::Normal;

    const auto storedGeometry = m_settings.value(normalGeometryKey);
    if (storedGeometry.canConvert<QRect>()) {
        const auto geometry = storedGeometry.toRect();
        if (isPlausibleNormalGeometry(geometry)) {
            m_normalGeometry = geometry;
            m_hasNormalGeometry = true;
        }
    }

    const auto storedPosition = m_settings.value(miniPlayerPositionKey);
    if (storedPosition.canConvert<QPoint>()) {
        m_miniPlayerPosition = storedPosition.toPoint();
        m_hasMiniPlayerPosition = true;
    }
}

void WindowPresentationController::persistMode()
{
    m_settings.setValue(schemaVersionKey, settingsSchemaVersion);
    m_settings.setValue(presentationModeKey, static_cast<int>(m_mode));
    m_settings.sync();
}

QRect WindowPresentationController::availableGeometryFor(const QPoint& point) const
{
    if (qobject_cast<QGuiApplication*>(QCoreApplication::instance()) != nullptr) {
        const auto screens = QGuiApplication::screens();
        for (const auto* screen : screens) {
            if (screen != nullptr && screen->availableGeometry().contains(point)) {
                return screen->availableGeometry();
            }
        }
        if (const auto* primary = QGuiApplication::primaryScreen(); primary != nullptr) {
            return primary->availableGeometry();
        }
    }
    return fallbackAvailableGeometry();
}

QRect WindowPresentationController::centered(
    const QSize& size, const QRect& available)
{
    const auto boundedSize = QSize{std::min(size.width(), available.width()),
        std::min(size.height(), available.height())};
    return {available.x() + (available.width() - boundedSize.width()) / 2,
        available.y() + (available.height() - boundedSize.height()) / 2,
        boundedSize.width(), boundedSize.height()};
}

QRect WindowPresentationController::clampGeometry(const QRect& geometry,
    const QSize& minimumSize,
    const QRect& available)
{
    if (!available.isValid()) {
        return geometry;
    }
    const auto minimumWidth = std::min(minimumSize.width(), available.width());
    const auto minimumHeight = std::min(minimumSize.height(), available.height());
    const auto width = std::clamp(geometry.width(), minimumWidth, available.width());
    const auto height = std::clamp(geometry.height(), minimumHeight, available.height());
    const auto maximumX = available.x() + available.width() - width;
    const auto maximumY = available.y() + available.height() - height;
    return {std::clamp(geometry.x(), available.x(), maximumX),
        std::clamp(geometry.y(), available.y(), maximumY), width, height};
}

bool WindowPresentationController::isPlausibleNormalGeometry(const QRect& geometry)
{
    return geometry.isValid()
        && geometry.width() >= minimumNormalSize().width()
        && geometry.height() >= minimumNormalSize().height()
        && geometry.width() <= maximumPersistedDimension
        && geometry.height() <= maximumPersistedDimension;
}

} // namespace yaap
