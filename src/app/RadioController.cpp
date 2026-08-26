#include "app/RadioController.hpp"

#include "radio/RadioBrowserClient.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <utility>

namespace yaap {
namespace {
constexpr auto stationsSettingsKey = "radio/stations-v1";
}

RadioController::RadioController(RadioBrowserClient* directoryClient, QObject* parent)
    : QAbstractListModel(parent)
    , m_directoryClient(directoryClient)
{
    load();
}

int RadioController::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_stations.size());
}

QVariant RadioController::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0
        || index.row() >= static_cast<int>(m_stations.size())) {
        return {};
    }
    const auto& station = m_stations[static_cast<std::size_t>(index.row())];
    if (role == NameRole) {
        return station.name;
    }
    if (role == UrlRole) {
        return station.streamUrl;
    }
    return {};
}

QHash<int, QByteArray> RadioController::roleNames() const
{
    return {{NameRole, "stationName"}, {UrlRole, "streamUrl"}};
}

int RadioController::count() const noexcept { return static_cast<int>(m_stations.size()); }
QString RadioController::errorMessage() const { return m_errorMessage; }

bool RadioController::addStation(const QString& name, const QString& streamUrl)
{
    const QUrl url{streamUrl.trimmed()};
    if (!url.isValid() || (url.scheme() != "http" && url.scheme() != "https")) {
        setError("A valid HTTP(S) station URL is required.");
        return false;
    }
    return addDirectoryStation(name, url, {});
}

bool RadioController::addDirectoryStation(
    const QString& name, const QUrl& streamUrl, const QString& stationUuid)
{
    if (!streamUrl.isValid()
        || (streamUrl.scheme() != "http" && streamUrl.scheme() != "https")) {
        setError("A valid HTTP(S) station URL is required.");
        return false;
    }
    const auto duplicate = std::find_if(m_stations.cbegin(), m_stations.cend(),
        [&streamUrl, &stationUuid](const RadioStation& station) {
            return (!stationUuid.isEmpty() && station.directoryUuid == stationUuid)
                || station.streamUrl == streamUrl;
        });
    if (duplicate != m_stations.cend()) {
        setError("That station is already in your collection.");
        return false;
    }

    const auto row = static_cast<int>(m_stations.size());
    beginInsertRows({}, row, row);
    m_stations.push_back({name.trimmed().isEmpty() ? streamUrl.host() : name.trimmed(),
        streamUrl, stationUuid});
    endInsertRows();
    persist();
    emit countChanged();
    setError({});
    return true;
}

void RadioController::importPlaylist(const QUrl& playlistUrl)
{
    m_loader.load(playlistUrl, [this](RadioPlaylistResult result) {
        if (!result.succeeded()) {
            setError(std::move(result.error));
            return;
        }
        if (result.stations.empty()) {
            setError("Playlist contains no supported stations.");
            return;
        }
        const auto first = static_cast<int>(m_stations.size());
        const auto last = first + static_cast<int>(result.stations.size()) - 1;
        beginInsertRows({}, first, last);
        m_stations.insert(m_stations.end(),
            std::make_move_iterator(result.stations.begin()),
            std::make_move_iterator(result.stations.end()));
        endInsertRows();
        persist();
        emit countChanged();
        setError({});
    });
}

void RadioController::removeStation(const int row)
{
    if (row < 0 || row >= static_cast<int>(m_stations.size())) {
        return;
    }
    beginRemoveRows({}, row, row);
    m_stations.erase(m_stations.begin() + row);
    endRemoveRows();
    persist();
    emit countChanged();
}

void RadioController::play(const int row)
{
    if (row >= 0 && row < static_cast<int>(m_stations.size())) {
        const auto& station = m_stations[static_cast<std::size_t>(row)];
        if (m_directoryClient && !station.directoryUuid.isEmpty()) {
            m_directoryClient->recordClick(station.directoryUuid);
        }
        emit playbackRequested(station.streamUrl, station.name);
    }
}

void RadioController::persist()
{
    QJsonArray values;
    for (const auto& station : m_stations) {
        values.push_back(QJsonObject{{"name", station.name},
            {"url", station.streamUrl.toString(QUrl::FullyEncoded)},
            {"radioBrowserUuid", station.directoryUuid}});
    }
    m_settings.setValue(stationsSettingsKey,
        QJsonDocument{values}.toJson(QJsonDocument::Compact));
    m_settings.sync();
}

void RadioController::load()
{
    const auto values = QJsonDocument::fromJson(
        m_settings.value(stationsSettingsKey).toByteArray()).array();
    for (const auto& value : values) {
        const auto object = value.toObject();
        const QUrl url{object.value("url").toString()};
        if (url.isValid() && (url.scheme() == "http" || url.scheme() == "https")) {
            m_stations.push_back({object.value("name").toString(), url,
                object.value("radioBrowserUuid").toString()});
        }
    }
}

void RadioController::setError(QString error)
{
    m_errorMessage = std::move(error);
    emit errorMessageChanged();
}

} // namespace yaap
