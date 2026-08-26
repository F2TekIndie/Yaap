#include "app/RadioBrowserDirectoryModel.hpp"

#include "provider_runtime/ProviderCache.hpp"

#include <QCryptographicHash>
#include <QLocale>
#include <QPointer>
#include <QVariant>
#include <QDebug>

#include <utility>

namespace yaap {
namespace {

constexpr auto cacheNamespace = "radio-browser";
constexpr qint64 cacheLifetimeSeconds = 24 * 60 * 60;
constexpr int resultLimit = 50;

[[nodiscard]] QString searchCacheKey(const QString& term)
{
    const auto digest = QCryptographicHash::hash(
        term.trimmed().toCaseFolded().toUtf8(), QCryptographicHash::Sha256).toHex();
    return "search:name:" + QString::fromLatin1(digest);
}

} // namespace

RadioBrowserDirectoryModel::RadioBrowserDirectoryModel(
    RadioBrowserClient& client, ProviderCache& cache, QObject* parent)
    : QAbstractListModel(parent)
    , m_client(client)
    , m_cache(cache)
    , m_countryCode(QLocale::territoryToCode(QLocale{}.territory()).toUpper())
{
}

int RadioBrowserDirectoryModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_stations.size());
}

QVariant RadioBrowserDirectoryModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0
        || index.row() >= static_cast<int>(m_stations.size())) {
        return {};
    }
    const auto& station = m_stations[static_cast<std::size_t>(index.row())];
    switch (role) {
    case StationUuidRole: return station.stationUuid;
    case NameRole: return station.name;
    case StreamUrlRole: return station.streamUrl;
    case HomepageUrlRole: return station.homepageUrl;
    case FaviconUrlRole: return station.faviconUrl;
    case CountryCodeRole: return station.countryCode;
    case LanguageRole: return station.language;
    case TagsRole: return station.tags;
    case CodecRole: return station.codec;
    case BitrateRole: return station.bitrate;
    case HlsRole: return station.hls;
    default: return {};
    }
}

QHash<int, QByteArray> RadioBrowserDirectoryModel::roleNames() const
{
    return {
        {StationUuidRole, "stationUuid"},
        {NameRole, "stationName"},
        {StreamUrlRole, "streamUrl"},
        {HomepageUrlRole, "homepageUrl"},
        {FaviconUrlRole, "faviconUrl"},
        {CountryCodeRole, "stationCountryCode"},
        {LanguageRole, "stationLanguage"},
        {TagsRole, "stationTags"},
        {CodecRole, "stationCodec"},
        {BitrateRole, "stationBitrate"},
        {HlsRole, "stationHls"},
    };
}

int RadioBrowserDirectoryModel::count() const noexcept
{
    return static_cast<int>(m_stations.size());
}

bool RadioBrowserDirectoryModel::loading() const noexcept { return m_loading; }
bool RadioBrowserDirectoryModel::stale() const noexcept { return m_stale; }
QString RadioBrowserDirectoryModel::errorMessage() const { return m_errorMessage; }
QString RadioBrowserDirectoryModel::countryCode() const { return m_countryCode; }

void RadioBrowserDirectoryModel::refreshPopular()
{
    const auto key = "popular:" + (m_countryCode.isEmpty() ? QString{"global"} : m_countryCode);
    request(key, [this](RadioBrowserClient::Callback callback) {
        m_client.fetchPopular(m_countryCode, resultLimit, std::move(callback));
    });
}

void RadioBrowserDirectoryModel::search(const QString& name)
{
    const auto term = name.trimmed();
    if (term.isEmpty()) {
        refreshPopular();
        return;
    }
    request(searchCacheKey(term), [this, term](RadioBrowserClient::Callback callback) {
        m_client.searchByName(term, resultLimit, std::move(callback));
    });
}

void RadioBrowserDirectoryModel::play(const int row)
{
    if (row < 0 || row >= static_cast<int>(m_stations.size())) {
        return;
    }
    const auto& station = m_stations[static_cast<std::size_t>(row)];
    m_client.recordClick(station.stationUuid);
    emit playbackRequested(station.streamUrl, station.name);
}

void RadioBrowserDirectoryModel::save(const int row)
{
    if (row >= 0 && row < static_cast<int>(m_stations.size())) {
        emit saveRequested(m_stations[static_cast<std::size_t>(row)]);
    }
}

void RadioBrowserDirectoryModel::cancel()
{
    ++m_generation;
    m_client.cancel();
    setLoading(false);
}

void RadioBrowserDirectoryModel::request(QString cacheKey,
    std::function<void(RadioBrowserClient::Callback)> startRequest)
{
    ++m_generation;
    const auto generation = m_generation;
    m_client.cancel();
    setError({});
    const auto restored = restoreCache(cacheKey);
    if (!restored) {
        replaceStations({});
        setStale(false);
    }
    setLoading(true);

    QPointer<RadioBrowserDirectoryModel> self{this};
    startRequest([self, generation, cacheKey = std::move(cacheKey), restored]
        (RadioBrowserResult result) mutable {
        if (!self || generation != self->m_generation) {
            return;
        }
        self->setLoading(false);
        if (!result.succeeded()) {
            self->setError(restored
                    ? "Could not refresh Radio Browser; showing cached results. " + result.error
                    : std::move(result.error));
            return;
        }

        QString cacheError;
        if (!self->m_cache.put(cacheNamespace, cacheKey, result.payload, result.etag,
                cacheLifetimeSeconds, cacheError)) {
            qWarning() << cacheError;
        }
        self->replaceStations(std::move(result.stations));
        self->setStale(false);
        self->setError({});
    });
}

bool RadioBrowserDirectoryModel::restoreCache(const QString& key)
{
    QString error;
    const auto entry = m_cache.get(cacheNamespace, key, true, error);
    if (!error.isEmpty()) {
        qWarning() << error;
    }
    if (!entry) {
        return false;
    }
    auto result = RadioBrowserParser::parseStations(entry->payload);
    if (!result.succeeded()) {
        return false;
    }
    replaceStations(std::move(result.stations));
    setStale(entry->stale);
    return true;
}

void RadioBrowserDirectoryModel::replaceStations(
    std::vector<RadioBrowserStation> stations)
{
    beginResetModel();
    m_stations = std::move(stations);
    endResetModel();
    emit countChanged();
}

void RadioBrowserDirectoryModel::setLoading(const bool loading)
{
    if (m_loading == loading) {
        return;
    }
    m_loading = loading;
    emit loadingChanged();
}

void RadioBrowserDirectoryModel::setStale(const bool stale)
{
    if (m_stale == stale) {
        return;
    }
    m_stale = stale;
    emit staleChanged();
}

void RadioBrowserDirectoryModel::setError(QString error)
{
    if (m_errorMessage == error) {
        return;
    }
    m_errorMessage = std::move(error);
    emit errorMessageChanged();
}

} // namespace yaap
