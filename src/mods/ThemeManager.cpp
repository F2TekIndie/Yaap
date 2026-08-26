#include "mods/ThemeManager.hpp"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSettings>

#include <cmath>

namespace yaap {
namespace {

constexpr qint64 maximumThemeBytes = 256 * 1024;
constexpr qreal minimumHueShiftDegrees = -180.0;
constexpr qreal maximumHueShiftDegrees = 180.0;

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
    if (!readThemeFile(manifest.theme.dataPath, data, error)) {
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
QString ThemeManager::backgroundEffect() const { return m_current.backgroundEffect; }
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

bool ThemeManager::readThemeFile(const QString& path, ThemeData& data, QString& error)
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

    const auto background = root.value("background").toObject();
    const auto backgroundEffect = background.value("effect").toString("none");
    if (backgroundEffect != "none" && backgroundEffect != "waves"
        && backgroundEffect != "spectrum") {
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
    return true;
}

} // namespace yaap
