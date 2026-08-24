#pragma once

#include "audio/MiniaudioOutput.hpp"
#include "core/PlaybackState.hpp"

#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <thread>

namespace yaap {

class PlayerController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QString stateName READ stateName NOTIFY stateChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(qint64 positionMilliseconds READ positionMilliseconds NOTIFY positionChanged)
    Q_PROPERTY(qint64 durationMilliseconds READ durationMilliseconds NOTIFY durationChanged)
    Q_PROPERTY(double progress READ progress NOTIFY positionChanged)
    Q_PROPERTY(bool hasAudio READ hasAudio NOTIFY controlsChanged)
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY controlsChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY controlsChanged)

public:
    explicit PlayerController(QObject* parent = nullptr);
    ~PlayerController() override;

    PlayerController(const PlayerController&) = delete;
    PlayerController& operator=(const PlayerController&) = delete;

    [[nodiscard]] QString title() const;
    [[nodiscard]] QString stateName() const;
    [[nodiscard]] QString errorMessage() const;
    [[nodiscard]] qint64 positionMilliseconds() const noexcept;
    [[nodiscard]] qint64 durationMilliseconds() const noexcept;
    [[nodiscard]] double progress() const noexcept;
    [[nodiscard]] bool hasAudio() const noexcept;
    [[nodiscard]] bool isPlaying() const noexcept;
    [[nodiscard]] bool isLoading() const noexcept;

    Q_INVOKABLE void openFile(const QUrl& url);
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stop();

signals:
    void titleChanged();
    void stateChanged();
    void errorMessageChanged();
    void positionChanged();
    void durationChanged();
    void controlsChanged();

private:
    void cancelDecode();
    void setState(PlaybackState state);
    void setError(QString message);
    void updatePosition();

    PlaybackStateMachine m_stateMachine;
    MiniaudioOutput m_output;
    QTimer m_positionTimer;
    std::jthread m_decodeThread;
    std::atomic<std::uint64_t> m_generation{0};
    QString m_title{"No track selected"};
    QString m_errorMessage;
    qint64 m_positionMilliseconds{};
    qint64 m_durationMilliseconds{};
};

} // namespace yaap
