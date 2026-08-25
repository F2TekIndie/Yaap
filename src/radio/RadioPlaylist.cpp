#include "radio/RadioPlaylist.hpp"

#include <QBuffer>
#include <QMap>
#include <QRegularExpression>
#include <QTextStream>
#include <QXmlStreamReader>

#include <algorithm>
#include <cmath>
#include <utility>

namespace yaap {
namespace {

constexpr qsizetype maximumPlaylistBytes = 2 * 1024 * 1024;
constexpr std::size_t maximumStations = 10'000;

[[nodiscard]] QUrl resolvedUrl(const QString& value, const QUrl& source)
{
    const QUrl candidate{value.trimmed()};
    const auto result = candidate.isRelative() ? source.resolved(candidate) : candidate;
    if (result.scheme() != "http" && result.scheme() != "https") {
        return {};
    }
    return result;
}

void addStation(
    std::vector<RadioStation>& stations,
    QString name,
    const QString& url,
    const QUrl& source)
{
    if (stations.size() >= maximumStations) {
        return;
    }
    const auto resolved = resolvedUrl(url, source);
    if (!resolved.isValid() || resolved.isEmpty()) {
        return;
    }
    if (name.trimmed().isEmpty()) {
        name = resolved.host();
    }
    stations.push_back({name.trimmed(), resolved});
}

[[nodiscard]] RadioPlaylistResult parseM3u(const QByteArray& content, const QUrl& source)
{
    RadioPlaylistResult result;
    QTextStream stream{content};
    QString pendingName;
    while (!stream.atEnd() && result.stations.size() < maximumStations) {
        const auto line = stream.readLine().trimmed();
        if (line.startsWith("#EXTINF:", Qt::CaseInsensitive)) {
            const auto comma = line.indexOf(',');
            pendingName = comma >= 0 ? line.mid(comma + 1).trimmed() : QString{};
        } else if (!line.isEmpty() && !line.startsWith('#')) {
            addStation(result.stations, std::exchange(pendingName, {}), line, source);
        }
    }
    return result;
}

[[nodiscard]] RadioPlaylistResult parsePls(const QByteArray& content, const QUrl& source)
{
    QMap<int, QString> urls;
    QMap<int, QString> titles;
    QTextStream stream{content};
    const QRegularExpression entry{R"(^\s*(File|Title)(\d+)\s*=\s*(.*)$)",
        QRegularExpression::CaseInsensitiveOption};
    while (!stream.atEnd() && urls.size() < static_cast<qsizetype>(maximumStations)) {
        const auto match = entry.match(stream.readLine());
        if (!match.hasMatch()) {
            continue;
        }
        const auto index = match.captured(2).toInt();
        if (match.captured(1).compare("File", Qt::CaseInsensitive) == 0) {
            urls[index] = match.captured(3).trimmed();
        } else {
            titles[index] = match.captured(3).trimmed();
        }
    }
    RadioPlaylistResult result;
    for (auto iterator = urls.cbegin(); iterator != urls.cend(); ++iterator) {
        addStation(result.stations, titles.value(iterator.key()), iterator.value(), source);
    }
    return result;
}

[[nodiscard]] RadioPlaylistResult parseXspf(const QByteArray& content, const QUrl& source)
{
    RadioPlaylistResult result;
    QXmlStreamReader xml{content};
    QString title;
    QString location;
    bool inTrack{};
    while (!xml.atEnd() && result.stations.size() < maximumStations) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == u"track") {
            inTrack = true;
            title.clear();
            location.clear();
        } else if (inTrack && xml.isStartElement() && xml.name() == u"title") {
            title = xml.readElementText();
        } else if (inTrack && xml.isStartElement() && xml.name() == u"location") {
            location = xml.readElementText();
        } else if (xml.isEndElement() && xml.name() == u"track") {
            addStation(result.stations, title, location, source);
            inTrack = false;
        }
    }
    if (xml.hasError()) {
        result.error = "Invalid XSPF playlist: " + xml.errorString();
    }
    return result;
}

