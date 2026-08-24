#include "audio/MiniaudioOutput.hpp"

#include "audio/AudioBuffer.hpp"

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
    AudioBuffer buffer;
    bool initialized{};

    static void dataCallback(
        ma_device* device,
        void* output,
        const void*,
        const ma_uint32 frameCount) noexcept
    {
        auto* self = static_cast<Impl*>(device->pUserData);
        auto* samples = static_cast<float*>(output);
        const auto sampleCount = static_cast<std::size_t>(frameCount) * AudioBuffer::channels;
        [[maybe_unused]] const auto rendered = self->buffer.render(
            std::span<float>{samples, sampleCount}, static_cast<std::size_t>(frameCount));
    }

    [[nodiscard]] bool initialize(std::string& error)
    {
        if (initialized) {
            return true;
        }

        auto config = ma_device_config_init(ma_device_type_playback);
        config.playback.format = ma_format_f32;
        config.playback.channels = static_cast<ma_uint32>(AudioBuffer::channels);
        config.sampleRate = AudioBuffer::sampleRate;
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

MiniaudioOutput::MiniaudioOutput()
    : m_impl(std::make_unique<Impl>())
{
}

MiniaudioOutput::~MiniaudioOutput() = default;

bool MiniaudioOutput::load(DecodedAudio audio, std::string& error)
{
    m_impl->stopDevice();
    m_impl->buffer.setSamples(std::move(audio.interleavedSamples));
    return m_impl->initialize(error);
}

bool MiniaudioOutput::play(std::string& error)
{
    if (!m_impl->buffer.hasAudio()) {
        error = "No audio is loaded.";
        return false;
    }
    if (!m_impl->initialize(error)) {
        return false;
    }

    m_impl->buffer.play();
    const auto result = ma_device_start(&m_impl->device);
    if (result != MA_SUCCESS) {
        m_impl->buffer.pause();
        error = "Could not start the audio output device: ";
        error += ma_result_description(result);
        return false;
    }
    return true;
}

void MiniaudioOutput::pause() noexcept
{
    m_impl->stopDevice();
    m_impl->buffer.pause();
}

void MiniaudioOutput::stop() noexcept
{
    m_impl->stopDevice();
    m_impl->buffer.stop();
}

bool MiniaudioOutput::hasAudio() const noexcept
{
    return m_impl->buffer.hasAudio();
}

bool MiniaudioOutput::isPlaying() const noexcept
{
    return m_impl->buffer.isPlaying();
}

bool MiniaudioOutput::isFinished() const noexcept
{
    return m_impl->buffer.isFinished();
}

std::int64_t MiniaudioOutput::positionMilliseconds() const noexcept
{
    return m_impl->buffer.positionMilliseconds();
}

std::int64_t MiniaudioOutput::durationMilliseconds() const noexcept
{
    return m_impl->buffer.durationMilliseconds();
}

} // namespace yaap

