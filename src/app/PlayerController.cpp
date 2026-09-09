#include "app/PlayerController.hpp"

#include "audio/AudioAnalysisEngine.hpp"
#include "audio/FFmpegDecoder.hpp"

#include <QFileInfo>
#include <QByteArray>
#include <QDebug>
#include <QMetaObject>
#include <QPointer>
#include <QSettings>

#include <algorithm>
#include <memory>
#include <utility>

namespace yaap {
namespace {

[[nodiscard]] std::filesystem::path filesystemPath(const QString& path)
{
#ifdef _WIN32
    return std::filesystem::path{path.toStdWString()};
#else
    const auto encoded = path.toUtf8();
    return std::filesystem::path{
        std::string{encoded.constData(), static_cast<std::size_t>(encoded.size())}};
#endif
}

[[nodiscard]] QString metadataText(const std::string& value)
{
    const QByteArray bytes{value.data(), static_cast<qsizetype>(value.size())};
    auto decoded = QString::fromUtf8(bytes);
    if (decoded.contains(QChar::ReplacementCharacter)) {
        decoded = QString::fromLatin1(bytes);
    }
    return decoded;
}

} // namespace

PlayerController::PlayerController(QObject* parent)
    : PlayerController(nullptr, parent)
{
}

PlayerController::PlayerController(AudioAnalysisEngine& analysisEngine, QObject* parent)
    : PlayerController(&analysisEngine, parent)
{
}

PlayerController::PlayerController(AudioAnalysisEngine* analysisEngine, QObject* parent)
    : QObject(parent)
    , m_output(analysisEngine)
{
    QSettings settings;
    m_output.setVolume(static_cast<float>(
        std::clamp(settings.value("playback/volume", 0.8).toDouble(), 0.0, 1.0)));
    m_output.setMuted(settings.value("playback/muted", false).toBool());

    // PROTOTYPE: Position/end state is polled from atomics. The full playback
    // control layer should publish coalesced PlaybackSessionSnapshot updates.
    m_positionTimer.setInterval(100);
    m_positionTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_positionTimer, &QTimer::timeout, this, &PlayerController::updatePosition);
    m_positionTimer.start();
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, [this] {
        m_reconnectScheduled = false;
        startStream(StreamStartMode::AutoPlay, false);
    });
}

PlayerController::~PlayerController()
{
    shutdown();
}

void PlayerController::shutdown()
{
    if (m_shuttingDown) {
        return;
    }
    m_shuttingDown = true;
    m_positionTimer.stop();
    m_reconnectTimer.stop();
    m_reconnectScheduled = false;
    m_output.clear();
    cancelDecode();
    m_stream.reset();
}

QString PlayerController::title() const
{
    return m_title;
}

QString PlayerController::stationTitle() const { return m_sourceFallbackTitle; }
QString PlayerController::nowPlayingText() const { return m_nowPlayingText; }
QString PlayerController::nowPlayingArtist() const { return m_nowPlayingArtist; }
QString PlayerController::nowPlayingTitle() const { return m_nowPlayingTitle; }
QString PlayerController::nowPlayingAlbum() const { return m_nowPlayingAlbum; }
bool PlayerController::hasNowPlayingMetadata() const noexcept
{
    return !m_nowPlayingText.isEmpty();
}
bool PlayerController::nowPlayingMetadataStale() const noexcept
{
    return m_nowPlayingMetadataStale;
}

QString PlayerController::stateName() const
{
    return QString::fromLatin1(toString(m_stateMachine.state()));
}

QString PlayerController::errorMessage() const
{
    return m_errorMessage;
}

qint64 PlayerController::positionMilliseconds() const noexcept
{
    return m_positionMilliseconds;
}

qint64 PlayerController::durationMilliseconds() const noexcept
{
    return m_durationMilliseconds;
}

double PlayerController::progress() const noexcept
{
    if (m_durationMilliseconds <= 0) {
        return 0.0;
    }
    return static_cast<double>(m_positionMilliseconds) / static_cast<double>(m_durationMilliseconds);
}

bool PlayerController::hasAudio() const noexcept
{
    return m_output.hasAudio() || hasSource();
}

bool PlayerController::isPlaying() const noexcept
{
    return m_stateMachine.state() == PlaybackState::Playing;
}

bool PlayerController::isLoading() const noexcept
{
    return m_stateMachine.state() == PlaybackState::Loading;
}

bool PlayerController::isBuffering() const noexcept
{
    return m_stateMachine.state() == PlaybackState::Buffering;
}

