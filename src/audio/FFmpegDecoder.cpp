#include "audio/FFmpegDecoder.hpp"

#include "audio/PcmFormat.hpp"
#include "audio/StreamMetadata.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
#include <libavutil/mathematics.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
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

struct DictionaryDeleter final {
    void operator()(AVDictionary* dictionary) const noexcept
    {
        av_dict_free(&dictionary);
    }
};

using FormatContextPtr = std::unique_ptr<AVFormatContext, FormatContextDeleter>;
using CodecContextPtr = std::unique_ptr<AVCodecContext, CodecContextDeleter>;
using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;
using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;
using SwrContextPtr = std::unique_ptr<SwrContext, SwrContextDeleter>;
using DictionaryPtr = std::unique_ptr<AVDictionary, DictionaryDeleter>;

class InterruptState final {
public:
    InterruptState(const std::stop_token stopToken, const std::chrono::milliseconds timeout)
        : m_stopToken(stopToken)
        , m_timeout(timeout)
    {
    }

    void arm() noexcept
    {
        m_timedOut.store(false, std::memory_order_relaxed);
        if (m_timeout.count() <= 0) {
            m_deadlineNanoseconds.store(0, std::memory_order_release);
            return;
        }
        const auto deadline = std::chrono::steady_clock::now() + m_timeout;
        m_deadlineNanoseconds.store(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                deadline.time_since_epoch()).count(),
            std::memory_order_release);
    }

    void disarm() noexcept
    {
        m_deadlineNanoseconds.store(0, std::memory_order_release);
    }

    [[nodiscard]] bool timedOut() const noexcept
    {
        return m_timedOut.load(std::memory_order_acquire);
    }

    [[nodiscard]] static int interrupt(void* opaque) noexcept
    {
        auto& state = *static_cast<InterruptState*>(opaque);
        if (state.m_stopToken.stop_requested()) {
            return 1;
        }

        const auto deadline = state.m_deadlineNanoseconds.load(std::memory_order_acquire);
        if (deadline <= 0) {
            return 0;
        }
        const auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        if (now < deadline) {
            return 0;
        }
        state.m_timedOut.store(true, std::memory_order_release);
        return 1;
    }

private:
    std::stop_token m_stopToken;
    std::chrono::milliseconds m_timeout;
    std::atomic<std::int64_t> m_deadlineNanoseconds{0};
    std::atomic<bool> m_timedOut{false};
};

struct OperationResult final {
    std::string error;
    bool cancelled{};

    [[nodiscard]] bool succeeded() const noexcept
    {
        return error.empty() && !cancelled;
    }
};

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

