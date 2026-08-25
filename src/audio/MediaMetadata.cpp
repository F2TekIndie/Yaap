#include "audio/MediaMetadata.hpp"

#include <array>
#include <memory>
#include <string>
#include <utility>

extern "C" {
#include <libavcodec/codec_id.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
#include <libavutil/mathematics.h>
}

namespace yaap {
namespace {

struct FormatContextDeleter final {
    void operator()(AVFormatContext* context) const noexcept
    {
        avformat_close_input(&context);
    }
};

using FormatContextPtr = std::unique_ptr<AVFormatContext, FormatContextDeleter>;

[[nodiscard]] std::string ffmpegError(const int code)
{
    std::array<char, AV_ERROR_MAX_STRING_SIZE> buffer{};
    if (av_strerror(code, buffer.data(), buffer.size()) < 0) {
        return "Unknown FFmpeg error (" + std::to_string(code) + ')';
    }
    return buffer.data();
}

[[nodiscard]] std::string utf8Path(const std::filesystem::path& path)
{
    const auto value = path.u8string();
    return {value.begin(), value.end()};
}

[[nodiscard]] std::string tag(AVDictionary* dictionary, const char* name)
{
    const auto* entry = av_dict_get(dictionary, name, nullptr, 0);
    return entry != nullptr && entry->value != nullptr ? entry->value : "";
}

} // namespace

MetadataResult MediaMetadataReader::read(const std::filesystem::path& path) const
{
    if (path.empty()) {
        return {.error = "Cannot read metadata from an empty path."};
    }

    AVFormatContext* rawContext = nullptr;
    const auto pathString = utf8Path(path);
    auto result = avformat_open_input(&rawContext, pathString.c_str(), nullptr, nullptr);
    if (result < 0) {
        return {.error = "Could not open media metadata: " + ffmpegError(result)};
    }
    FormatContextPtr context{rawContext};

    result = avformat_find_stream_info(context.get(), nullptr);
    if (result < 0) {
        return {.error = "Could not inspect media metadata: " + ffmpegError(result)};
    }

    MediaMetadata metadata;
    metadata.title = tag(context->metadata, "title");
    metadata.artist = tag(context->metadata, "artist");
    metadata.album = tag(context->metadata, "album");
    if (context->duration != AV_NOPTS_VALUE && context->duration > 0) {
        metadata.durationMilliseconds = av_rescale_q(
            context->duration, AV_TIME_BASE_Q, AVRational{1, 1'000});
    }

    for (unsigned int index = 0; index < context->nb_streams; ++index) {
        const auto* stream = context->streams[index];
        if ((stream->disposition & AV_DISPOSITION_ATTACHED_PIC) == 0
            || stream->attached_pic.data == nullptr
            || stream->attached_pic.size <= 0) {
            continue;
        }

        const auto codecId = stream->codecpar->codec_id;
        if (codecId == AV_CODEC_ID_MJPEG) {
            metadata.artworkMimeType = "image/jpeg";
        } else if (codecId == AV_CODEC_ID_PNG) {
            metadata.artworkMimeType = "image/png";
        } else {
            metadata.artworkMimeType = "application/octet-stream";
        }
        metadata.artwork.assign(
            stream->attached_pic.data,
            stream->attached_pic.data + stream->attached_pic.size);
        break;
    }

    return {.metadata = std::move(metadata)};
}

} // namespace yaap
