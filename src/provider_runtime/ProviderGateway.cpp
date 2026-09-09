#include "provider_runtime/ProviderGateway.hpp"

#include "extension_api/ProviderProtocol.hpp"
#include "provider_runtime/ProviderExtensionManager.hpp"
#include "provider_runtime/ProviderCache.hpp"
#include "providers/ProviderAccountStore.hpp"

#include <QJsonArray>
#include <QJsonDocument>

#include <algorithm>
#include <utility>

namespace yaap {

ProviderGateway::ProviderGateway(
    ProviderExtensionManager& providers,
    ProviderCache& cache,
    ProviderAccountStore& accounts,
    QObject* parent)
    : QAbstractListModel(parent)
    , m_providers(providers)
    , m_cache(cache)
    , m_accounts(accounts)
{
    connect(&m_providers, &ProviderExtensionManager::providerResponse,
        this, &ProviderGateway::handleResponse);
    connect(&m_providers, &ProviderExtensionManager::providerAvailabilityChanged,
        this, &ProviderGateway::stateChanged);
    connect(&m_accounts, &QAbstractItemModel::rowsInserted,
        this, [this] { emit stateChanged(); });
    connect(&m_accounts, &QAbstractItemModel::rowsRemoved,
        this, [this] { emit stateChanged(); });
    connect(&m_accounts, &QAbstractItemModel::dataChanged,
        this, [this] { emit stateChanged(); });
}

int ProviderGateway::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_items.size());
}

QVariant ProviderGateway::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0
        || index.row() >= static_cast<int>(m_items.size())) {
        return {};
    }
    const auto& item = m_items[static_cast<std::size_t>(index.row())];
    switch (role) {
    case TrackIdRole: return item.id;
    case TitleRole: return item.title;
    case ArtistRole: return item.artist;
    case AlbumRole: return item.album;
    case ProviderIdRole: return item.providerId;
    case DurationRole: return item.durationMilliseconds;
    default: return {};
    }
}

QHash<int, QByteArray> ProviderGateway::roleNames() const
{
    return {{TrackIdRole, "trackId"}, {TitleRole, "title"}, {ArtistRole, "artist"},
        {AlbumRole, "album"}, {ProviderIdRole, "providerId"},
        {DurationRole, "durationMilliseconds"}};
}

int ProviderGateway::count() const noexcept { return static_cast<int>(m_items.size()); }
bool ProviderGateway::loading() const noexcept { return m_pending > 0; }
QString ProviderGateway::errorMessage() const { return m_errorMessage; }
int ProviderGateway::availableProviderCount() const
{
    return m_providers.readyProviderIds().size() + m_accounts.enabledAccountIds().size();
}

bool ProviderGateway::offlineMode() const noexcept { return m_offlineMode; }

void ProviderGateway::setOfflineMode(const bool offline)
{
    if (m_offlineMode != offline) {
        m_offlineMode = offline;
        emit stateChanged();
    }
}

void ProviderGateway::search(const QString& query)
{
    cancel();
    beginResetModel();
    m_items.clear();
    endResetModel();
    emit countChanged();
    m_errorMessage.clear();
    const auto providerIds = m_providers.readyProviderIds();
    const auto accountIds = m_accounts.enabledAccountIds();
    if (providerIds.isEmpty() && accountIds.isEmpty()) {
        m_errorMessage = "Enable and trust at least one provider mod before searching.";
        emit stateChanged();
        return;
    }
    for (const auto& providerId : providerIds) {
        const auto cacheKey = "search:" + query.trimmed();
        QString cacheError;
        if (const auto cached = m_cache.get(providerId, cacheKey, m_offlineMode, cacheError)) {
            const auto cachedObject = QJsonDocument::fromJson(cached->payload).object();
            appendItems(providerId, cachedObject);
        }
        if (!m_offlineMode) {
            startItemsRequest(providerId, provider_protocol::search,
                QJsonObject{{"query", query.trimmed()}, {"limit", 100}}, cacheKey);
        }
    }
    for (const auto& accountId : accountIds) {
        const auto cacheKey = "search:" + query.trimmed();
        QString cacheError;
        if (const auto cached = m_cache.get(accountId, cacheKey, m_offlineMode, cacheError)) {
            appendItems(accountId, QJsonDocument::fromJson(cached->payload).object());
        }
        if (m_offlineMode) {
            continue;
        }
        ++m_pending;
        const auto generation = m_generation;
        m_accounts.searchAccount(accountId, query,
            [this, accountId, cacheKey, generation](ProviderTracksResult response) {
                if (generation != m_generation) {
                    return;
                }
                m_pending = std::max(0, m_pending - 1);
                if (!response.succeeded()) {
                    m_errorMessage = std::move(response.error);
                    emit stateChanged();
                    return;
                }
                QJsonArray items;
                for (const auto& track : response.tracks) {
                    items.push_back(QJsonObject{{"id", QString::fromStdString(track.id)},
                        {"type", "track"}, {"title", QString::fromStdString(track.title)},
                        {"artist", QString::fromStdString(track.artist)},
                        {"album", QString::fromStdString(track.album)},
                        {"playbackUrl", QString::fromStdString(track.source)},
                        {"durationMilliseconds", static_cast<qint64>(track.durationMilliseconds)}});
                }
                const QJsonObject result{{"items", items}};
                QString cacheError;
                m_cache.put(accountId, cacheKey,
                    QJsonDocument{result}.toJson(QJsonDocument::Compact), {}, 300, cacheError);
                m_cache.prune(64 * 1024 * 1024, cacheError);
                appendItems(accountId, result);
                emit stateChanged();
            });
    }
    emit stateChanged();
}