[[nodiscard]] std::int64_t mediaDurationMilliseconds(const AVFormatContext& context)
{
    if (context.duration == AV_NOPTS_VALUE || context.duration <= 0) {
        return 0;
    }
    return av_rescale_q(context.duration, AV_TIME_BASE_Q, AVRational{1, 1'000});
}

[[nodiscard]] std::vector<StreamMetadataParser::Field> metadataFields(
    const AVDictionary* dictionary)
{
    std::vector<StreamMetadataParser::Field> fields;
    const AVDictionaryEntry* entry = nullptr;
    while ((entry = av_dict_iterate(dictionary, entry)) != nullptr) {
        fields.emplace_back(entry->key != nullptr ? entry->key : "",
            entry->value != nullptr ? entry->value : "");
    }
    return fields;
}

} // namespace

StreamDecodeResult FFmpegDecoder::streamFile(
    const std::filesystem::path& path,
    PcmStream& destination,
    ReadyCallback readyCallback,
    MetadataCallback metadataCallback,
    StreamOptions options,
    const std::stop_token stopToken) const
{
    if (path.empty()) {
        return {.error = "No input file was selected."};
    }
    return streamInput(
        utf8Path(path), destination, std::move(readyCallback), std::move(metadataCallback),
        std::move(options), stopToken);
}

StreamDecodeResult FFmpegDecoder::streamUrl(
    const std::string& url,
    PcmStream& destination,
    ReadyCallback readyCallback,
    MetadataCallback metadataCallback,
    StreamOptions options,
    const std::stop_token stopToken) const
{
    options.requestIcyMetadata = true;
    return streamInput(url, destination, std::move(readyCallback), std::move(metadataCallback),
        std::move(options), stopToken);
}

StreamDecodeResult FFmpegDecoder::streamInput(
    const std::string& input,
    PcmStream& destination,
    ReadyCallback readyCallback,
    MetadataCallback metadataCallback,
    StreamOptions options,
    const std::stop_token stopToken) const
{
    if (stopToken.stop_requested()) {
        return {.cancelled = true};
    }

    if (input.empty()) {
        return {.error = "No media source was selected."};
    }

    // Wake a producer blocked on a full PCM ring as soon as cancellation is requested.
    std::stop_callback stopWakeup{
        stopToken, [&destination] { destination.interruptProducerWait(); }};

    InterruptState interruptState{stopToken, options.ioTimeout};
    AVFormatContext* rawFormatContext = avformat_alloc_context();
    if (rawFormatContext == nullptr) {
        return {.error = "Could not allocate the FFmpeg format context."};
    }
    rawFormatContext->interrupt_callback = {
        .callback = &InterruptState::interrupt,
        .opaque = &interruptState};

    AVDictionary* rawOpenOptions = nullptr;
    const auto timeoutMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(
        options.ioTimeout).count();
    if (timeoutMicroseconds > 0) {
        av_dict_set_int(&rawOpenOptions, "rw_timeout", timeoutMicroseconds, 0);
    }
    if (!options.userAgent.empty()) {
        av_dict_set(&rawOpenOptions, "user_agent", options.userAgent.c_str(), 0);
    }
    if (!options.httpHeaders.empty()) {
        av_dict_set(&rawOpenOptions, "headers", options.httpHeaders.c_str(), 0);
    }
    if (options.requestIcyMetadata) {
        av_dict_set(&rawOpenOptions, "icy", "1", 0);
    }
    if (options.reconnectNetworkStream) {
        av_dict_set(&rawOpenOptions, "reconnect", "1", 0);
        av_dict_set(&rawOpenOptions, "reconnect_streamed", "1", 0);
        av_dict_set(&rawOpenOptions, "reconnect_on_network_error", "1", 0);
        av_dict_set(&rawOpenOptions, "reconnect_delay_max", "30", 0);
    }
    DictionaryPtr openOptions{rawOpenOptions};

    interruptState.arm();
    auto resultCode = avformat_open_input(
        &rawFormatContext, input.c_str(), nullptr, &rawOpenOptions);
    interruptState.disarm();
    openOptions.release();
    DictionaryPtr remainingOpenOptions{rawOpenOptions};
    if (resultCode < 0) {
        if (rawFormatContext != nullptr) {
            avformat_free_context(rawFormatContext);
        }
        if (stopToken.stop_requested()) {
            return {.cancelled = true};
        }
        if (interruptState.timedOut()) {
            return {.error = "Opening the media source timed out."};
        }
        return {.error = "Could not open the media source: " + ffmpegError(resultCode)};
    }
    FormatContextPtr formatContext{rawFormatContext};

    interruptState.arm();
    resultCode = avformat_find_stream_info(formatContext.get(), nullptr);
    interruptState.disarm();
    if (resultCode < 0) {
        if (stopToken.stop_requested()) {
            return {.cancelled = true};
        }
        if (interruptState.timedOut()) {
            return {.error = "Reading media stream information timed out."};
        }
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
        static_cast<int>(PcmFormat::sampleRate),
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

    const auto startPosition = std::max<std::int64_t>(options.startPositionMilliseconds, 0);
    destination.setStartPositionMilliseconds(startPosition);
    if (startPosition > 0) {
        const auto targetTimestamp = av_rescale_q(
            startPosition,
            AVRational{1, 1'000},
            formatContext->streams[audioStreamIndex]->time_base);
        interruptState.arm();
        resultCode = avformat_seek_file(
            formatContext.get(),
            audioStreamIndex,
            std::numeric_limits<std::int64_t>::min(),
            targetTimestamp,
            std::numeric_limits<std::int64_t>::max(),
            AVSEEK_FLAG_BACKWARD);
        interruptState.disarm();
        if (resultCode < 0) {
            if (stopToken.stop_requested()) {
                return {.cancelled = true};
            }
            if (interruptState.timedOut()) {
                return {.error = "Seeking the media source timed out."};
            }
            return {.error = "Could not seek in the media source: " + ffmpegError(resultCode)};
        }
        avcodec_flush_buffers(codecContext.get());
    }

    PacketPtr packet{av_packet_alloc()};
    FramePtr frame{av_frame_alloc()};
    if (!packet || !frame) {
        return {.error = "Could not allocate FFmpeg decode buffers."};
    }

    AudioStreamInfo streamInfo;
    streamInfo.durationMilliseconds = mediaDurationMilliseconds(*formatContext);
    destination.setDurationMilliseconds(streamInfo.durationMilliseconds);
    if (const auto* title = av_dict_get(formatContext->metadata, "title", nullptr, 0); title != nullptr) {
        streamInfo.title = title->value;
    }

    std::optional<NowPlayingMetadata> lastMetadata;
    const auto publishMetadata = [&](std::optional<NowPlayingMetadata> metadata) {
        if (!metadata || !metadataCallback
            || (lastMetadata && lastMetadata->sameContentAs(*metadata))) {
            return;
        }
        lastMetadata = *metadata;
        metadataCallback(*metadata);
    };
    const auto publishDictionary = [&](const AVDictionary* dictionary,
                                       const NowPlayingMetadataSource source) {
        publishMetadata(StreamMetadataParser::parseFields(
            metadataFields(dictionary), source));
    };
    const auto pollIcyMetadata = [&] {
        if (!options.requestIcyMetadata || formatContext->pb == nullptr) {
            return;
        }

        std::uint8_t* rawMetadata = nullptr;
        if (av_opt_get(formatContext->pb, "icy_metadata_packet", AV_OPT_SEARCH_CHILDREN,
                &rawMetadata) >= 0
            && rawMetadata != nullptr && rawMetadata[0] != '\0') {
            publishMetadata(StreamMetadataParser::parseIcy(
                reinterpret_cast<const char*>(rawMetadata)));
        }
        av_free(rawMetadata);

        AVDictionary* rawIcyDictionary = nullptr;
        if (av_opt_get_dict_val(formatContext->pb, "metadata", AV_OPT_SEARCH_CHILDREN,
                &rawIcyDictionary) >= 0
            && rawIcyDictionary != nullptr) {
            DictionaryPtr icyDictionary{rawIcyDictionary};
            publishDictionary(icyDictionary.get(), NowPlayingMetadataSource::Icy);
        }
    };
    pollIcyMetadata();

    std::vector<float> convertedSamples;
    std::size_t decodedFrameCount = 0;
    bool readyWasPublished = false;
    const auto requestedPrebufferFrames = static_cast<std::size_t>(std::max<std::int64_t>(
        (options.prebufferDuration.count() * PcmFormat::sampleRate) / 1'000,
        1));
    const auto prebufferTargetFrames = std::min(
        requestedPrebufferFrames,
        std::max<std::size_t>(destination.capacityFrames() / 2, 1));
    auto nextIcyMetadataPoll = std::chrono::steady_clock::now();

    const auto writeSamples = [&](std::span<const float> samples) -> OperationResult {
        while (!samples.empty()) {
            if (stopToken.stop_requested()) {
                return {.cancelled = true};
            }

            const auto writtenFrames = destination.write(samples);
            if (writtenFrames == 0) {
                if (!destination.waitForWritableFrames(stopToken)) {
                    return {.cancelled = true};
                }
                continue;
            }

            decodedFrameCount += writtenFrames;
            samples = samples.subspan(writtenFrames * PcmFormat::channels);
            if (!readyWasPublished
                && destination.bufferedFrames() >= prebufferTargetFrames) {
                readyWasPublished = true;
                if (readyCallback) {
                    readyCallback(streamInfo);
                }
            }
        }
        return {};
    };

    const auto convertSamples = [&](const std::uint8_t* const* inputPlanes,
                                    const int inputSampleCount) -> OperationResult {
        const auto outputCapacity64 = av_rescale_rnd(
            swr_get_delay(resampler.get(), codecContext->sample_rate) + inputSampleCount,
            PcmFormat::sampleRate,
            codecContext->sample_rate,
            AV_ROUND_UP);
        if (outputCapacity64 <= 0) {
            return {};
        }
        if (outputCapacity64 > std::numeric_limits<int>::max()) {
            return {.error = "Decoded audio frame is too large to resample safely."};
        }

        const auto outputCapacity = static_cast<int>(outputCapacity64);
        convertedSamples.resize(
            static_cast<std::size_t>(outputCapacity) * PcmFormat::channels);
        std::uint8_t* outputPlanes[] = {
            reinterpret_cast<std::uint8_t*>(convertedSamples.data())};

        const auto converted = swr_convert(
            resampler.get(),
            outputPlanes,
            outputCapacity,
            inputPlanes,
            inputSampleCount);
        if (converted < 0) {
            return {.error = "Could not resample decoded audio: " + ffmpegError(converted)};
        }

        const auto convertedSampleCount = static_cast<std::size_t>(converted) * PcmFormat::channels;
        return writeSamples(std::span<const float>{convertedSamples}.first(convertedSampleCount));
    };

    const auto receiveFrames = [&]() -> OperationResult {
        while (true) {
            if (stopToken.stop_requested()) {
                return {.cancelled = true};
            }

            const auto receiveResult = avcodec_receive_frame(codecContext.get(), frame.get());
            if (receiveResult == AVERROR(EAGAIN) || receiveResult == AVERROR_EOF) {
                return {};
            }
            if (receiveResult < 0) {
                return {.error = "Could not decode an audio frame: " + ffmpegError(receiveResult)};
            }

            const auto* const* inputPlanes =
                const_cast<const std::uint8_t* const*>(frame->extended_data);
            auto operation = convertSamples(inputPlanes, frame->nb_samples);
            av_frame_unref(frame.get());
            if (!operation.succeeded()) {
                return operation;
            }
        }
    };

    const auto submitPacket = [&](const AVPacket* inputPacket) -> OperationResult {
        auto sendResult = avcodec_send_packet(codecContext.get(), inputPacket);
        if (sendResult == AVERROR(EAGAIN)) {
            auto operation = receiveFrames();
            if (!operation.succeeded()) {
                return operation;
            }
            sendResult = avcodec_send_packet(codecContext.get(), inputPacket);
        }
        if (sendResult < 0 && sendResult != AVERROR_EOF) {
            return {.error = "Could not submit audio data to the decoder: " + ffmpegError(sendResult)};
        }
        return receiveFrames();
    };

    while (true) {
        interruptState.arm();
        resultCode = av_read_frame(formatContext.get(), packet.get());
        interruptState.disarm();
        if (resultCode < 0) {
            break;
        }

        if (stopToken.stop_requested()) {
            return {.info = std::move(streamInfo),
                    .decodedFrameCount = decodedFrameCount,
                    .cancelled = true};
        }

        if (formatContext->event_flags & AVFMT_EVENT_FLAG_METADATA_UPDATED) {
            publishDictionary(formatContext->metadata,
                NowPlayingMetadataSource::Container);
            formatContext->event_flags &= ~AVFMT_EVENT_FLAG_METADATA_UPDATED;
        }
        for (unsigned int streamIndex = 0; streamIndex < formatContext->nb_streams;
             ++streamIndex) {
            auto* stream = formatContext->streams[streamIndex];
            if (stream->event_flags & AVSTREAM_EVENT_FLAG_METADATA_UPDATED) {
                publishDictionary(stream->metadata,
                    NowPlayingMetadataSource::Container);
                stream->event_flags &= ~AVSTREAM_EVENT_FLAG_METADATA_UPDATED;
            }
        }

        std::size_t sideDataSize{};
        if (const auto* sideData = av_packet_get_side_data(packet.get(),
                AV_PKT_DATA_STRINGS_METADATA, &sideDataSize);
            sideData != nullptr && sideDataSize > 0) {
            AVDictionary* rawPacketMetadata = nullptr;
            if (av_packet_unpack_dictionary(
                    sideData, sideDataSize, &rawPacketMetadata) >= 0) {
                DictionaryPtr packetMetadata{rawPacketMetadata};
                publishDictionary(packetMetadata.get(),
                    NowPlayingMetadataSource::TimedId3);
            }
        }

        const auto now = std::chrono::steady_clock::now();
        if (now >= nextIcyMetadataPoll) {
            pollIcyMetadata();
            nextIcyMetadataPoll = now + std::chrono::milliseconds{100};
        }

        if (packet->stream_index == audioStreamIndex) {
            auto operation = submitPacket(packet.get());
            if (!operation.succeeded()) {
                return {.info = std::move(streamInfo),
                        .decodedFrameCount = decodedFrameCount,
                        .error = std::move(operation.error),
                        .cancelled = operation.cancelled};
            }
        }
        av_packet_unref(packet.get());
    }

    if (resultCode != AVERROR_EOF) {
        if (stopToken.stop_requested()) {
            return {.info = std::move(streamInfo),
                    .decodedFrameCount = decodedFrameCount,
                    .cancelled = true};
        }
        if (interruptState.timedOut()) {
            return {.info = std::move(streamInfo),
                    .decodedFrameCount = decodedFrameCount,
                    .error = "Reading the media source timed out."};
        }
        return {.info = std::move(streamInfo),
                .decodedFrameCount = decodedFrameCount,
                .error = "Could not finish reading the audio file: " + ffmpegError(resultCode)};
    }

    auto operation = submitPacket(nullptr);
    if (!operation.succeeded()) {
        return {.info = std::move(streamInfo),
                .decodedFrameCount = decodedFrameCount,
                .error = std::move(operation.error),
                .cancelled = operation.cancelled};
    }

    while (swr_get_delay(resampler.get(), PcmFormat::sampleRate) > 0) {
        const auto delayedSamples = swr_get_delay(resampler.get(), PcmFormat::sampleRate);
        const auto outputCapacity = static_cast<int>(
            std::min<std::int64_t>(delayedSamples, std::numeric_limits<int>::max()));
        convertedSamples.resize(
            static_cast<std::size_t>(outputCapacity) * PcmFormat::channels);
        std::uint8_t* outputPlanes[] = {
            reinterpret_cast<std::uint8_t*>(convertedSamples.data())};
        const auto converted = swr_convert(
            resampler.get(), outputPlanes, outputCapacity, nullptr, 0);
        if (converted < 0) {
            return {.info = std::move(streamInfo),
                    .decodedFrameCount = decodedFrameCount,
                    .error = "Could not flush resampled audio: " + ffmpegError(converted)};
        }
        if (converted == 0) {
            break;
        }

        operation = writeSamples(std::span<const float>{convertedSamples}.first(
            static_cast<std::size_t>(converted) * PcmFormat::channels));
        if (!operation.succeeded()) {
            return {.info = std::move(streamInfo),
                    .decodedFrameCount = decodedFrameCount,
                    .error = std::move(operation.error),
                    .cancelled = operation.cancelled};
        }
    }

    if (decodedFrameCount == 0) {
        return {.info = std::move(streamInfo), .error = "The decoder produced no audio samples."};
    }

    streamInfo.durationMilliseconds = static_cast<std::int64_t>(
        (decodedFrameCount * 1'000ULL) / PcmFormat::sampleRate) + startPosition;
    if (destination.durationMilliseconds() <= 0) {
        destination.setDurationMilliseconds(streamInfo.durationMilliseconds);
    } else {
        streamInfo.durationMilliseconds = destination.durationMilliseconds();
    }
    if (!readyWasPublished && readyCallback) {
        readyCallback(streamInfo);
    }
    destination.markEndOfStream();
    return {.info = std::move(streamInfo), .decodedFrameCount = decodedFrameCount};
}

} // namespace yaap
