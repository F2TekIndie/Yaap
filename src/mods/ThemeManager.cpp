#include "mods/ThemeManager.hpp"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSettings>

namespace yaap {
namespace {

constexpr qint64 maximumThemeBytes = 256 * 1024;

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
    QSettings{}.setValue("mods/currentTheme", modId);
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

    const auto background = root.value("background").toObject();
    const auto backgroundEffect = background.value("effect").toString("none");
    if (backgroundEffect != "none" && backgroundEffect != "waves") {
        error = "Theme background effect is not supported.";
        return false;
    }
    data.backgroundEffect = backgroundEffect;
    return true;
}

} // namespace yaap