[[nodiscard]] std::optional<QString> icyTitle(const QByteArray& rawMetadata)
{
    const auto metadata = QString::fromUtf8(rawMetadata).remove(QChar::Null);
    const QRegularExpression expression{
        R"((?:^|;)\s*StreamTitle\s*=\s*(['"])(.*?)\1\s*(?:;|$))",
        QRegularExpression::CaseInsensitiveOption};
    const auto match = expression.match(metadata);
    return match.hasMatch()
        ? std::optional<QString>{match.captured(2).trimmed()}
        : std::nullopt;
}

} // namespace

RadioPlaylistResult RadioPlaylistParser::parse(
    const QByteArray& content,
    const QUrl& sourceUrl,
    const QString& contentType)
{
    if (content.isEmpty()) {
        return {.error = "Radio playlist is empty."};
    }
    if (content.size() > maximumPlaylistBytes) {
        return {.error = "Radio playlist exceeds the 2 MiB safety limit."};
    }

    const auto hint = (sourceUrl.path() + ' ' + contentType).toLower();
    const auto trimmed = content.trimmed();
    if (hint.contains("xspf") || trimmed.startsWith("<?xml")
        || trimmed.contains("<playlist")) {
        return parseXspf(content, sourceUrl);
    }
    if (hint.contains(".pls") || trimmed.startsWith("[playlist]")) {
        return parsePls(content, sourceUrl);
    }
    return parseM3u(content, sourceUrl);
}

IcyMetadataDemuxer::IcyMetadataDemuxer(const std::size_t metadataInterval)
    : m_metadataInterval(metadataInterval)
    , m_audioRemaining(metadataInterval)
{
}

IcyMetadataDemuxer::Output IcyMetadataDemuxer::consume(const QByteArray& input)
{
    Output output;
    qsizetype offset{};
    while (offset < input.size()) {
        if (m_state == State::Audio) {
            if (m_metadataInterval == 0) {
                output.audio.append(input.sliced(offset));
                break;
            }
            const auto count = std::min<qsizetype>(
                static_cast<qsizetype>(m_audioRemaining), input.size() - offset);
            output.audio.append(input.constData() + offset, count);
            offset += count;
            m_audioRemaining -= static_cast<std::size_t>(count);
            if (m_audioRemaining == 0) {
                m_state = State::MetadataLength;
            }
        } else if (m_state == State::MetadataLength) {
            m_metadataRemaining = static_cast<unsigned char>(input[offset++]) * 16U;
            m_metadata.clear();
            if (m_metadataRemaining == 0) {
                m_audioRemaining = m_metadataInterval;
                m_state = State::Audio;
            } else {
                m_state = State::Metadata;
            }
        } else {
            const auto count = std::min<qsizetype>(
                static_cast<qsizetype>(m_metadataRemaining), input.size() - offset);
            m_metadata.append(input.constData() + offset, count);
            offset += count;
            m_metadataRemaining -= static_cast<std::size_t>(count);
            if (m_metadataRemaining == 0) {
                output.streamTitle = icyTitle(m_metadata);
                m_audioRemaining = m_metadataInterval;
                m_state = State::Audio;
            }
        }
    }
    return output;
}

void IcyMetadataDemuxer::reset()
{
    m_audioRemaining = m_metadataInterval;
    m_metadataRemaining = 0;
    m_metadata.clear();
    m_state = State::Audio;
}

std::chrono::milliseconds ReconnectPolicy::delayForAttempt(const std::size_t attempt) const noexcept
{
    const auto factor = std::pow(std::max(multiplier, 1.0), static_cast<double>(attempt));
    const auto calculated = static_cast<double>(initialDelay.count()) * factor;
    return std::chrono::milliseconds{static_cast<std::chrono::milliseconds::rep>(
        std::min(calculated, static_cast<double>(maximumDelay.count())))};
}

} // namespace yaap
