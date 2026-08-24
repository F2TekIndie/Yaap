#include "app/PlayerController.hpp"

#include "audio/FFmpegDecoder.hpp"

#include <QFileInfo>
#include <QMetaObject>
#include <QPointer>
#include <QDebug>

#include <memory>
#include <utility>

namespace yaap {

PlayerController::PlayerController(QObject* parent)
    : QObject(parent)
{
    // PROTOTYPE: Position/end state is polled from atomics. The streaming version
    // should publish coalesced playback snapshots from a dedicated control layer.
    m_positionTimer.setInterval(100);
    m_positionTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_positionTimer, &QTimer::timeout, this, &PlayerController::updatePosition);
    m_positionTimer.start();
}

PlayerController::~PlayerController()
{
    m_positionTimer.stop();
    m_output.stop();
    cancelDecode();
}

QString PlayerController::title() const
{
    return m_title;
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
    return m_output.hasAudio();
}

bool PlayerController::isPlaying() const noexcept
{
    return m_stateMachine.state() == PlaybackState::Playing;
}

bool PlayerController::isLoading() const noexcept
{
    return m_stateMachine.state() == PlaybackState::Loading;
}

void PlayerController::openFile(const QUrl& url)
{
    if (!url.isLocalFile()) {
        setError("The prototype currently accepts local files only.");
        return;
    }

    const auto localPath = url.toLocalFile();
    if (localPath.isEmpty()) {
        setError("No input file was selected.");
        return;
    }

    m_output.stop();
    cancelDecode();
    const auto generation = m_generation.fetch_add(1, std::memory_order_acq_rel) + 1;

    m_title = QFileInfo(localPath).completeBaseName();
    m_errorMessage.clear();
    m_positionMilliseconds = 0;
    m_durationMilliseconds = 0;
    emit titleChanged();
    emit errorMessageChanged();
    emit positionChanged();
    emit durationChanged();
    setState(PlaybackState::Loading);

    const std::filesystem::path sourcePath{localPath.toStdWString()};
    QPointer<PlayerController> guardedThis{this};
    m_decodeThread = std::jthread(
        [guardedThis, generation, sourcePath](const std::stop_token stopToken) {
            FFmpegDecoder decoder;
            auto sharedResult = std::make_shared<DecodeResult>(decoder.decode(sourcePath, stopToken));

            if (!guardedThis) {
                return;
            }

            QMetaObject::invokeMethod(
                guardedThis,
                [guardedThis, generation, sourcePath, sharedResult = std::move(sharedResult)]() mutable {
                    if (!guardedThis || generation != guardedThis->m_generation.load(std::memory_order_acquire)) {
                        return;
                    }
                    if (sharedResult->cancelled) {
                        return;
                    }
                    if (!sharedResult->succeeded()) {
                        guardedThis->setError(QString::fromUtf8(sharedResult->error));
                        return;
                    }

                    auto audio = std::move(*sharedResult->audio);
                    if (!audio.title.empty()) {
                        guardedThis->m_title = QString::fromUtf8(audio.title);
                        emit guardedThis->titleChanged();
                    } else if (guardedThis->m_title.isEmpty()) {
                        guardedThis->m_title = QString::fromStdWString(sourcePath.stem().wstring());
                        emit guardedThis->titleChanged();
                    }

                    std::string outputError;
                    if (!guardedThis->m_output.load(std::move(audio), outputError)) {
                        guardedThis->setError(QString::fromUtf8(outputError));
                        return;
                    }

                    guardedThis->m_durationMilliseconds = guardedThis->m_output.durationMilliseconds();
                    guardedThis->m_positionMilliseconds = 0;
                    emit guardedThis->durationChanged();
                    emit guardedThis->positionChanged();
                    guardedThis->setState(PlaybackState::Ready);
                },
                Qt::QueuedConnection);
        });
}

void PlayerController::play()
{
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
    if (m_stateMachine.state() != PlaybackState::Playing) {
        return;
    }
    m_output.pause();
    setState(PlaybackState::Paused);
}

void PlayerController::stop()
{
    if (!m_output.hasAudio()) {
        return;
    }
    m_output.stop();
    m_positionMilliseconds = 0;
    emit positionChanged();
    setState(PlaybackState::Stopped);
}

void PlayerController::cancelDecode()
{
    if (!m_decodeThread.joinable()) {
        return;
    }

    m_generation.fetch_add(1, std::memory_order_acq_rel);
    m_decodeThread.request_stop();
    // PROTOTYPE: joining here can briefly block the GUI if a local filesystem
    // operation stalls. The streaming worker will use asynchronous shutdown and
    // interruptible FFmpeg I/O callbacks with explicit timeouts.
    m_decodeThread.join();
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
    m_output.stop();
    m_errorMessage = std::move(message);
    emit errorMessageChanged();
    setState(PlaybackState::Error);
}

void PlayerController::updatePosition()
{
    const auto position = static_cast<qint64>(m_output.positionMilliseconds());
    if (position != m_positionMilliseconds) {
        m_positionMilliseconds = position;
        emit positionChanged();
    }

    if (m_stateMachine.state() == PlaybackState::Playing && m_output.isFinished()) {
        setState(PlaybackState::Finished);
    }
}

} // namespace yaap
