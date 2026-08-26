#include "app/AudioVisualizationModel.hpp"

#include <QVariant>

#include <utility>

namespace yaap {

AudioVisualizationModel::AudioVisualizationModel(
    AudioAnalysisEngine& engine, QObject* parent)
    : QAbstractListModel(parent)
    , m_engine(engine)
    , m_frame(engine.snapshot())
{
    m_refreshTimer.setInterval(33);
    m_refreshTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_refreshTimer, &QTimer::timeout,
        this, &AudioVisualizationModel::refresh);
    m_refreshTimer.start();
}

int AudioVisualizationModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(audioSpectrumBandCount);
}

QVariant AudioVisualizationModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0
        || index.row() >= static_cast<int>(audioSpectrumBandCount)) {
        return {};
    }
    const auto band = static_cast<std::size_t>(index.row());
    switch (role) {
    case LevelRole: return m_frame.levels[band];
    case PeakRole: return m_frame.peaks[band];
    case FrequencyHzRole: return m_frame.centerFrequenciesHz[band];
    default: return {};
    }
}

QHash<int, QByteArray> AudioVisualizationModel::roleNames() const
{
    return {
        {LevelRole, "level"},
        {PeakRole, "peakLevel"},
        {FrequencyHzRole, "frequencyHz"},
    };
}

int AudioVisualizationModel::count() const noexcept
{
    return static_cast<int>(audioSpectrumBandCount);
}

qulonglong AudioVisualizationModel::revision() const noexcept
{
    return static_cast<qulonglong>(m_frame.sequence);
}

qreal AudioVisualizationModel::rms() const noexcept { return m_frame.rms; }
qreal AudioVisualizationModel::peak() const noexcept { return m_frame.peak; }
bool AudioVisualizationModel::active() const noexcept { return m_frame.active; }
bool AudioVisualizationModel::available() const noexcept { return m_engine.available(); }

qreal AudioVisualizationModel::levelAt(const int band) const noexcept
{
    return band >= 0 && band < static_cast<int>(audioSpectrumBandCount)
        ? m_frame.levels[static_cast<std::size_t>(band)]
        : 0.0;
}

qreal AudioVisualizationModel::peakAt(const int band) const noexcept
{
    return band >= 0 && band < static_cast<int>(audioSpectrumBandCount)
        ? m_frame.peaks[static_cast<std::size_t>(band)]
        : 0.0;
}

qreal AudioVisualizationModel::frequencyAt(const int band) const noexcept
{
    return band >= 0 && band < static_cast<int>(audioSpectrumBandCount)
        ? m_frame.centerFrequenciesHz[static_cast<std::size_t>(band)]
        : 0.0;
}

void AudioVisualizationModel::refresh()
{
    auto frame = m_engine.snapshot();
    if (frame.sequence == m_frame.sequence) {
        return;
    }
    m_frame = std::move(frame);
    emit dataChanged(index(0), index(static_cast<int>(audioSpectrumBandCount) - 1),
        {LevelRole, PeakRole});
    emit frameChanged();
}

} // namespace yaap
