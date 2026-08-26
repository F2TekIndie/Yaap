#pragma once

#include <QByteArray>
#include <QString>
#include <QUrl>

#include <chrono>
#include <cstddef>
#include <optional>
#include <vector>

namespace yaap {

struct RadioStation final {
    QString name;
    QUrl streamUrl;
    QString directoryUuid;
};

struct RadioPlaylistResult final {
    std::vector<RadioStation> stations;
    QString error;

    [[nodiscard]] bool succeeded() const noexcept { return error.isEmpty(); }
};

class RadioPlaylistParser final {
public:
    static RadioPlaylistResult parse(
        const QByteArray& content,
        const QUrl& sourceUrl = {},
        const QString& contentType = {});
};

class IcyMetadataDemuxer final {
public:
    struct Output final {
        QByteArray audio;
        std::optional<QString> streamTitle;
    };

    explicit IcyMetadataDemuxer(std::size_t metadataInterval);
    [[nodiscard]] Output consume(const QByteArray& input);
    void reset();

private:
    enum class State { Audio, MetadataLength, Metadata };

    std::size_t m_metadataInterval{};
    std::size_t m_audioRemaining{};
    std::size_t m_metadataRemaining{};
    QByteArray m_metadata;
    State m_state{State::Audio};
};

struct ReconnectPolicy final {
    std::chrono::milliseconds initialDelay{1'000};
    std::chrono::milliseconds maximumDelay{30'000};
    double multiplier{2.0};

    [[nodiscard]] std::chrono::milliseconds delayForAttempt(std::size_t attempt) const noexcept;
};

} // namespace yaap