qreal PlayerController::volume() const noexcept { return m_output.volume(); }
bool PlayerController::muted() const noexcept { return m_output.isMuted(); }

void PlayerController::openFile(const QUrl& url)
{
    if (!url.isLocalFile()) {
        setError("Open file accepts local URLs; use Open stream for HTTP(S) audio.");
        return;
    }

    const auto localPath = url.toLocalFile();
    if (localPath.isEmpty()) {
        setError("No input file was selected.");
        return;
    }

    m_sourcePath = filesystemPath(localPath);
    m_sourceUrl.clear();
    m_sourceIsNetwork = false;
    m_sourceIsLive = false;
    m_sourceFallbackTitle = QFileInfo(localPath).completeBaseName();
    startStream(StreamStartMode::Ready, true);
}

void PlayerController::openStream(const QUrl& url, const QString& title, const bool live)
{
    if (!url.isValid() || (url.scheme() != "http" && url.scheme() != "https")) {
        setError("A valid HTTP or HTTPS stream URL is required.");
        return;
    }
    m_sourceUrl = url.toString(QUrl::FullyEncoded).toStdString();
    m_sourcePath.clear();
    m_sourceIsNetwork = true;
    m_sourceIsLive = live;
    m_sourceFallbackTitle = title.trimmed().isEmpty() ? url.host() : title.trimmed();
    startStream(StreamStartMode::AutoPlay, true);
}

void PlayerController::openRadioPlaylist(const QUrl& url)
{
    m_radioPlaylistLoader.load(url, [this](RadioPlaylistResult result) {
        if (!result.succeeded()) {
            setError(std::move(result.error));
            return;
        }
        if (result.stations.empty()) {
            setError("The radio playlist contains no supported HTTP(S) stations.");
            return;
        }
        const auto& station = result.stations.front();
        openStream(station.streamUrl, station.name, true);
    });
}