void ProviderGateway::browse(const QString& providerId, const QString& cursor)
{
    cancel();
    beginResetModel();
    m_items.clear();
    endResetModel();
    emit countChanged();
    m_errorMessage.clear();
    const auto cacheKey = "browse:" + cursor;
    QString cacheError;
    if (const auto cached = m_cache.get(providerId, cacheKey, m_offlineMode, cacheError)) {
        appendItems(providerId, QJsonDocument::fromJson(cached->payload).object());
    }
    if (!m_offlineMode) {
        startItemsRequest(providerId, provider_protocol::browse,
            QJsonObject{{"cursor", cursor}, {"limit", 100}}, cacheKey);
    }
    emit stateChanged();
}

void ProviderGateway::play(const int row)
{
    if (row < 0 || row >= static_cast<int>(m_items.size())) {
        return;
    }
    const auto item = m_items[static_cast<std::size_t>(row)];
    if (item.playbackUrl.isValid()) {
        emit playbackRequested(item.playbackUrl, item.title);
        return;
    }
    const auto requestId = m_providers.request(item.providerId,
        provider_protocol::resolvePlayback, QJsonObject{{"trackId", item.id}});
    if (requestId == 0) {
        m_errorMessage = "Provider is not available to resolve playback.";
        emit stateChanged();
        return;
    }
    m_requests.insert(requestKey(item.providerId, requestId),
        {.generation = m_generation, .kind = RequestKind::Resolve, .item = item});
    ++m_pending;
    emit stateChanged();
}

void ProviderGateway::cancel()
{
    for (auto iterator = m_requests.cbegin(); iterator != m_requests.cend(); ++iterator) {
        const auto separator = iterator.key().lastIndexOf(':');
        bool idOk{};
        const auto requestId = iterator.key().sliced(separator + 1).toULongLong(&idOk);
        if (idOk) {
            m_providers.cancel(iterator.key().first(separator), requestId);
        }
    }
    m_requests.clear();
    m_pending = 0;
    ++m_generation;
    emit stateChanged();
}

void ProviderGateway::startItemsRequest(
    const QString& providerId,
    const QString& method,
    const QJsonObject& parameters,
    QString cacheKey)
{
    const auto requestId = m_providers.request(providerId, method, parameters);
    if (requestId == 0) {
        m_errorMessage = "Provider is not ready: " + providerId;
        return;
    }
    m_requests.insert(requestKey(providerId, requestId),
        {.generation = m_generation, .kind = RequestKind::Items,
            .cacheKey = std::move(cacheKey)});
    ++m_pending;
}

void ProviderGateway::handleResponse(
    const QString& providerId,
    const quint64 requestId,
    const QJsonValue& result,
    const QJsonObject& error)
{
    const auto key = requestKey(providerId, requestId);
    const auto iterator = m_requests.find(key);
    if (iterator == m_requests.end()) {
        return;
    }
    const auto context = iterator.value();
    finishRequest(key);
    if (context.generation != m_generation) {
        return;
    }
    if (!error.isEmpty()) {
        m_errorMessage = error.value("message").toString("Provider request failed.");
        emit stateChanged();
        return;
    }
    if (context.kind == RequestKind::Resolve) {
        const QUrl url{result.toObject().value("url").toString()};
        if (!url.isValid() || (url.scheme() != "file" && url.scheme() != "http"
                && url.scheme() != "https")) {
            m_errorMessage = "Provider returned an unsupported playback URL.";
        } else {
            emit playbackRequested(url, context.item.title);
        }
        emit stateChanged();
        return;
    }

    const auto objectResult = result.toObject();
    if (!context.cacheKey.isEmpty()) {
        QString cacheError;
        m_cache.put(providerId, context.cacheKey,
            QJsonDocument{objectResult}.toJson(QJsonDocument::Compact), {}, 300, cacheError);
        m_cache.prune(64 * 1024 * 1024, cacheError);
    }
    appendItems(providerId, objectResult);
    emit stateChanged();
}

void ProviderGateway::appendItems(const QString& providerId, const QJsonObject& result)
{
    std::vector<Item> additions;
    for (const auto& value : result.value("items").toArray()) {
        const auto object = value.toObject();
        if (object.value("type").toString() != "track") {
            continue;
        }
        Item item{.id = object.value("id").toString().trimmed(),
            .title = object.value("title").toString().trimmed(),
            .artist = object.value("artist").toString().trimmed(),
            .album = object.value("album").toString().trimmed(),
            .providerId = providerId,
            .playbackUrl = QUrl{object.value("playbackUrl").toString()},
            .durationMilliseconds = object.value("durationMilliseconds").toInteger()};
        const auto duplicate = std::ranges::any_of(m_items, [&](const Item& existing) {
            return existing.providerId == providerId && existing.id == item.id;
        });
        if (!item.id.isEmpty() && !item.title.isEmpty() && !duplicate) {
            additions.push_back(std::move(item));
        }
    }
    if (!additions.empty()) {
        const auto first = static_cast<int>(m_items.size());
        const auto last = first + static_cast<int>(additions.size()) - 1;
        beginInsertRows({}, first, last);
        m_items.insert(m_items.end(), std::make_move_iterator(additions.begin()),
            std::make_move_iterator(additions.end()));
        endInsertRows();
        emit countChanged();
    }
}

void ProviderGateway::finishRequest(const QString& key)
{
    m_requests.remove(key);
    m_pending = std::max(0, m_pending - 1);
}

QString ProviderGateway::requestKey(const QString& providerId, const quint64 requestId)
{
    return providerId + ':' + QString::number(requestId);
}

} // namespace yaap
