#include "mods/ThemeManager.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>
#include <QSettings>

#include <cmath>
#include <limits>

namespace yaap {
namespace {

constexpr qint64 maximumThemeBytes = 256 * 1024;
constexpr qint64 maximumBackgroundImageBytes = 8 * 1024 * 1024;
constexpr qint64 maximumBackgroundRasterPixels = 32 * 1024 * 1024;
constexpr int maximumBackgroundRasterDimension = 8'192;
constexpr qreal minimumHueShiftDegrees = -180.0;
constexpr qreal maximumHueShiftDegrees = 180.0;
constexpr int maximumLayoutInset = 256;
constexpr int maximumHorizontalLayoutInsetSum = 400;
constexpr int maximumVerticalLayoutInsetSum = 280;

bool readColor(const QJsonObject& palette, const char* key, QColor& output, QString& error)
{
    const QColor candidate{palette.value(QLatin1String{key}).toString()};
    if (!candidate.isValid()) {
        error = "Theme palette contains an invalid " + QString::fromLatin1(key) + " color.";
        return false;
    }
    output = candidate;
    return true;
}

bool readLayoutInteger(const QJsonObject& object,
    const char* key,
    const int minimum,
    const int maximum,
    int& output,
    QString& error)
{
    const auto value = object.value(QLatin1String{key});
    if (value.isUndefined()) {
        return true;
    }
    const auto number = value.toDouble(std::numeric_limits<double>::quiet_NaN());
    if (!value.isDouble() || !std::isfinite(number) || number != std::floor(number)
        || number < minimum || number > maximum) {
        error = "Theme layout " + QString::fromLatin1(key)
            + " must be a bounded whole number.";
        return false;
    }
    output = static_cast<int>(number);
    return true;
}

QString hueShiftSettingsKey(const QString& themeId)
{
    return "mods/themeSettings/" + themeId + "/spectrumHueShiftDegrees";
}

QColor hueShifted(const QColor& source, const qreal degrees)
{
    if (qFuzzyIsNull(degrees)) {
        return source;
    }
    auto shifted = source.toHsl();
    auto hue = shifted.hslHueF();
    if (hue < 0.0) {
        return source;
    }
    hue = std::fmod(hue + degrees / 360.0, 1.0);
    if (hue < 0.0) {
        hue += 1.0;
    }
    shifted.setHslF(hue, shifted.hslSaturationF(), shifted.lightnessF(), shifted.alphaF());
    return shifted.toRgb();
}

bool readBackgroundImage(const QString& packageRoot,
    const QString& relativePath,
    QUrl& output,
    QString& error)
{
    if (relativePath.trimmed().isEmpty() || QDir::isAbsolutePath(relativePath)
        || !QUrl{relativePath}.scheme().isEmpty()) {
        error = "Theme background image must be a non-empty package-relative path.";
        return false;
    }

    const auto canonicalRoot = QFileInfo{packageRoot}.canonicalFilePath();
    const QFileInfo candidate{QDir{canonicalRoot}.filePath(relativePath)};
    const auto canonicalCandidate = candidate.canonicalFilePath();
    const auto rootPrefix = QDir::cleanPath(canonicalRoot) + '/';
#ifdef _WIN32
    constexpr auto pathCaseSensitivity = Qt::CaseInsensitive;
#else
    constexpr auto pathCaseSensitivity = Qt::CaseSensitive;
#endif
    if (canonicalRoot.isEmpty() || canonicalCandidate.isEmpty() || !candidate.isFile()
        || (!QDir::cleanPath(canonicalCandidate).startsWith(
                rootPrefix, pathCaseSensitivity)
            && canonicalCandidate.compare(canonicalRoot, pathCaseSensitivity) != 0)) {
        error = "Theme background image escapes the package or does not name a file.";
        return false;
    }
    if (candidate.size() <= 0 || candidate.size() > maximumBackgroundImageBytes) {
        error = "Theme background image must be between 1 byte and 8 MiB.";
        return false;
    }

    const auto suffix = candidate.suffix().toLower();
    if (suffix != "png" && suffix != "svg") {
        error = "Theme background image must use PNG or SVG format.";
        return false;
    }
    QImageReader reader{canonicalCandidate};
    reader.setDecideFormatFromContent(true);
    if (!reader.canRead()) {
        error = "Theme background image could not be decoded: " + reader.errorString();
        return false;
    }
    const auto detectedFormat = QString::fromLatin1(reader.format()).toLower();
    if (detectedFormat != suffix) {
        error = "Theme background image extension does not match its contents.";
        return false;
    }
    if (detectedFormat == "png") {
        const auto size = reader.size();
        if (!size.isValid() || size.width() > maximumBackgroundRasterDimension
            || size.height() > maximumBackgroundRasterDimension
            || static_cast<qint64>(size.width()) * size.height()
                > maximumBackgroundRasterPixels) {
            error = "Theme PNG dimensions exceed the supported decoding bounds.";
            return false;
        }
    }
    output = QUrl::fromLocalFile(canonicalCandidate);
    return true;
}

} // namespace

ThemeManager::ThemeManager(QObject* parent)
    : QObject(parent)
{
    m_themes.insert(m_currentThemeId, m_current);
}

void ThemeManager::resetAvailableThemes()
{
    const auto builtIn = m_themes.value("builtin.default", ThemeData{});
    m_themes.clear();
    m_themes.insert("builtin.default", builtIn);
    if (m_currentThemeId != "builtin.default") {
        m_currentThemeId = "builtin.default";
        m_current = builtIn;
        emit themeChanged();
    }
}

bool ThemeManager::registerTheme(const ModManifest& manifest, QString& error)
{
    if (!manifest.hasKind(ModKind::Theme)) {
        error = "Mod does not declare a theme.";
        return false;
    }
    ThemeData data;
    const auto packageRoot = manifest.packageRoot.isEmpty()
        ? QFileInfo{manifest.theme.dataPath}.absolutePath()
        : manifest.packageRoot;
    if (!readThemeFile(manifest.theme.dataPath, packageRoot, data, error)) {
        return false;
    }
    m_themes.insert(manifest.id, std::move(data));
    return true;
}

bool ThemeManager::selectTheme(const QString& modId, QString& error)
{
    const auto iterator = m_themes.constFind(modId);
    if (iterator == m_themes.cend()) {
        error = "Theme is not available: " + modId;
        return false;
    }
    m_currentThemeId = modId;
    m_current = iterator.value();
    QSettings settings;
    if (m_current.spectrumHueShiftAdjustable) {
        bool valid = false;
        const auto savedDegrees = settings.value(hueShiftSettingsKey(modId),
            m_current.spectrumHueShiftDegrees).toDouble(&valid);
        if (valid && std::isfinite(savedDegrees)) {
            m_current.spectrumHueShiftDegrees = qBound(minimumHueShiftDegrees,
                savedDegrees, maximumHueShiftDegrees);
        }
    }
    settings.setValue("mods/currentTheme", modId);
    emit themeChanged();
    return true;
}

QString ThemeManager::currentThemeId() const { return m_currentThemeId; }
QColor ThemeManager::windowTop() const { return m_current.windowTop; }
QColor ThemeManager::windowBottom() const { return m_current.windowBottom; }
QColor ThemeManager::surface() const { return m_current.surface; }
QColor ThemeManager::primaryText() const { return m_current.primaryText; }
QColor ThemeManager::secondaryText() const { return m_current.secondaryText; }
QColor ThemeManager::accent() const { return m_current.accent; }
QColor ThemeManager::error() const { return m_current.error; }
int ThemeManager::cornerRadius() const noexcept { return m_current.cornerRadius; }
int ThemeManager::spacing() const noexcept { return m_current.spacing; }
int ThemeManager::controlAreaLeftInset() const noexcept
{
    return m_current.controlAreaLeftInset;
}
int ThemeManager::controlAreaTopInset() const noexcept
{
    return m_current.controlAreaTopInset;
}
int ThemeManager::controlAreaRightInset() const noexcept
{
    return m_current.controlAreaRightInset;
}
int ThemeManager::controlAreaBottomInset() const noexcept
{
    return m_current.controlAreaBottomInset;
}
int ThemeManager::closeButtonRightInset() const noexcept
{
    return m_current.closeButtonRightInset;
}
int ThemeManager::closeButtonTopInset() const noexcept
{
    return m_current.closeButtonTopInset;
}
int ThemeManager::closeButtonWidth() const noexcept
{
    return m_current.closeButtonWidth;
}
int ThemeManager::closeButtonHeight() const noexcept
{
    return m_current.closeButtonHeight;
}
QUrl ThemeManager::backgroundImageSource() const { return m_current.backgroundImageSource; }
QString ThemeManager::backgroundImageFit() const { return m_current.backgroundImageFit; }
QString ThemeManager::backgroundImageAlignment() const
{
    return m_current.backgroundImageAlignment;
}
qreal ThemeManager::backgroundImageOpacity() const noexcept
{
    return m_current.backgroundImageOpacity;
}
bool ThemeManager::backgroundImageShapesWindow() const noexcept
{
    return m_current.backgroundImageShapesWindow;
}
QString ThemeManager::backgroundEffect() const { return m_current.backgroundEffect; }
int ThemeManager::miniPlayerWidth() const noexcept { return m_current.miniPlayerWidth; }
int ThemeManager::miniPlayerHeight() const noexcept { return m_current.miniPlayerHeight; }
int ThemeManager::miniControlAreaLeftInset() const noexcept
{
    return m_current.miniControlAreaLeftInset;
}
int ThemeManager::miniControlAreaTopInset() const noexcept
{
    return m_current.miniControlAreaTopInset;
}
int ThemeManager::miniControlAreaRightInset() const noexcept
{
    return m_current.miniControlAreaRightInset;
}
int ThemeManager::miniControlAreaBottomInset() const noexcept
{
    return m_current.miniControlAreaBottomInset;
}
int ThemeManager::miniWindowControlsRightInset() const noexcept
{
    return m_current.miniWindowControlsRightInset;
}
int ThemeManager::miniWindowControlsTopInset() const noexcept
{
    return m_current.miniWindowControlsTopInset;
}
int ThemeManager::miniWindowControlWidth() const noexcept
{
    return m_current.miniWindowControlWidth;
}
int ThemeManager::miniWindowControlHeight() const noexcept
{
    return m_current.miniWindowControlHeight;
}
int ThemeManager::miniWindowControlSpacing() const noexcept
{
    return m_current.miniWindowControlSpacing;
}
QUrl ThemeManager::miniBackgroundImageSource() const
{
    return m_current.miniBackgroundImageSource;
}
QString ThemeManager::miniBackgroundImageFit() const
{
    return m_current.miniBackgroundImageFit;
}
QString ThemeManager::miniBackgroundImageAlignment() const
{
    return m_current.miniBackgroundImageAlignment;
}
qreal ThemeManager::miniBackgroundImageOpacity() const noexcept
{
    return m_current.miniBackgroundImageOpacity;
}
bool ThemeManager::miniBackgroundImageShapesWindow() const noexcept
{
    return m_current.miniBackgroundImageShapesWindow;
}
int ThemeManager::spectrumColumns() const noexcept { return m_current.spectrumColumns; }
bool ThemeManager::spectrumMirror() const noexcept { return m_current.spectrumMirror; }
qreal ThemeManager::spectrumOpacity() const noexcept { return m_current.spectrumOpacity; }
int ThemeManager::spectrumAttackMilliseconds() const noexcept
{
    return m_current.spectrumAttackMilliseconds;
}
int ThemeManager::spectrumReleaseMilliseconds() const noexcept
{
    return m_current.spectrumReleaseMilliseconds;
}
QColor ThemeManager::spectrumGradientStart() const
{
    return hueShifted(m_current.spectrumGradientStart, m_current.spectrumHueShiftDegrees);
}
QColor ThemeManager::spectrumGradientMiddle() const
{
    return hueShifted(m_current.spectrumGradientMiddle, m_current.spectrumHueShiftDegrees);
}
QColor ThemeManager::spectrumGradientEnd() const
{
    return hueShifted(m_current.spectrumGradientEnd, m_current.spectrumHueShiftDegrees);
}
bool ThemeManager::spectrumHueShiftAdjustable() const noexcept
{
    return m_current.spectrumHueShiftAdjustable;
}
qreal ThemeManager::spectrumHueShiftDegrees() const noexcept
{
    return m_current.spectrumHueShiftDegrees;
}
void ThemeManager::setSpectrumHueShiftDegrees(const qreal degrees)
{
    if (!m_current.spectrumHueShiftAdjustable || !std::isfinite(degrees)) {
        return;
    }
    const auto bounded = qBound(minimumHueShiftDegrees, degrees, maximumHueShiftDegrees);
    if (qFuzzyCompare(m_current.spectrumHueShiftDegrees + 181.0, bounded + 181.0)) {
        return;
    }
    m_current.spectrumHueShiftDegrees = bounded;
    m_themes[m_currentThemeId].spectrumHueShiftDegrees = bounded;
    QSettings{}.setValue(hueShiftSettingsKey(m_currentThemeId), bounded);
    emit themeChanged();
}

bool ThemeManager::readThemeFile(const QString& path,
    const QString& packageRoot,
    ThemeData& data,
    QString& error)
{
    QFile file{path};
    if (!file.open(QIODevice::ReadOnly) || file.size() <= 0 || file.size() > maximumThemeBytes) {
        error = "Could not read a theme file of at most 256 KiB.";
        return false;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = "Invalid theme JSON: " + parseError.errorString();
        return false;
    }
    const auto root = document.object();
    if (root.value("schemaVersion").toInt(-1) != 1) {
        error = "Unsupported theme schema version.";
        return false;
    }
    const auto palette = root.value("palette").toObject();
    if (!readColor(palette, "windowTop", data.windowTop, error)
        || !readColor(palette, "windowBottom", data.windowBottom, error)
        || !readColor(palette, "surface", data.surface, error)
        || !readColor(palette, "primaryText", data.primaryText, error)
        || !readColor(palette, "secondaryText", data.secondaryText, error)
        || !readColor(palette, "accent", data.accent, error)
        || !readColor(palette, "error", data.error, error)) {
        return false;
    }
    const auto metrics = root.value("metrics").toObject();
    const auto cornerRadius = metrics.value("cornerRadius").toInt(-1);
    const auto spacing = metrics.value("spacing").toInt(-1);
    if (cornerRadius < 0 || cornerRadius > 64 || spacing < 1 || spacing > 64) {
        error = "Theme metrics are outside supported bounds.";
        return false;
    }
    data.cornerRadius = cornerRadius;
    data.spacing = spacing;
    data.spectrumGradientStart = data.accent;
    data.spectrumGradientMiddle = data.secondaryText;
    data.spectrumGradientEnd = data.accent;

    const auto layoutValue = root.value("layout");
    if (!layoutValue.isUndefined()) {
        if (!layoutValue.isObject()) {
            error = "Theme layout declaration must be an object.";
            return false;
        }
        const auto layout = layoutValue.toObject();
        const auto controlAreaValue = layout.value("controlArea");
        if (!controlAreaValue.isUndefined()) {
            if (!controlAreaValue.isObject()) {
                error = "Theme control area declaration must be an object.";
                return false;
            }
            const auto controlArea = controlAreaValue.toObject();
            if (!readLayoutInteger(controlArea, "leftInset", 0,
                    maximumLayoutInset, data.controlAreaLeftInset, error)
                || !readLayoutInteger(controlArea, "topInset", 0,
                    maximumLayoutInset, data.controlAreaTopInset, error)
                || !readLayoutInteger(controlArea, "rightInset", 0,
                    maximumLayoutInset, data.controlAreaRightInset, error)
                || !readLayoutInteger(controlArea, "bottomInset", 0,
                    maximumLayoutInset, data.controlAreaBottomInset, error)) {
                return false;
            }
            if (data.controlAreaLeftInset + data.controlAreaRightInset
                    > maximumHorizontalLayoutInsetSum
                || data.controlAreaTopInset + data.controlAreaBottomInset
                    > maximumVerticalLayoutInsetSum) {
                error = "Theme control area leaves too little usable window space.";
                return false;
            }
        }

        const auto closeButtonValue = layout.value("closeButton");
        if (!closeButtonValue.isUndefined()) {
            if (!closeButtonValue.isObject()) {
                error = "Theme close-button declaration must be an object.";
                return false;
            }
            const auto closeButton = closeButtonValue.toObject();
            if (!readLayoutInteger(closeButton, "rightInset", 0,
                    maximumLayoutInset, data.closeButtonRightInset, error)
                || !readLayoutInteger(closeButton, "topInset", 0,
                    maximumLayoutInset, data.closeButtonTopInset, error)
                || !readLayoutInteger(closeButton, "width", 28, 96,
                    data.closeButtonWidth, error)
                || !readLayoutInteger(closeButton, "height", 24, 72,
                    data.closeButtonHeight, error)) {
                return false;
            }
        }
    }

    const auto background = root.value("background").toObject();
    const auto imageValue = background.value("image");
    if (!imageValue.isUndefined()) {
        if (!imageValue.isObject()) {
            error = "Theme background image declaration must be an object.";
            return false;
        }
        const auto image = imageValue.toObject();
        const auto assetValue = image.value("asset");
        const auto fitValue = image.value("fit");
        const auto alignmentValue = image.value("alignment");
        const auto opacityValue = image.value("opacity");
        const auto windowShapeValue = image.value("windowShape");
        if (!assetValue.isString()
            || (!fitValue.isUndefined() && !fitValue.isString())
            || (!alignmentValue.isUndefined() && !alignmentValue.isString())
            || (!opacityValue.isUndefined() && !opacityValue.isDouble())
            || (!windowShapeValue.isUndefined() && !windowShapeValue.isBool())) {
            error = "Theme background image properties have invalid types.";
            return false;
        }
        data.backgroundImageFit = fitValue.toString(data.backgroundImageFit);
        data.backgroundImageAlignment = alignmentValue.toString(
            data.backgroundImageAlignment);
        data.backgroundImageOpacity = opacityValue.toDouble(data.backgroundImageOpacity);
        data.backgroundImageShapesWindow = windowShapeValue.toBool(false);
        static const QSet<QString> supportedFits{
            "preserveAspectFit", "preserveAspectCrop", "stretch"};
        static const QSet<QString> supportedAlignments{"center", "top", "top-left",
            "top-right", "left", "right", "bottom", "bottom-left", "bottom-right"};
        if (!supportedFits.contains(data.backgroundImageFit)
            || !supportedAlignments.contains(data.backgroundImageAlignment)
            || data.backgroundImageOpacity < 0.0 || data.backgroundImageOpacity > 1.0) {
            error = "Theme background image properties are outside supported values.";
            return false;
        }
        if (!readBackgroundImage(packageRoot, assetValue.toString(),
                data.backgroundImageSource, error)) {
            return false;
        }
    }

    const auto backgroundEffect = background.value("effect").toString("none");
    if (backgroundEffect != "none" && backgroundEffect != "waves"
        && backgroundEffect != "spectrum" && backgroundEffect != "paperPlanes") {
        error = "Theme background effect is not supported.";
        return false;
    }
    data.backgroundEffect = backgroundEffect;
    if (backgroundEffect == "spectrum") {
        const auto parametersValue = background.value("parameters");
        if (!parametersValue.isUndefined() && !parametersValue.isObject()) {
            error = "Spectrum background parameters must be an object.";
            return false;
        }
        const auto parameters = parametersValue.toObject();
        const auto columnsValue = parameters.value("columns");
        const auto mirrorValue = parameters.value("mirror");
        const auto opacityValue = parameters.value("opacity");
        const auto attackValue = parameters.value("attackMilliseconds");
        const auto releaseValue = parameters.value("releaseMilliseconds");
        const auto gradientStartValue = parameters.value("gradientStart");
        const auto gradientMiddleValue = parameters.value("gradientMiddle");
        const auto gradientEndValue = parameters.value("gradientEnd");
        const auto hueShiftAdjustableValue = parameters.value("hueShiftAdjustable");
        if ((!columnsValue.isUndefined() && !columnsValue.isDouble())
            || (!mirrorValue.isUndefined() && !mirrorValue.isBool())
            || (!opacityValue.isUndefined() && !opacityValue.isDouble())
            || (!attackValue.isUndefined() && !attackValue.isDouble())
            || (!releaseValue.isUndefined() && !releaseValue.isDouble())
            || (!gradientStartValue.isUndefined() && !gradientStartValue.isString())
            || (!gradientMiddleValue.isUndefined() && !gradientMiddleValue.isString())
            || (!gradientEndValue.isUndefined() && !gradientEndValue.isString())
            || (!hueShiftAdjustableValue.isUndefined()
                && !hueShiftAdjustableValue.isBool())) {
            error = "Spectrum background parameters have invalid types.";
            return false;
        }
        const auto containsFractionalInteger = [](const QJsonValue& value) {
            return value.isDouble() && value.toDouble() != std::floor(value.toDouble());
        };
        if (containsFractionalInteger(columnsValue)
            || containsFractionalInteger(attackValue)
            || containsFractionalInteger(releaseValue)) {
            error = "Spectrum column and timing parameters must be whole numbers.";
            return false;
        }

        data.spectrumColumns = columnsValue.toInt(data.spectrumColumns);
        data.spectrumMirror = mirrorValue.toBool(data.spectrumMirror);
        data.spectrumOpacity = opacityValue.toDouble(data.spectrumOpacity);
        data.spectrumAttackMilliseconds = attackValue.toInt(
            data.spectrumAttackMilliseconds);
        data.spectrumReleaseMilliseconds = releaseValue.toInt(
            data.spectrumReleaseMilliseconds);
        data.spectrumHueShiftAdjustable = hueShiftAdjustableValue.toBool(false);
        const auto readGradientColor = [&error](const QJsonValue& value,
                                               QColor& output,
                                               const char* name) {
            if (value.isUndefined()) {
                return true;
            }
            const QColor color{value.toString()};
            if (!color.isValid()) {
                error = "Spectrum background contains an invalid "
                    + QString::fromLatin1(name) + " color.";
                return false;
            }
            output = color;
            return true;
        };
        if (!readGradientColor(gradientStartValue,
                data.spectrumGradientStart, "gradientStart")
            || !readGradientColor(gradientMiddleValue,
                data.spectrumGradientMiddle, "gradientMiddle")
            || !readGradientColor(gradientEndValue,
                data.spectrumGradientEnd, "gradientEnd")) {
            return false;
        }
        if (data.spectrumColumns < 8 || data.spectrumColumns > 48
            || data.spectrumOpacity < 0.02 || data.spectrumOpacity > 0.80
            || data.spectrumAttackMilliseconds < 0
            || data.spectrumAttackMilliseconds > 1'000
            || data.spectrumReleaseMilliseconds < 0
            || data.spectrumReleaseMilliseconds > 3'000) {
            error = "Spectrum background parameters are outside supported bounds.";
            return false;
        }
    }

    const auto miniPlayerValue = root.value("miniPlayer");
    if (!miniPlayerValue.isUndefined()) {
        if (!miniPlayerValue.isObject()) {
            error = "Theme miniplayer declaration must be an object.";
            return false;
        }
        const auto miniPlayer = miniPlayerValue.toObject();
        const auto sizeValue = miniPlayer.value("size");
        if (!sizeValue.isUndefined()) {
            if (!sizeValue.isObject()) {
                error = "Theme miniplayer size must be an object.";
                return false;
            }
            const auto size = sizeValue.toObject();
            if (!readLayoutInteger(size, "width", 480, 480,
                    data.miniPlayerWidth, error)
                || !readLayoutInteger(size, "height", 112, 112,
                    data.miniPlayerHeight, error)) {
                return false;
            }
        }

        const auto miniLayoutValue = miniPlayer.value("layout");
        if (!miniLayoutValue.isUndefined()) {
            if (!miniLayoutValue.isObject()) {
                error = "Theme miniplayer layout must be an object.";
                return false;
            }
            const auto miniLayout = miniLayoutValue.toObject();
            const auto controlAreaValue = miniLayout.value("controlArea");
            if (!controlAreaValue.isUndefined()) {
                if (!controlAreaValue.isObject()) {
                    error = "Theme miniplayer control area must be an object.";
                    return false;
                }
                const auto controlArea = controlAreaValue.toObject();
                if (!readLayoutInteger(controlArea, "leftInset", 0,
                        maximumLayoutInset, data.miniControlAreaLeftInset, error)
                    || !readLayoutInteger(controlArea, "topInset", 0,
                        maximumLayoutInset, data.miniControlAreaTopInset, error)
                    || !readLayoutInteger(controlArea, "rightInset", 0,
                        maximumLayoutInset, data.miniControlAreaRightInset, error)
                    || !readLayoutInteger(controlArea, "bottomInset", 0,
                        maximumLayoutInset, data.miniControlAreaBottomInset, error)) {
                    return false;
                }
            }

            const auto controlsValue = miniLayout.value("windowControls");
            if (!controlsValue.isUndefined()) {
                if (!controlsValue.isObject()) {
                    error = "Theme miniplayer window controls must be an object.";
                    return false;
                }
                const auto controls = controlsValue.toObject();
                if (!readLayoutInteger(controls, "rightInset", 0,
                        maximumLayoutInset, data.miniWindowControlsRightInset, error)
                    || !readLayoutInteger(controls, "topInset", 0,
                        maximumLayoutInset, data.miniWindowControlsTopInset, error)
                    || !readLayoutInteger(controls, "buttonWidth", 28, 72,
                        data.miniWindowControlWidth, error)
                    || !readLayoutInteger(controls, "buttonHeight", 24, 56,
                        data.miniWindowControlHeight, error)
                    || !readLayoutInteger(controls, "spacing", 0, 32,
                        data.miniWindowControlSpacing, error)) {
                    return false;
                }
            }
        }

        const auto minimumContentWidth = 160;
        const auto minimumContentHeight = 48;
        const auto controlsWidth = data.miniWindowControlWidth * 2
            + data.miniWindowControlSpacing;
        if (data.miniControlAreaLeftInset + data.miniControlAreaRightInset
                > data.miniPlayerWidth - minimumContentWidth
            || data.miniControlAreaTopInset + data.miniControlAreaBottomInset
                > data.miniPlayerHeight - minimumContentHeight
            || data.miniWindowControlsRightInset + controlsWidth
                > data.miniPlayerWidth
            || data.miniWindowControlsTopInset + data.miniWindowControlHeight
                > data.miniPlayerHeight) {
            error = "Theme miniplayer layout leaves too little usable space.";
            return false;
        }

        const auto miniBackgroundValue = miniPlayer.value("background");
        if (!miniBackgroundValue.isUndefined()) {
            if (!miniBackgroundValue.isObject()) {
                error = "Theme miniplayer background must be an object.";
                return false;
            }
            const auto miniBackground = miniBackgroundValue.toObject();
            const auto effectValue = miniBackground.value("effect");
            if (!effectValue.isUndefined() && effectValue.toString("invalid") != "none") {
                error = "Theme miniplayer background effects are not supported.";
                return false;
            }
            const auto miniImageValue = miniBackground.value("image");
            if (!miniImageValue.isUndefined()) {
                if (!miniImageValue.isObject()) {
                    error = "Theme miniplayer background image must be an object.";
                    return false;
                }
                const auto image = miniImageValue.toObject();
                const auto assetValue = image.value("asset");
                const auto fitValue = image.value("fit");
                const auto alignmentValue = image.value("alignment");
                const auto opacityValue = image.value("opacity");
                const auto windowShapeValue = image.value("windowShape");
                if (!assetValue.isString()
                    || (!fitValue.isUndefined() && !fitValue.isString())
                    || (!alignmentValue.isUndefined() && !alignmentValue.isString())
                    || (!opacityValue.isUndefined() && !opacityValue.isDouble())
                    || (!windowShapeValue.isUndefined() && !windowShapeValue.isBool())) {
                    error = "Theme miniplayer background image properties have invalid types.";
                    return false;
                }
                data.miniBackgroundImageFit = fitValue.toString(
                    data.miniBackgroundImageFit);
                data.miniBackgroundImageAlignment = alignmentValue.toString(
                    data.miniBackgroundImageAlignment);
                data.miniBackgroundImageOpacity = opacityValue.toDouble(
                    data.miniBackgroundImageOpacity);
                data.miniBackgroundImageShapesWindow = windowShapeValue.toBool(false);
                static const QSet<QString> supportedFits{
                    "preserveAspectFit", "preserveAspectCrop", "stretch"};
                static const QSet<QString> supportedAlignments{"center", "top",
                    "top-left", "top-right", "left", "right", "bottom",
                    "bottom-left", "bottom-right"};
                if (!supportedFits.contains(data.miniBackgroundImageFit)
                    || !supportedAlignments.contains(
                        data.miniBackgroundImageAlignment)
                    || data.miniBackgroundImageOpacity < 0.0
                    || data.miniBackgroundImageOpacity > 1.0) {
                    error = "Theme miniplayer background image properties are outside supported values.";
                    return false;
                }
                if (!readBackgroundImage(packageRoot, assetValue.toString(),
                        data.miniBackgroundImageSource, error)) {
                    return false;
                }
            }
        }
    }
    return true;
}

} // namespace yaap
