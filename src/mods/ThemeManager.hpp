#pragma once

#include "extension_api/ModManifest.hpp"

#include <QColor>
#include <QHash>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

namespace yaap {

class ThemeManager final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString dmsStatus READ dmsStatus NOTIFY dmsStatusChanged)
    Q_PROPERTY(QVariantList availableThemes READ availableThemes NOTIFY availableThemesChanged)
    Q_PROPERTY(QVariantList customFields READ customFields CONSTANT)
    Q_PROPERTY(QVariantMap customValues READ customValues NOTIFY themeChanged)
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
    Q_PROPERTY(int controlAreaLeftInset READ controlAreaLeftInset NOTIFY themeChanged)
    Q_PROPERTY(int controlAreaTopInset READ controlAreaTopInset NOTIFY themeChanged)
    Q_PROPERTY(int controlAreaRightInset READ controlAreaRightInset NOTIFY themeChanged)
    Q_PROPERTY(int controlAreaBottomInset READ controlAreaBottomInset NOTIFY themeChanged)
    Q_PROPERTY(int closeButtonRightInset READ closeButtonRightInset NOTIFY themeChanged)
    Q_PROPERTY(int closeButtonTopInset READ closeButtonTopInset NOTIFY themeChanged)
    Q_PROPERTY(int closeButtonWidth READ closeButtonWidth NOTIFY themeChanged)
    Q_PROPERTY(int closeButtonHeight READ closeButtonHeight NOTIFY themeChanged)
    Q_PROPERTY(QUrl backgroundImageSource READ backgroundImageSource NOTIFY themeChanged)
    Q_PROPERTY(QString backgroundImageFit READ backgroundImageFit NOTIFY themeChanged)
    Q_PROPERTY(QString backgroundImageAlignment READ backgroundImageAlignment NOTIFY themeChanged)
    Q_PROPERTY(qreal backgroundImageOpacity READ backgroundImageOpacity NOTIFY themeChanged)
    Q_PROPERTY(bool backgroundImageShapesWindow READ backgroundImageShapesWindow NOTIFY themeChanged)
    Q_PROPERTY(QString backgroundEffect READ backgroundEffect NOTIFY themeChanged)
    Q_PROPERTY(int miniPlayerWidth READ miniPlayerWidth NOTIFY themeChanged)
    Q_PROPERTY(int miniPlayerHeight READ miniPlayerHeight NOTIFY themeChanged)
    Q_PROPERTY(int miniControlAreaLeftInset READ miniControlAreaLeftInset NOTIFY themeChanged)
    Q_PROPERTY(int miniControlAreaTopInset READ miniControlAreaTopInset NOTIFY themeChanged)
    Q_PROPERTY(int miniControlAreaRightInset READ miniControlAreaRightInset NOTIFY themeChanged)
    Q_PROPERTY(int miniControlAreaBottomInset READ miniControlAreaBottomInset NOTIFY themeChanged)
    Q_PROPERTY(int miniWindowControlsRightInset READ miniWindowControlsRightInset NOTIFY themeChanged)
    Q_PROPERTY(int miniWindowControlsTopInset READ miniWindowControlsTopInset NOTIFY themeChanged)
    Q_PROPERTY(int miniWindowControlWidth READ miniWindowControlWidth NOTIFY themeChanged)
    Q_PROPERTY(int miniWindowControlHeight READ miniWindowControlHeight NOTIFY themeChanged)
    Q_PROPERTY(int miniWindowControlSpacing READ miniWindowControlSpacing NOTIFY themeChanged)
    Q_PROPERTY(QUrl miniBackgroundImageSource READ miniBackgroundImageSource NOTIFY themeChanged)
    Q_PROPERTY(QString miniBackgroundImageFit READ miniBackgroundImageFit NOTIFY themeChanged)
    Q_PROPERTY(QString miniBackgroundImageAlignment READ miniBackgroundImageAlignment NOTIFY themeChanged)
    Q_PROPERTY(qreal miniBackgroundImageOpacity READ miniBackgroundImageOpacity NOTIFY themeChanged)
    Q_PROPERTY(bool miniBackgroundImageShapesWindow READ miniBackgroundImageShapesWindow NOTIFY themeChanged)
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

    QString dmsStatus() const;
    QVariantList availableThemes() const;
    QVariantList customFields() const;
    QVariantMap customValues() const;
    Q_INVOKABLE QString useTheme(const QString& id);
    Q_INVOKABLE QString applyCustom(const QVariantMap& values);
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
    [[nodiscard]] int controlAreaLeftInset() const noexcept;
    [[nodiscard]] int controlAreaTopInset() const noexcept;
    [[nodiscard]] int controlAreaRightInset() const noexcept;
    [[nodiscard]] int controlAreaBottomInset() const noexcept;
    [[nodiscard]] int closeButtonRightInset() const noexcept;
    [[nodiscard]] int closeButtonTopInset() const noexcept;
    [[nodiscard]] int closeButtonWidth() const noexcept;
    [[nodiscard]] int closeButtonHeight() const noexcept;
    [[nodiscard]] QUrl backgroundImageSource() const;
    [[nodiscard]] QString backgroundImageFit() const;
    [[nodiscard]] QString backgroundImageAlignment() const;
    [[nodiscard]] qreal backgroundImageOpacity() const noexcept;
    [[nodiscard]] bool backgroundImageShapesWindow() const noexcept;
    [[nodiscard]] QString backgroundEffect() const;
    [[nodiscard]] int miniPlayerWidth() const noexcept;
    [[nodiscard]] int miniPlayerHeight() const noexcept;
    [[nodiscard]] int miniControlAreaLeftInset() const noexcept;
    [[nodiscard]] int miniControlAreaTopInset() const noexcept;
    [[nodiscard]] int miniControlAreaRightInset() const noexcept;
    [[nodiscard]] int miniControlAreaBottomInset() const noexcept;
    [[nodiscard]] int miniWindowControlsRightInset() const noexcept;
    [[nodiscard]] int miniWindowControlsTopInset() const noexcept;
    [[nodiscard]] int miniWindowControlWidth() const noexcept;
    [[nodiscard]] int miniWindowControlHeight() const noexcept;
    [[nodiscard]] int miniWindowControlSpacing() const noexcept;
    [[nodiscard]] QUrl miniBackgroundImageSource() const;
    [[nodiscard]] QString miniBackgroundImageFit() const;
    [[nodiscard]] QString miniBackgroundImageAlignment() const;
    [[nodiscard]] qreal miniBackgroundImageOpacity() const noexcept;
    [[nodiscard]] bool miniBackgroundImageShapesWindow() const noexcept;
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
    Q_INVOKABLE void commitSpectrumHueShift();

