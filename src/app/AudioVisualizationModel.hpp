#pragma once

#include "audio/AudioAnalysisEngine.hpp"

#include <QAbstractListModel>
#include <QTimer>

namespace yaap {

class AudioVisualizationModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count CONSTANT)
    Q_PROPERTY(qulonglong revision READ revision NOTIFY frameChanged)
    Q_PROPERTY(qreal rms READ rms NOTIFY frameChanged)
    Q_PROPERTY(qreal peak READ peak NOTIFY frameChanged)
    Q_PROPERTY(bool active READ active NOTIFY frameChanged)
    Q_PROPERTY(bool available READ available CONSTANT)

public:
    enum Role {
        LevelRole = Qt::UserRole + 1,
        PeakRole,
        FrequencyHzRole,
    };
    Q_ENUM(Role)

    explicit AudioVisualizationModel(
        AudioAnalysisEngine& engine, QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] int count() const noexcept;
    [[nodiscard]] qulonglong revision() const noexcept;
    [[nodiscard]] qreal rms() const noexcept;
    [[nodiscard]] qreal peak() const noexcept;
    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] bool available() const noexcept;

    Q_INVOKABLE qreal levelAt(int band) const noexcept;
    Q_INVOKABLE qreal peakAt(int band) const noexcept;
    Q_INVOKABLE qreal frequencyAt(int band) const noexcept;

signals:
    void frameChanged();

private:
    void refresh();

    AudioAnalysisEngine& m_engine;
    AudioSpectrumFrame m_frame;
    QTimer m_refreshTimer;
};

} // namespace yaap
