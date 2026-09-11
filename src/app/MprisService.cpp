#include "app/MprisService.hpp"
#include "app/ApplicationSession.hpp"
#include "app/PlayerController.hpp"

#include <QDBusMessage>
#include <QUrl>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace yaap {
namespace {
qlonglong microseconds(qint64 milliseconds)
{
    return std::clamp<qint64>(milliseconds, 0,
        std::numeric_limits<qlonglong>::max() / 1000) * 1000;
}
}

MprisRootAdaptor::MprisRootAdaptor(QObject* parent, ApplicationSession& session)
    : QDBusAbstractAdaptor(parent), m_session(session) {}
QStringList MprisRootAdaptor::supportedMimeTypes() const
{
    return {"audio/mpeg", "audio/flac", "audio/ogg", "audio/opus", "audio/wav",
        "audio/x-wav", "audio/aac", "audio/mp4", "audio/x-ms-wma"};
}
void MprisRootAdaptor::Raise() { m_session.activate(); }
void MprisRootAdaptor::Quit() { m_session.quit(); }

MprisPlayerAdaptor::MprisPlayerAdaptor(QObject* parent, PlayerController& player,
    QDBusConnection connection)
    : QDBusAbstractAdaptor(parent), m_player(player), m_connection(std::move(connection))
{
    connect(&player, &PlayerController::controlsChanged, this, &MprisPlayerAdaptor::publishChanges);
    connect(&player, &PlayerController::sourceChanged, this, &MprisPlayerAdaptor::publishChanges);
    connect(&player, &PlayerController::titleChanged, this, &MprisPlayerAdaptor::publishChanges);
    connect(&player, &PlayerController::nowPlayingChanged, this, &MprisPlayerAdaptor::publishChanges);
    connect(&player, &PlayerController::durationChanged, this, &MprisPlayerAdaptor::publishChanges);
    connect(&player, &PlayerController::volumeChanged, this, &MprisPlayerAdaptor::publishChanges);
    connect(&player, &PlayerController::seekCompleted, this,
        [this](qint64 position) { emit Seeked(microseconds(position)); });
    publishChanges();
}

QString MprisPlayerAdaptor::playbackStatus() const
{
    if (m_player.isPlaying() || m_player.isBuffering()) return "Playing";
    if (m_player.stateName() == "Paused" || m_player.stateName() == "Ready") return "Paused";
    return "Stopped";
}
QDBusObjectPath MprisPlayerAdaptor::trackId() const
{
    return QDBusObjectPath{m_player.hasSource()
        ? "/org/yaap/Yaap/track/" + QString::number(m_player.sourceRevision())
        : "/org/mpris/MediaPlayer2/TrackList/NoTrack"};
}
QVariantMap MprisPlayerAdaptor::metadata() const
{
    if (!m_player.hasSource()) return {};
    QVariantMap result{{"mpris:trackid", QVariant::fromValue(trackId())},
        {"xesam:title", m_player.nowPlayingTitle().isEmpty()
            ? m_player.title() : m_player.nowPlayingTitle()}};
    if (m_player.durationMilliseconds() > 0)
        result.insert("mpris:length", microseconds(m_player.durationMilliseconds()));
    if (!m_player.nowPlayingArtist().isEmpty())
        result.insert("xesam:artist", QStringList{m_player.nowPlayingArtist()});
    if (!m_player.nowPlayingAlbum().isEmpty())
        result.insert("xesam:album", m_player.nowPlayingAlbum());
    if (!m_player.artworkSource().isEmpty())
        result.insert("mpris:artUrl", m_player.artworkSource().toString(QUrl::FullyEncoded));
    // Provider stream URLs may contain credentials. Never publish them on D-Bus.
    return result;
}
double MprisPlayerAdaptor::volume() const { return m_player.muted() ? 0.0 : m_player.volume(); }
void MprisPlayerAdaptor::setVolume(double value)
{
    if (!std::isfinite(value)) return;
    m_player.setVolume(value);
    m_player.setMuted(false);
}
qlonglong MprisPlayerAdaptor::position() const { return microseconds(m_player.positionMilliseconds()); }
void MprisPlayerAdaptor::setRate(double value) { if (value == 0.0) Pause(); }
bool MprisPlayerAdaptor::canPlay() const
{
    return m_player.hasSource() && !m_player.isLoading() && m_player.stateName() != "Error";
}
bool MprisPlayerAdaptor::canSeek() const { return m_player.canSeek(); }
void MprisPlayerAdaptor::Pause() { if (canPlay()) m_player.pause(); }
void MprisPlayerAdaptor::PlayPause()
{
    if (m_player.isPlaying() || m_player.isBuffering()) Pause();
    else Play();
}
void MprisPlayerAdaptor::Stop() { m_player.stop(); }
void MprisPlayerAdaptor::Play() { if (canPlay() && !m_player.isPlaying()) m_player.play(); }
void MprisPlayerAdaptor::Seek(qlonglong offset)
{
    if (!canSeek()) return;
    const auto target = static_cast<long double>(position()) + offset;
    if (target > microseconds(m_player.durationMilliseconds())) { Next(); return; }
    m_player.seek(static_cast<qint64>(std::max(target, 0.0L) / 1000));
}
void MprisPlayerAdaptor::SetPosition(const QDBusObjectPath& id, qlonglong position)
{
    if (!canSeek() || id.path() != trackId().path() || position < 0
        || position > microseconds(m_player.durationMilliseconds())) return;
    m_player.seek(position / 1000);
}
void MprisPlayerAdaptor::OpenUri(const QString& uri)
{
    const QUrl url{uri};
    if (!url.isValid()) return;
    if (url.isLocalFile()) m_player.openFile(url, {}, true);
    else if (url.scheme() == "http" || url.scheme() == "https") m_player.openStream(url);
}
void MprisPlayerAdaptor::publishChanges()
{
    const QVariantMap current{{"PlaybackStatus", playbackStatus()}, {"Metadata", metadata()},
        {"Volume", volume()}, {"CanPlay", canPlay()}, {"CanPause", canPlay()}, {"CanSeek", canSeek()}};
    QVariantMap changed;
    for (auto it = current.cbegin(); it != current.cend(); ++it) {
        if (!m_previousProperties.contains(it.key()) || m_previousProperties.value(it.key()) != it.value())
            changed.insert(it.key(), it.value());
    }
    m_previousProperties = current;
    if (changed.isEmpty()) return;
    auto message = QDBusMessage::createSignal(MprisService::objectPath,
        "org.freedesktop.DBus.Properties", "PropertiesChanged");
    message << QString{"org.mpris.MediaPlayer2.Player"} << changed << QStringList{};
    m_connection.send(message);
}

MprisService::MprisService(PlayerController& player, ApplicationSession& session,
    QDBusConnection connection) : m_connection(std::move(connection))
{
    new MprisRootAdaptor(this, session);
    new MprisPlayerAdaptor(this, player, m_connection);
    m_registered = m_connection.registerObject(objectPath, this, QDBusConnection::ExportAdaptors);
}
MprisService::~MprisService()
{
    if (m_registered) m_connection.unregisterObject(objectPath);
}
}
