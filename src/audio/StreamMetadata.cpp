#include "audio/StreamMetadata.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <string>

namespace yaap {
namespace {

constexpr std::size_t maximumMetadataBytes = 16 * 1024;

[[nodiscard]] std::string trim(std::string_view value)
{
    while (!value.empty()
        && (value.front() == '\0'
            || std::isspace(static_cast<unsigned char>(value.front())) != 0)) {
        value.remove_prefix(1);
    }
    while (!value.empty()
        && (value.back() == '\0'
            || std::isspace(static_cast<unsigned char>(value.back())) != 0)) {
        value.remove_suffix(1);
    }
    return std::string{value};
}

[[nodiscard]] bool equalsCaseInsensitive(std::string_view left, std::string_view right)
{
    return left.size() == right.size()
        && std::equal(left.begin(), left.end(), right.begin(),
            [](const char lhs, const char rhs) {
                return std::tolower(static_cast<unsigned char>(lhs))
                    == std::tolower(static_cast<unsigned char>(rhs));
            });
}

[[nodiscard]] std::int64_t currentEpochMilliseconds()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

void splitArtistAndTitle(NowPlayingMetadata& metadata)
{
    if (!metadata.artist.empty() || metadata.title.empty()) {
        return;
    }
    constexpr std::string_view separator = " - ";
    const auto separatorPosition = metadata.title.find(separator);
    if (separatorPosition == std::string::npos) {
        return;
    }
    const auto artist = trim(std::string_view{metadata.title}.substr(0, separatorPosition));
    const auto title = trim(std::string_view{metadata.title}.substr(
        separatorPosition + separator.size()));
    if (!artist.empty() && !title.empty()) {
        metadata.artist = artist;
        metadata.title = title;
    }
}

void finishMetadata(NowPlayingMetadata& metadata)
{
    splitArtistAndTitle(metadata);
    if (!metadata.artist.empty() && !metadata.title.empty()) {
        metadata.displayText = metadata.artist + " — " + metadata.title;
    } else if (!metadata.title.empty()) {
        metadata.displayText = metadata.title;
    } else if (!metadata.rawText.empty()) {
        metadata.displayText = metadata.rawText;
    }
    metadata.observedAtEpochMilliseconds = currentEpochMilliseconds();
}

[[nodiscard]] std::vector<StreamMetadataParser::Field> parseIcyFields(
    std::string_view input)
{
    std::vector<StreamMetadataParser::Field> fields;
    std::size_t position{};
    while (position < input.size()) {
        while (position < input.size()
            && (input[position] == ';'
                || std::isspace(static_cast<unsigned char>(input[position])) != 0)) {
            ++position;
        }
        const auto equals = input.find('=', position);
        if (equals == std::string_view::npos) {
            break;
        }
        auto key = trim(input.substr(position, equals - position));
        position = equals + 1;
        while (position < input.size()
            && std::isspace(static_cast<unsigned char>(input[position])) != 0) {
            ++position;
        }

        std::string_view value;
        if (position < input.size() && (input[position] == '\'' || input[position] == '"')) {
            const auto quote = input[position++];
            const auto valueStart = position;
            while (position < input.size() && input[position] != quote) {
                ++position;
            }
            value = input.substr(valueStart, position - valueStart);
            if (position < input.size()) {
                ++position;
            }
        } else {
            const auto separator = input.find(';', position);
            value = input.substr(position,
                separator == std::string_view::npos ? input.size() - position : separator - position);
            position = separator == std::string_view::npos ? input.size() : separator;
        }
        if (!key.empty()) {
            fields.emplace_back(std::move(key), trim(value));
        }
        const auto separator = input.find(';', position);
        if (separator == std::string_view::npos) {
            break;
        }
        position = separator + 1;
    }
    return fields;
}

} // namespace

std::optional<NowPlayingMetadata> StreamMetadataParser::parseIcy(
    const std::string_view rawMetadata)
{
    if (rawMetadata.empty() || rawMetadata.size() > maximumMetadataBytes) {
        return std::nullopt;
    }
    auto metadata = parseFields(parseIcyFields(rawMetadata), NowPlayingMetadataSource::Icy);
    if (metadata) {
        metadata->rawText = trim(rawMetadata);
        finishMetadata(*metadata);
    }
    return metadata;
}

std::optional<NowPlayingMetadata> StreamMetadataParser::parseFields(
    const std::vector<Field>& fields,
    const NowPlayingMetadataSource source)
{
    NowPlayingMetadata metadata;
    metadata.source = source;
    for (const auto& [rawKey, rawValue] : fields) {
        const auto key = trim(rawKey);
        const auto value = trim(rawValue);
        if (value.empty()) {
            continue;
        }
        if (equalsCaseInsensitive(key, "artist")
            || equalsCaseInsensitive(key, "album_artist")) {
            if (metadata.artist.empty()) {
                metadata.artist = value;
            }
        } else if (equalsCaseInsensitive(key, "title")
            || equalsCaseInsensitive(key, "streamtitle")) {
            metadata.title = value;
        } else if (equalsCaseInsensitive(key, "album")) {
            metadata.album = value;
        } else if (equalsCaseInsensitive(key, "streamurl")) {
            metadata.streamUrl = value;
        }
    }
    if (metadata.title.empty() && metadata.artist.empty() && metadata.album.empty()) {
        return std::nullopt;
    }
    metadata.rawText = metadata.title;
    finishMetadata(metadata);
    return metadata;
}

} // namespace yaap
