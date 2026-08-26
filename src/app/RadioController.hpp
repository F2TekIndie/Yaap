#pragma once

#include "radio/RadioPlaylistLoader.hpp"

#include <QAbstractListModel>
#include <QSettings>
#include <QUrl>

#include <vector>

namespace yaap {

class RadioBrowserClient;

class RadioController final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

public:
    enum Role { NameRole = Qt::UserRole + 1, UrlRole };
    Q_ENUM(Role)

    explicit RadioController(RadioBrowserClient* directoryClient = nullptr,
        QObject* parent = nullptr);
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] int count() const noexcept;
    [[nodiscard]] QString errorMessage() const;

    Q_INVOKABLE bool addStation(const QString& name, const QString& streamUrl);
    Q_INVOKABLE bool addDirectoryStation(const QString& name, const QUrl& streamUrl,
        const QString& stationUuid);
    Q_INVOKABLE void importPlaylist(const QUrl& playlistUrl);
    Q_INVOKABLE void removeStation(int row);
    Q_INVOKABLE void play(int row);

signals:
    void countChanged();
    void errorMessageChanged();
    void playbackRequested(const QUrl& url, const QString& title);

private:
    void persist();
    void load();
    void setError(QString error);

    std::vector<RadioStation> m_stations;
    RadioBrowserClient* m_directoryClient{};
    RadioPlaylistLoader m_loader;
    QSettings m_settings;
    QString m_errorMessage;
};

} // namespace yaap