void PlayerController::startStream(
    const StreamStartMode mode,
    const bool resetPresentation,
    const qint64 startPositionMilliseconds)
{
    if ((!m_sourceIsNetwork && m_sourcePath.empty())
        || (m_sourceIsNetwork && m_sourceUrl.empty())) {
        setError("No input file was selected.");
        return;
    }

    m_output.clear();
    cancelDecode();
    const auto generation = m_generation.fetch_add(1, std::memory_order_acq_rel) + 1;

    if (resetPresentation) {
        m_reconnectTimer.stop();
        m_reconnectAttempt = 0;
        m_reconnectScheduled = false;
        m_title = m_sourceFallbackTitle;
        m_nowPlayingText.clear();
        m_nowPlayingArtist.clear();
        m_nowPlayingTitle.clear();
        m_nowPlayingAlbum.clear();
        m_nowPlayingMetadataStale = false;
        emit titleChanged();
        emit nowPlayingChanged();
    }
    m_errorMessage.clear();
    m_positionMilliseconds = std::max<qint64>(startPositionMilliseconds, 0);
    m_durationMilliseconds = 0;
    m_lastUnderrunCount = 0;
    emit errorMessageChanged();
    emit positionChanged();
    emit durationChanged();
    setState(PlaybackState::Loading);

    auto stream = std::make_shared<PcmStream>();
    m_stream = stream;
    std::string outputError;
    if (!m_output.attach(stream, outputError)) {
        setError(QString::fromUtf8(outputError));
        return;
    }

    const auto sourcePath = m_sourcePath;
    const auto sourceUrl = m_sourceUrl;
    const auto sourceIsNetwork = m_sourceIsNetwork;
    QPointer<PlayerController> guardedThis{this};
    m_decodeThread = std::jthread(
        [guardedThis, generation, sourcePath, sourceUrl, sourceIsNetwork, stream, mode,
            startPositionMilliseconds](
            const std::stop_token stopToken) {
            FFmpegDecoder decoder;
            const auto readyCallback = [guardedThis, generation, stream, mode](
                                           const AudioStreamInfo& info) {
                if (!guardedThis) {
                    return;
                }

                QMetaObject::invokeMethod(
                    guardedThis,
                    [guardedThis, generation, stream, mode, info] {
                        if (!guardedThis
                            || generation != guardedThis->m_generation.load(std::memory_order_acquire)
                            || guardedThis->m_stream != stream) {
                            return;
                        }

                        if (!info.title.empty()) {
                            guardedThis->m_title = QString::fromUtf8(info.title);
                            emit guardedThis->titleChanged();
                        }
                        guardedThis->m_durationMilliseconds =
                            static_cast<qint64>(stream->durationMilliseconds());
                        guardedThis->m_positionMilliseconds =
                            static_cast<qint64>(stream->positionMilliseconds());
                        emit guardedThis->durationChanged();
                        emit guardedThis->positionChanged();
                        guardedThis->setState(PlaybackState::Ready);

                        if (mode == StreamStartMode::Stopped) {
                            guardedThis->setState(PlaybackState::Stopped);
                        } else if (mode == StreamStartMode::Paused) {
                            guardedThis->setState(PlaybackState::Paused);
                        } else if (mode == StreamStartMode::AutoPlay) {
                            guardedThis->play();
                        }
                    },
                    Qt::QueuedConnection);
            };
            const auto metadataCallback = [guardedThis, generation, stream](
                                              const NowPlayingMetadata& metadata) {
                if (!guardedThis) {
                    return;
                }
                QMetaObject::invokeMethod(
                    guardedThis,
                    [guardedThis, generation, stream, metadata] {
                        if (!guardedThis
                            || generation != guardedThis->m_generation.load(std::memory_order_acquire)
                            || guardedThis->m_stream != stream) {
                            return;
                        }
                        guardedThis->m_nowPlayingText = metadataText(metadata.displayText);
                        guardedThis->m_nowPlayingArtist = metadataText(metadata.artist);
                        guardedThis->m_nowPlayingTitle = metadataText(metadata.title);
                        guardedThis->m_nowPlayingAlbum = metadataText(metadata.album);
                        guardedThis->m_nowPlayingMetadataStale = false;
                        emit guardedThis->nowPlayingChanged();
                    },
                    Qt::QueuedConnection);
            };

            StreamOptions streamOptions;
            streamOptions.startPositionMilliseconds = startPositionMilliseconds;
            streamOptions.reconnectNetworkStream = sourceIsNetwork;
            auto sharedResult = std::make_shared<StreamDecodeResult>(sourceIsNetwork
                ? decoder.streamUrl(sourceUrl, *stream, readyCallback, metadataCallback,
                    std::move(streamOptions), stopToken)
                : decoder.streamFile(sourcePath, *stream, readyCallback, metadataCallback,
                    std::move(streamOptions), stopToken));

            if (!guardedThis) {
                return;
            }

            QMetaObject::invokeMethod(
                guardedThis,
                [guardedThis, generation, sharedResult = std::move(sharedResult)]() mutable {
                    if (!guardedThis || generation != guardedThis->m_generation.load(std::memory_order_acquire)) {
                        return;
                    }
                    if (sharedResult->cancelled) {
                        return;
                    }
                    if (!sharedResult->succeeded()) {
                        const auto error = QString::fromUtf8(sharedResult->error);
                        if (guardedThis->m_sourceIsLive) {
                            guardedThis->scheduleReconnect(error);
                        } else {
                            guardedThis->setError(error);
                        }
                        return;
                    }

                    const auto finalDuration =
                        static_cast<qint64>(guardedThis->m_output.durationMilliseconds());
                    if (finalDuration != guardedThis->m_durationMilliseconds) {
                        guardedThis->m_durationMilliseconds = finalDuration;
                        emit guardedThis->durationChanged();
                    }
                },
                Qt::QueuedConnection);
        });
}

void PlayerController::play()
{
    if (m_stateMachine.state() == PlaybackState::Finished
        || (m_stateMachine.state() == PlaybackState::Stopped
            && !m_output.hasAudio() && hasSource())) {
        // Replaying reopens the source because consumed frames are intentionally
        // absent from the bounded streaming ring buffer.
        startStream(StreamStartMode::AutoPlay, false);
        return;
    }

    if (!m_output.hasAudio()) {
        return;
    }

    std::string error;
    if (!m_output.play(error)) {
        setError(QString::fromUtf8(error));
        return;
    }
    setState(PlaybackState::Playing);
}

void PlayerController::pause()
{
    if (m_stateMachine.state() != PlaybackState::Playing
        && m_stateMachine.state() != PlaybackState::Buffering) {
        return;
    }
    m_output.pause();
    setState(PlaybackState::Paused);
}

void PlayerController::stop()
{
    if ((!m_sourceIsNetwork && m_sourcePath.empty())
        || (m_sourceIsNetwork && m_sourceUrl.empty())) {
        return;
    }

    m_reconnectTimer.stop();
    m_reconnectScheduled = false;
    m_output.clear();
    cancelDecode();
    m_stream.reset();
    m_positionMilliseconds = 0;
    m_durationMilliseconds = 0;
    emit positionChanged();
    emit durationChanged();
    setState(PlaybackState::Stopped);
}

