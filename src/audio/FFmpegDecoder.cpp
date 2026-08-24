#include "audio/FFmpegDecoder.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

namespace yaap {
namespace {

struct FormatContextDeleter final {
    void operator()(AVFormatContext* context) const noexcept
    {
        avformat_close_input(&context);
    }
};

struct CodecContextDeleter final {
    void operator()(AVCodecContext* context) const noexcept
    {
        avcodec_free_context(&context);
    }
};

struct PacketDeleter final {
    void operator()(AVPacket* packet) const noexcept
    {
        av_packet_free(&packet);
    }
};

struct FrameDeleter final {
    void operator()(AVFrame* frame) const noexcept
    {
        av_frame_free(&frame);
    }
};

struct SwrContextDeleter final {
    void operator()(SwrContext* context) const noexcept
    {
        swr_free(&context);
    }
};

using FormatContextPtr = std::unique_ptr<AVFormatContext, FormatContextDeleter>;
using CodecContextPtr = std::unique_ptr<AVCodecContext, CodecContextDeleter>;
using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;
using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;
using SwrContextPtr = std::unique_ptr<SwrContext, SwrContextDeleter>;

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

} // namespace

DecodeResult FFmpegDecoder::decode(const std::filesystem::path& path, const std::stop_token stopToken) const
{
    // PROTOTYPE: The complete source is decoded into RAM. Replace this method with
    // a packet-producing decode worker feeding a bounded SPSC PCM ring buffer.
    // Keeping all FFmpeg details behind this boundary makes that replacement local.
    if (stopToken.stop_requested()) {
        return {.cancelled = true};
    }

    if (path.empty()) {
        return {.error = "No input file was selected."};
    }

    AVFormatContext* rawFormatContext = nullptr;
    const auto pathString = utf8Path(path);
    auto resultCode = avformat_open_input(&rawFormatContext, pathString.c_str(), nullptr, nullptr);
    if (resultCode < 0) {
        return {.error = "Could not open the audio file: " + ffmpegError(resultCode)};
    }
    FormatContextPtr formatContext{rawFormatContext};

    resultCode = avformat_find_stream_info(formatContext.get(), nullptr);
    if (resultCode < 0) {
        return {.error = "Could not read media stream information: " + ffmpegError(resultCode)};
    }

    const auto audioStreamIndex = av_find_best_stream(
        formatContext.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audioStreamIndex < 0) {
        return {.error = "The selected file does not contain a playable audio stream."};
    }

    const auto* codecParameters = formatContext->streams[audioStreamIndex]->codecpar;
    const auto* codec = avcodec_find_decoder(codecParameters->codec_id);
    if (codec == nullptr) {
        return {.error = "No FFmpeg decoder is available for this audio format."};
    }

    CodecContextPtr codecContext{avcodec_alloc_context3(codec)};
    if (!codecContext) {
        return {.error = "Could not allocate the FFmpeg decoder context."};
    }

    resultCode = avcodec_parameters_to_context(codecContext.get(), codecParameters);
    if (resultCode < 0) {
        return {.error = "Could not configure the audio decoder: " + ffmpegError(resultCode)};
    }

    resultCode = avcodec_open2(codecContext.get(), codec, nullptr);
    if (resultCode < 0) {
        return {.error = "Could not start the audio decoder: " + ffmpegError(resultCode)};
    }

    if (codecContext->sample_rate <= 0 || codecContext->ch_layout.nb_channels <= 0) {
        return {.error = "The audio stream has an invalid sample rate or channel layout."};
    }

    AVChannelLayout outputLayout = AV_CHANNEL_LAYOUT_STEREO;
    SwrContext* rawResampler = nullptr;
    resultCode = swr_alloc_set_opts2(
        &rawResampler,
        &outputLayout,
        AV_SAMPLE_FMT_FLT,
        static_cast<int>(DecodedAudio::outputSampleRate),
        &codecContext->ch_layout,
        codecContext->sample_fmt,
        codecContext->sample_rate,
        0,
        nullptr);
    if (resultCode < 0 || rawResampler == nullptr) {
        return {.error = "Could not create the audio resampler: " + ffmpegError(resultCode)};
    }
    SwrContextPtr resampler{rawResampler};

    resultCode = swr_init(resampler.get());
    if (resultCode < 0) {
        return {.error = "Could not initialize the audio resampler: " + ffmpegError(resultCode)};
    }

    PacketPtr packet{av_packet_alloc()};
    FramePtr frame{av_frame_alloc()};
    if (!packet || !frame) {
        return {.error = "Could not allocate FFmpeg decode buffers."};
    }

    DecodedAudio decoded;
    if (const auto* title = av_dict_get(formatContext->metadata, "title", nullptr, 0); title != nullptr) {
        decoded.title = title->value;
    }

    const auto appendFrame = [&]() -> std::optional<std::string> {
        const auto outputCapacity = static_cast<int>(av_rescale_rnd(
            swr_get_delay(resampler.get(), codecContext->sample_rate) + frame->nb_samples,
            DecodedAudio::outputSampleRate,
            codecContext->sample_rate,
            AV_ROUND_UP));
        if (outputCapacity <= 0) {
            return std::nullopt;
        }

        const auto oldSize = decoded.interleavedSamples.size();
        decoded.interleavedSamples.resize(
            oldSize + static_cast<std::size_t>(outputCapacity) * DecodedAudio::outputChannels);
        std::uint8_t* outputPlanes[] = {
            reinterpret_cast<std::uint8_t*>(decoded.interleavedSamples.data() + oldSize)};
        const auto* const* inputPlanes = const_cast<const std::uint8_t* const*>(frame->extended_data);

        const auto converted = swr_convert(
            resampler.get(),
            outputPlanes,
            outputCapacity,
            inputPlanes,
            frame->nb_samples);
        if (converted < 0) {
            decoded.interleavedSamples.resize(oldSize);
            return "Could not resample decoded audio: " + ffmpegError(converted);
        }

        decoded.interleavedSamples.resize(
            oldSize + static_cast<std::size_t>(converted) * DecodedAudio::outputChannels);
        return std::nullopt;
    };

    const auto receiveFrames = [&]() -> std::optional<std::string> {
        while (true) {
            const auto receiveResult = avcodec_receive_frame(codecContext.get(), frame.get());
            if (receiveResult == AVERROR(EAGAIN) || receiveResult == AVERROR_EOF) {
                return std::nullopt;
            }
            if (receiveResult < 0) {
                return "Could not decode an audio frame: " + ffmpegError(receiveResult);
            }
            if (const auto error = appendFrame()) {
                return error;
            }
            av_frame_unref(frame.get());
        }
    };

    while ((resultCode = av_read_frame(formatContext.get(), packet.get())) >= 0) {
        if (stopToken.stop_requested()) {
            return {.cancelled = true};
        }

        if (packet->stream_index == audioStreamIndex) {
            const auto sendResult = avcodec_send_packet(codecContext.get(), packet.get());
            if (sendResult < 0 && sendResult != AVERROR(EAGAIN)) {
                return {.error = "Could not submit audio data to the decoder: " + ffmpegError(sendResult)};
            }
            if (const auto error = receiveFrames()) {
                return {.error = *error};
            }
        }
        av_packet_unref(packet.get());
    }

    if (resultCode != AVERROR_EOF) {
        return {.error = "Could not finish reading the audio file: " + ffmpegError(resultCode)};
    }

    resultCode = avcodec_send_packet(codecContext.get(), nullptr);
    if (resultCode < 0 && resultCode != AVERROR_EOF) {
        return {.error = "Could not flush the audio decoder: " + ffmpegError(resultCode)};
    }
    if (const auto error = receiveFrames()) {
        return {.error = *error};
    }

    if (stopToken.stop_requested()) {
        return {.cancelled = true};
    }

    if (decoded.interleavedSamples.empty()) {
        return {.error = "The decoder produced no audio samples."};
    }

    const auto frameCount = decoded.interleavedSamples.size() / DecodedAudio::outputChannels;
    decoded.durationMilliseconds = static_cast<std::int64_t>(
        (frameCount * 1'000ULL) / DecodedAudio::outputSampleRate);
    return {.audio = std::move(decoded)};
}

} // namespace yaap

