#pragma once

#include "extension_api/ModManifest.hpp"

#include <QColor>
#include <QHash>
#include <QObject>
#include <QString>
#include <QUrl>

namespace yaap {

class ThemeManager final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentThemeId READ currentThemeId NOTIFY themeChanged)
    Q_PROPERTY(QColor windowTop READ windowTop NOTIFY themeChanged)
    Q_PROPERTY(QColor windowBottom READ windowBottom NOTIFY themeChanged)
    Q_PROPERTY(QColor surface READ surface NOTIFY themeChanged)
    Q_PROPERTY(QColor primaryText READ primaryText NOTIFY themeChanged)
    Q_PROPERTY(QColor secondaryText READ secondaryText NOTIFY themeChanged)
    Q_PROPERTY(QColor accent READ accent NOTIFY themeChanged)
    Q_PROPERTY(QColor error READ error NOTIFY themeChanged)
    Q_PROPERTY(int cornerRadius READ cornerRadius NOTIFY themeChanged)
    Q_PROPERTY(int spacing READ spacing NOTIFY themeChanged)
    Q_PROPERTY(QUrl backgroundImageSource READ backgroundImageSource NOTIFY themeChanged)
    Q_PROPERTY(QString backgroundImageFit READ backgroundImageFit NOTIFY themeChanged)
    Q_PROPERTY(QString backgroundImageAlignment READ backgroundImageAlignment NOTIFY themeChanged)
    Q_PROPERTY(qreal backgroundImageOpacity READ backgroundImageOpacity NOTIFY themeChanged)
    Q_PROPERTY(bool backgroundImageShapesWindow READ backgroundImageShapesWindow NOTIFY themeChanged)
    Q_PROPERTY(QString backgroundEffect READ backgroundEffect NOTIFY themeChanged)
    Q_PROPERTY(int spectrumColumns READ spectrumColumns NOTIFY themeChanged)
    Q_PROPERTY(bool spectrumMirror READ spectrumMirror NOTIFY themeChanged)
    Q_PROPERTY(qreal spectrumOpacity READ spectrumOpacity NOTIFY themeChanged)
    Q_PROPERTY(int spectrumAttackMilliseconds READ spectrumAttackMilliseconds NOTIFY themeChanged)
    Q_PROPERTY(int spectrumReleaseMilliseconds READ spectrumReleaseMilliseconds NOTIFY themeChanged)
    Q_PROPERTY(QColor spectrumGradientStart READ spectrumGradientStart NOTIFY themeChanged)
    Q_PROPERTY(QColor spectrumGradientMiddle READ spectrumGradientMiddle NOTIFY themeChanged)
    Q_PROPERTY(QColor spectrumGradientEnd READ spectrumGradientEnd NOTIFY themeChanged)
    Q_PROPERTY(bool spectrumHueShiftAdjustable READ spectrumHueShiftAdjustable NOTIFY themeChanged)
    Q_PROPERTY(qreal spectrumHueShiftDegrees READ spectrumHueShiftDegrees WRITE setSpectrumHueShiftDegrees NOTIFY themeChanged)

public:
    explicit ThemeManager(QObject* parent = nullptr);

    void resetAvailableThemes();
    bool registerTheme(const ModManifest& manifest, QString& error);
    bool selectTheme(const QString& modId, QString& error);

    [[nodiscard]] QString currentThemeId() const;
    [[nodiscard]] QColor windowTop() const;
    [[nodiscard]] QColor windowBottom() const;
    [[nodiscard]] QColor surface() const;
    [[nodiscard]] QColor primaryText() const;
    [[nodiscard]] QColor secondaryText() const;
    [[nodiscard]] QColor accent() const;
    [[nodiscard]] QColor error() const;
    [[nodiscard]] int cornerRadius() const noexcept;
    [[nodiscard]] int spacing() const noexcept;
    [[nodiscard]] QUrl backgroundImageSource() const;
    [[nodiscard]] QString backgroundImageFit() const;
    [[nodiscard]] QString backgroundImageAlignment() const;
    [[nodiscard]] qreal backgroundImageOpacity() const noexcept;
    [[nodiscard]] bool backgroundImageShapesWindow() const noexcept;
    [[nodiscard]] QString backgroundEffect() const;
    [[nodiscard]] int spectrumColumns() const noexcept;
    [[nodiscard]] bool spectrumMirror() const noexcept;
    [[nodiscard]] qreal spectrumOpacity() const noexcept;
    [[nodiscard]] int spectrumAttackMilliseconds() const noexcept;
    [[nodiscard]] int spectrumReleaseMilliseconds() const noexcept;
    [[nodiscard]] QColor spectrumGradientStart() const;
    [[nodiscard]] QColor spectrumGradientMiddle() const;
    [[nodiscard]] QColor spectrumGradientEnd() const;
    [[nodiscard]] bool spectrumHueShiftAdjustable() const noexcept;
    [[nodiscard]] qreal spectrumHueShiftDegrees() const noexcept;
    void setSpectrumHueShiftDegrees(qreal degrees);

signals:
    void themeChanged();

private:
    struct ThemeData final {
        QColor windowTop{"#202838"};
        QColor windowBottom{"#101216"};
        QColor surface{"#171b22"};
        QColor primaryText{"#f4f6fa"};
        QColor secondaryText{"#aeb8ca"};
        QColor accent{"#80cbc4"};
        QColor error{"#ff8a80"};
        int cornerRadius{8};
        int spacing{12};
        QUrl backgroundImageSource;
        QString backgroundImageFit{"preserveAspectFit"};
        QString backgroundImageAlignment{"center"};
        qreal backgroundImageOpacity{1.0};
        bool backgroundImageShapesWindow{};
        QString backgroundEffect{"none"};
        int spectrumColumns{48};
        bool spectrumMirror{true};
        qreal spectrumOpacity{0.28};
        int spectrumAttackMilliseconds{45};
        int spectrumReleaseMilliseconds{220};
        QColor spectrumGradientStart{"#80cbc4"};
        QColor spectrumGradientMiddle{"#aeb8ca"};
        QColor spectrumGradientEnd{"#80cbc4"};
        bool spectrumHueShiftAdjustable{};
        qreal spectrumHueShiftDegrees{};
    };

    static bool readThemeFile(const QString& path,
        const QString& packageRoot,
        ThemeData& data,
        QString& error);

    QHash<QString, ThemeData> m_themes;
    QString m_currentThemeId{"builtin.default"};
    ThemeData m_current;
};

} // namespace yaap
