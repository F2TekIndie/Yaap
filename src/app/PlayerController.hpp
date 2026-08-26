#pragma once

#include "audio/MiniaudioOutput.hpp"
#include "core/AsyncTaskReaper.hpp"
#include "core/PlaybackState.hpp"
#include "radio/RadioPlaylist.hpp"
#include "radio/RadioPlaylistLoader.hpp"

#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <thread>

namespace yaap {

class AudioAnalysisEngine;

class PlayerController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QString stationTitle READ stationTitle NOTIFY titleChanged)
    Q_PROPERTY(QString nowPlayingText READ nowPlayingText NOTIFY nowPlayingChanged)
    Q_PROPERTY(QString nowPlayingArtist READ nowPlayingArtist NOTIFY nowPlayingChanged)
    Q_PROPERTY(QString nowPlayingTitle READ nowPlayingTitle NOTIFY nowPlayingChanged)
    Q_PROPERTY(QString nowPlayingAlbum READ nowPlayingAlbum NOTIFY nowPlayingChanged)
    Q_PROPERTY(bool hasNowPlayingMetadata READ hasNowPlayingMetadata NOTIFY nowPlayingChanged)
    Q_PROPERTY(bool nowPlayingMetadataStale READ nowPlayingMetadataStale NOTIFY nowPlayingChanged)
    Q_PROPERTY(QString stateName READ stateName NOTIFY stateChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(qint64 positionMilliseconds READ positionMilliseconds NOTIFY positionChanged)
    Q_PROPERTY(qint64 durationMilliseconds READ durationMilliseconds NOTIFY durationChanged)
    Q_PROPERTY(double progress READ progress NOTIFY positionChanged)
    Q_PROPERTY(bool hasAudio READ hasAudio NOTIFY controlsChanged)
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY controlsChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY controlsChanged)
    Q_PROPERTY(bool isBuffering READ isBuffering NOTIFY controlsChanged)

public:
    explicit PlayerController(QObject* parent = nullptr);
    explicit PlayerController(AudioAnalysisEngine& analysisEngine, QObject* parent = nullptr);
    ~PlayerController() override;

    PlayerController(const PlayerController&) = delete;
    PlayerController& operator=(const PlayerController&) = delete;

    [[nodiscard]] QString title() const;
    [[nodiscard]] QString stationTitle() const;
    [[nodiscard]] QString nowPlayingText() const;
    [[nodiscard]] QString nowPlayingArtist() const;
    [[nodiscard]] QString nowPlayingTitle() const;
    [[nodiscard]] QString nowPlayingAlbum() const;
    [[nodiscard]] bool hasNowPlayingMetadata() const noexcept;
    [[nodiscard]] bool nowPlayingMetadataStale() const noexcept;
    [[nodiscard]] QString stateName() const;
    [[nodiscard]] QString errorMessage() const;
    [[nodiscard]] qint64 positionMilliseconds() const noexcept;
    [[nodiscard]] qint64 durationMilliseconds() const noexcept;
    [[nodiscard]] double progress() const noexcept;
    [[nodiscard]] bool hasAudio() const noexcept;
    [[nodiscard]] bool isPlaying() const noexcept;
    [[nodiscard]] bool isLoading() const noexcept;
    [[nodiscard]] bool isBuffering() const noexcept;

    Q_INVOKABLE void openFile(const QUrl& url);
    Q_INVOKABLE void openStream(const QUrl& url, const QString& title = {});
    Q_INVOKABLE void openRadioPlaylist(const QUrl& url);
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void seek(qint64 positionMilliseconds);

    // Stops real-time output and requests cancellation of background work.
    // Safe to call repeatedly during application shutdown.
    void shutdown();

signals:
    void titleChanged();
    void nowPlayingChanged();
    void stateChanged();
    void errorMessageChanged();
    void positionChanged();
    void durationChanged();
    void controlsChanged();

private:
    PlayerController(AudioAnalysisEngine* analysisEngine, QObject* parent);

    enum class StreamStartMode {
        Ready,
        Stopped,
        Paused,
        AutoPlay,
    };

    void startStream(
        StreamStartMode mode,
        bool resetPresentation,
        qint64 startPositionMilliseconds = 0);
    void cancelDecode();
    void setState(PlaybackState state);
    void setError(QString message);
    void updatePosition();
    void scheduleReconnect(QString reason);

    PlaybackStateMachine m_stateMachine;
    MiniaudioOutput m_output;
    QTimer m_positionTimer;
    QTimer m_reconnectTimer;
    RadioPlaylistLoader m_radioPlaylistLoader;
    ReconnectPolicy m_reconnectPolicy;
    AsyncTaskReaper m_taskReaper;
    std::jthread m_decodeThread;
    std::shared_ptr<PcmStream> m_stream;
    std::atomic<std::uint64_t> m_generation{0};
    std::filesystem::path m_sourcePath;
    std::string m_sourceUrl;
    bool m_sourceIsNetwork{};
    QString m_sourceFallbackTitle;
    QString m_title{"No track selected"};
    QString m_nowPlayingText;
    QString m_nowPlayingArtist;
    QString m_nowPlayingTitle;
    QString m_nowPlayingAlbum;
    QString m_errorMessage;
    qint64 m_positionMilliseconds{};
    qint64 m_durationMilliseconds{};
    std::size_t m_lastUnderrunCount{};
    std::size_t m_reconnectAttempt{};
    bool m_reconnectScheduled{};
    bool m_nowPlayingMetadataStale{};
    bool m_shuttingDown{};
};

} // namespace yaap