signals:
    void availableThemesChanged();
    void dmsStatusChanged();
    void themeChanged();
    void windowShapeChanged();

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
        int controlAreaLeftInset{36};
        int controlAreaTopInset{36};
        int controlAreaRightInset{36};
        int controlAreaBottomInset{36};
        int closeButtonRightInset{};
        int closeButtonTopInset{};
        int closeButtonWidth{44};
        int closeButtonHeight{36};
        QUrl backgroundImageSource;
        QString backgroundImageFit{"preserveAspectFit"};
        QString backgroundImageAlignment{"center"};
        qreal backgroundImageOpacity{1.0};
        bool backgroundImageShapesWindow{};
        QString backgroundEffect{"none"};
        int miniPlayerWidth{480};
        int miniPlayerHeight{112};
        int miniControlAreaLeftInset{16};
        int miniControlAreaTopInset{8};
        int miniControlAreaRightInset{96};
        int miniControlAreaBottomInset{8};
        int miniWindowControlsRightInset{8};
        int miniWindowControlsTopInset{8};
        int miniWindowControlWidth{36};
        int miniWindowControlHeight{32};
        int miniWindowControlSpacing{4};
        QUrl miniBackgroundImageSource;
        QString miniBackgroundImageFit{"preserveAspectFit"};
        QString miniBackgroundImageAlignment{"center"};
        qreal miniBackgroundImageOpacity{1.0};
        bool miniBackgroundImageShapesWindow{};
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
    QHash<QString, QString> m_themeNames;
    QString m_currentThemeId{"builtin.default"};
    ThemeData m_current;
    QTimer m_huePersistTimer;
    void refreshDmsTheme();
    QTimer m_dmsTimer;
    QString m_dmsStatus;
    QByteArray m_dmsSignature;
    ThemeData m_dmsTheme;
};

} // namespace yaap