void PlayerController::setVolume(const qreal volume)
{
    const auto bounded = std::clamp(volume, 0.0, 1.0);
    if (qFuzzyCompare(this->volume() + 1.0, bounded + 1.0)) {
        return;
    }
    m_output.setVolume(static_cast<float>(bounded));
    QSettings{}.setValue("playback/volume", bounded);
    emit volumeChanged();
}

void PlayerController::setMuted(const bool muted)
{
    if (this->muted() == muted) {
        return;
    }
    m_output.setMuted(muted);
    QSettings{}.setValue("playback/muted", muted);
    emit volumeChanged();
}

void PlayerController::toggleMuted()
{
    setMuted(!muted());
}

void PlayerController::seek(const qint64 positionMilliseconds)
{
    if ((!m_sourceIsNetwork && m_sourcePath.empty()) || m_sourceIsNetwork
        || m_durationMilliseconds <= 0) {
        return;
    }

    const auto target = std::clamp<qint64>(
        positionMilliseconds, 0, m_durationMilliseconds);
    const auto state = m_stateMachine.state();
    const auto mode = state == PlaybackState::Playing || state == PlaybackState::Buffering
        ? StreamStartMode::AutoPlay
        : state == PlaybackState::Paused
            ? StreamStartMode::Paused
            : StreamStartMode::Ready;
    startStream(mode, false, target);
}

void PlayerController::cancelDecode()
{
    if (!m_decodeThread.joinable()) {
        return;
    }

    m_generation.fetch_add(1, std::memory_order_acq_rel);
    m_decodeThread.request_stop();
    if (m_stream) {
        m_stream->interruptProducerWait();
    }
    m_taskReaper.retire(std::move(m_decodeThread));
}

void PlayerController::setState(const PlaybackState state)
{
    if (!m_stateMachine.transitionTo(state)) {
        qWarning() << "Rejected playback state transition from" << stateName()
                   << "to" << QString::fromLatin1(toString(state));
        return;
    }
    emit stateChanged();
    emit controlsChanged();
}

void PlayerController::setError(QString message)
{
    m_output.clear();
    cancelDecode();
    m_stream.reset();
    m_errorMessage = std::move(message);
    emit errorMessageChanged();
    setState(PlaybackState::Error);
}

bool PlayerController::hasSource() const noexcept
{
    return m_sourceIsNetwork ? !m_sourceUrl.empty() : !m_sourcePath.empty();
}

void PlayerController::updatePosition()
{
    const auto position = static_cast<qint64>(m_output.positionMilliseconds());
    if (position != m_positionMilliseconds) {
        m_positionMilliseconds = position;
        emit positionChanged();
    }

    const auto duration = static_cast<qint64>(m_output.durationMilliseconds());
    if (duration > 0 && duration != m_durationMilliseconds) {
        m_durationMilliseconds = duration;
        emit durationChanged();
    }

    if ((m_stateMachine.state() == PlaybackState::Playing
            || m_stateMachine.state() == PlaybackState::Buffering)
        && m_output.isFinished()) {
        m_output.pause();
        setState(PlaybackState::Finished);
        if (m_sourceIsLive) {
            scheduleReconnect("The stream ended.");
        }
        return;
    }

    if (m_stateMachine.state() == PlaybackState::Playing) {
        const auto underruns = m_output.underrunCount();
        if (underruns != m_lastUnderrunCount) {
            m_lastUnderrunCount = underruns;
            m_output.pause();
            setState(PlaybackState::Buffering);
        }
    } else if (m_stateMachine.state() == PlaybackState::Buffering
        && (m_output.bufferedMilliseconds() >= 250
            || (m_output.isEndOfStream() && m_output.bufferedMilliseconds() > 0))) {
        std::string error;
        if (!m_output.play(error)) {
            setError(QString::fromUtf8(error));
            return;
        }
        setState(PlaybackState::Playing);
    }
}

void PlayerController::scheduleReconnect(QString reason)
{
    if (!m_sourceIsLive || m_reconnectScheduled) {
        return;
    }
    m_output.pause();
    m_errorMessage = std::move(reason) + " Reconnecting…";
    if (!m_nowPlayingText.isEmpty()) {
        m_nowPlayingMetadataStale = true;
        emit nowPlayingChanged();
    }
    emit errorMessageChanged();
    if (m_stateMachine.state() != PlaybackState::Finished) {
        setState(PlaybackState::Buffering);
    }
    const auto delay = m_reconnectPolicy.delayForAttempt(m_reconnectAttempt++);
    m_reconnectScheduled = true;
    m_reconnectTimer.start(static_cast<int>(delay.count()));
}

} // namespace yaap
