#pragma once

#include <QObject>
#include <QPoint>
#include <QRect>
#include <QSettings>
#include <QSize>

namespace yaap {

class WindowPresentationController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(PresentationMode mode READ mode NOTIFY modeChanged)
    Q_PROPERTY(bool miniPlayer READ isMiniPlayer NOTIFY modeChanged)
    Q_PROPERTY(QRect normalGeometry READ normalGeometry NOTIFY normalGeometryChanged)
    Q_PROPERTY(QPoint miniPlayerPosition READ miniPlayerPosition
        NOTIFY miniPlayerPositionChanged)
    Q_PROPERTY(QRect initialGeometry READ initialGeometry CONSTANT)

public:
    enum class PresentationMode {
        Normal,
        MiniPlayer,
    };
    Q_ENUM(PresentationMode)

    explicit WindowPresentationController(QObject* parent = nullptr);

    [[nodiscard]] PresentationMode mode() const noexcept;
    [[nodiscard]] bool isMiniPlayer() const noexcept;
    [[nodiscard]] QRect normalGeometry() const noexcept;
    [[nodiscard]] QPoint miniPlayerPosition() const noexcept;
    [[nodiscard]] QRect initialGeometry() const;

    Q_INVOKABLE QRect availableGeometryForWindow(
        const QRect& windowGeometry) const;
    Q_INVOKABLE QRect normalGeometryFor(const QRect& availableGeometry) const;
    Q_INVOKABLE QRect miniPlayerGeometryFor(const QRect& availableGeometry) const;
    Q_INVOKABLE void enterMiniPlayer(
        const QRect& currentGeometry, const QRect& availableGeometry);
    Q_INVOKABLE void restoreFullPlayer();
    Q_INVOKABLE void recordNormalGeometry(
        const QRect& geometry, const QRect& availableGeometry);
    Q_INVOKABLE void recordMiniPlayerPosition(
        const QPoint& position, const QRect& availableGeometry);
    Q_INVOKABLE void resetToDefaults();

    [[nodiscard]] static QSize defaultNormalSize() noexcept;
    [[nodiscard]] static QSize minimumNormalSize() noexcept;
    [[nodiscard]] static QSize defaultMiniPlayerSize() noexcept;

signals:
    void modeChanged();
    void normalGeometryChanged();
    void miniPlayerPositionChanged();

private:
    void load();
    void persistMode();
    [[nodiscard]] QRect availableGeometryFor(const QPoint& point) const;
    [[nodiscard]] static QRect centered(const QSize& size, const QRect& available);
    [[nodiscard]] static QRect clampGeometry(
        const QRect& geometry, const QSize& minimumSize, const QRect& available);
    [[nodiscard]] static bool isPlausibleNormalGeometry(const QRect& geometry);

    QSettings m_settings;
    PresentationMode m_mode{PresentationMode::Normal};
    QRect m_normalGeometry{{0, 0}, defaultNormalSize()};
    QPoint m_miniPlayerPosition;
    bool m_hasNormalGeometry{};
    bool m_hasMiniPlayerPosition{};
};

} // namespace yaap
