#include "audio/MiniaudioOutput.hpp"

#include "audio/AudioAnalysisEngine.hpp"

#include <algorithm>
#include <span>
#include <utility>

#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION
#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#include <miniaudio.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace yaap {

struct MiniaudioOutput::Impl final {
    ma_device device{};
    std::shared_ptr<PcmStream> stream;
    AudioAnalysisEngine* analysisEngine{};
    bool initialized{};

    explicit Impl(AudioAnalysisEngine* analysisEngineValue)
        : analysisEngine(analysisEngineValue)
    {
    }

    static void dataCallback(
        ma_device* device,
        void* output,
        const void*,
        const ma_uint32 frameCount) noexcept
    {
        auto* self = static_cast<Impl*>(device->pUserData);
        auto* samples = static_cast<float*>(output);
        const auto sampleCount = static_cast<std::size_t>(frameCount) * PcmFormat::channels;
        const std::span outputSamples{samples, sampleCount};
        if (self->stream) {
            [[maybe_unused]] const auto rendered = self->stream->render(
                outputSamples, static_cast<std::size_t>(frameCount));
        } else {
            std::fill(outputSamples.begin(), outputSamples.end(), 0.0F);
        }
        if (self->analysisEngine != nullptr) {
            self->analysisEngine->submitInterleaved(outputSamples);
        }
    }

    [[nodiscard]] bool initialize(std::string& error)
    {
        if (initialized) {
            return true;
        }

        auto config = ma_device_config_init(ma_device_type_playback);
        config.playback.format = ma_format_f32;
        config.playback.channels = static_cast<ma_uint32>(PcmFormat::channels);
        config.sampleRate = PcmFormat::sampleRate;
        config.dataCallback = &dataCallback;
        config.pUserData = this;

        const auto result = ma_device_init(nullptr, &config, &device);
        if (result != MA_SUCCESS) {
            error = "Could not initialize the audio output device: ";
            error += ma_result_description(result);
            return false;
        }

        initialized = true;
        return true;
    }

    void stopDevice() noexcept
    {
        if (initialized && ma_device_is_started(&device)) {
            ma_device_stop(&device);
        }
    }

    ~Impl()
    {
        stopDevice();
        if (initialized) {
            ma_device_uninit(&device);
        }
    }
};

MiniaudioOutput::MiniaudioOutput(AudioAnalysisEngine* analysisEngine)
    : m_impl(std::make_unique<Impl>(analysisEngine))
{
}

MiniaudioOutput::~MiniaudioOutput() = default;

bool MiniaudioOutput::attach(std::shared_ptr<PcmStream> stream, std::string& error)
{
    if (!stream) {
        error = "Cannot attach an empty PCM stream.";
        return false;
    }

    m_impl->stopDevice();
    if (m_impl->stream) {
        m_impl->stream->pause();
    }
    if (m_impl->analysisEngine != nullptr) {
        m_impl->analysisEngine->reset();
    }
    m_impl->stream = std::move(stream);
    if (!m_impl->initialize(error)) {
        m_impl->stream.reset();
        return false;
    }
    return true;
}

bool MiniaudioOutput::play(std::string& error)
{
    if (!m_impl->stream || !m_impl->stream->hasAudio()) {
        error = "No audio is loaded.";
        return false;
    }
    if (!m_impl->initialize(error)) {
        return false;
    }

    m_impl->stream->play();
    const auto result = ma_device_start(&m_impl->device);
    if (result != MA_SUCCESS) {
        m_impl->stream->pause();
        error = "Could not start the audio output device: ";
        error += ma_result_description(result);
        return false;
    }
    return true;
}

void MiniaudioOutput::pause() noexcept
{
    m_impl->stopDevice();
    if (m_impl->stream) {
        m_impl->stream->pause();
    }
}

void MiniaudioOutput::clear() noexcept
{
    m_impl->stopDevice();
    if (m_impl->stream) {
        m_impl->stream->pause();
        m_impl->stream.reset();
    }
    if (m_impl->analysisEngine != nullptr) {
        m_impl->analysisEngine->reset();
    }
}

bool MiniaudioOutput::hasAudio() const noexcept
{
    return m_impl->stream && m_impl->stream->hasAudio();
}

bool MiniaudioOutput::isPlaying() const noexcept
{
    return m_impl->stream && m_impl->stream->isPlaying();
}

bool MiniaudioOutput::isEndOfStream() const noexcept
{
    return m_impl->stream && m_impl->stream->isEndOfStream();
}

bool MiniaudioOutput::isFinished() const noexcept
{
    return m_impl->stream && m_impl->stream->isFinished();
}

std::size_t MiniaudioOutput::underrunCount() const noexcept
{
    return m_impl->stream ? m_impl->stream->underrunCount() : 0;
}

std::int64_t MiniaudioOutput::bufferedMilliseconds() const noexcept
{
    return m_impl->stream ? m_impl->stream->bufferedMilliseconds() : 0;
}

std::int64_t MiniaudioOutput::positionMilliseconds() const noexcept
{
    return m_impl->stream ? m_impl->stream->positionMilliseconds() : 0;
}

std::int64_t MiniaudioOutput::durationMilliseconds() const noexcept
{
    return m_impl->stream ? m_impl->stream->durationMilliseconds() : 0;
}

} // namespace yaap
