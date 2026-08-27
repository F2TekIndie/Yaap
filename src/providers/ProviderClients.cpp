#include "providers/ProviderClients.hpp"

#include "security/CredentialStore.hpp"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QUrlQuery>

#include <utility>
#include <memory>
#include <iterator>

namespace yaap {
namespace {

constexpr int requestTimeoutMilliseconds = 15'000;
constexpr qsizetype maximumProviderResponseBytes = 4 * 1024 * 1024;
constexpr int jellyfinPageSize = 250;
constexpr std::size_t maximumJellyfinTracks = 20'000;

struct BoundedReplyBody final {
    QByteArray bytes;
    bool oversized{};
};

void consumeReplyData(QNetworkReply& reply, BoundedReplyBody& body)
{
    const auto chunk = reply.readAll();
    const auto remaining = maximumProviderResponseBytes - body.bytes.size();
    if (chunk.size() > remaining) {
        if (remaining > 0) {
            body.bytes.append(chunk.first(remaining));
        }
        body.oversized = true;
        reply.abort();
        return;
    }
    body.bytes.append(chunk);
}

[[nodiscard]] std::shared_ptr<BoundedReplyBody> collectReplyBody(
    QNetworkReply& reply, QObject& context)
{
    auto body = std::make_shared<BoundedReplyBody>();
    QObject::connect(&reply, &QNetworkReply::readyRead, &context,
        [&reply, body] { consumeReplyData(reply, *body); });
    return body;
}

[[nodiscard]] QString oversizedReplyError()
{
    return "Provider response exceeds the 4 MiB safety limit.";
}

[[nodiscard]] QString replyError(QNetworkReply& reply)
{
    const auto status = reply.attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    return status > 0
        ? QString{"HTTP %1: %2"}.arg(status).arg(reply.errorString())
        : reply.errorString();
}

[[nodiscard]] QUrl appendPath(QUrl base, const QString& suffix)
{
    auto path = base.path();
    if (path.endsWith('/')) {
        path.chop(1);
    }
    base.setPath(path + suffix);
    return base;
}

[[nodiscard]] QString jsonError(const QJsonParseError& error)
{
    return "Invalid provider response: " + error.errorString();
}

[[nodiscard]] std::string text(const QJsonObject& object, const char* key)
{
    return object.value(QLatin1String{key}).toString().toStdString();
}

} // namespace

OpenSubsonicClient::OpenSubsonicClient(CredentialStore& credentials, QObject* parent)
    : QObject(parent)
    , m_credentials(credentials)
{
}

void OpenSubsonicClient::setConfiguration(OpenSubsonicConfiguration configuration)
{
    m_configuration = std::move(configuration);
}

bool OpenSubsonicClient::authenticationQuery(QUrlQuery& query, QString& error) const
{
    if (!m_configuration.serverUrl.isValid() || m_configuration.username.isEmpty()) {
        error = "OpenSubsonic server URL and username are required.";
        return false;
    }
    const auto secret = m_credentials.load(m_configuration.credentialKey, error);
    if (!secret) {
        if (error.isEmpty()) {
            error = "OpenSubsonic credential was not found.";
        }
        return false;
    }

    const auto salt = QByteArray::number(QRandomGenerator::global()->generate64(), 16);
    const auto token = QCryptographicHash::hash(*secret + salt, QCryptographicHash::Md5).toHex();
    query.addQueryItem("u", m_configuration.username);
    query.addQueryItem("s", QString::fromLatin1(salt));
    query.addQueryItem("t", QString::fromLatin1(token));
    query.addQueryItem("v", "1.16.1");
    query.addQueryItem("c", "Yaap");
    query.addQueryItem("f", "json");
    return true;
}

QUrl OpenSubsonicClient::endpoint(const QString& path, const QUrlQuery& query) const
{
    auto url = appendPath(m_configuration.serverUrl, "/rest/" + path);
    url.setQuery(query);
    return url;
}

void OpenSubsonicClient::ping(StatusCallback callback)
{
    QUrlQuery query;
    QString error;
    if (!authenticationQuery(query, error)) {
        callback(false, std::move(error));
        return;
    }
    QNetworkRequest request{endpoint("ping", query)};
    request.setTransferTimeout(requestTimeoutMilliseconds);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Yaap/0.2");
    auto* reply = m_network.get(request);
    auto body = collectReplyBody(*reply, *this);
    connect(reply, &QNetworkReply::finished, this, [reply, body, callback = std::move(callback)]() mutable {
        consumeReplyData(*reply, *body);
        if (body->oversized) {
            reply->deleteLater();
            callback(false, oversizedReplyError());
            return;
        }
        const auto successful = reply->error() == QNetworkReply::NoError;
        auto error = successful ? QString{} : replyError(*reply);
        reply->deleteLater();
        callback(successful, std::move(error));
    });
}

void OpenSubsonicClient::search(const QString& searchText, TracksCallback callback)
{
    QUrlQuery query;
    QString error;
    if (!authenticationQuery(query, error)) {
        callback({.error = std::move(error)});
        return;
    }
    query.addQueryItem("query", searchText);
    query.addQueryItem("songCount", "100");
    query.addQueryItem("albumCount", "0");
    query.addQueryItem("artistCount", "0");
    const auto authentication = query;
    QNetworkRequest request{endpoint("search3", query)};
    request.setTransferTimeout(requestTimeoutMilliseconds);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Yaap/0.2");
    auto* reply = m_network.get(request);
    auto body = collectReplyBody(*reply, *this);
    connect(reply, &QNetworkReply::finished, this,
        [reply, body, callback = std::move(callback), server = m_configuration.serverUrl, authentication]() mutable {
            consumeReplyData(*reply, *body);
            if (body->oversized) {
                reply->deleteLater();
                callback({.error = oversizedReplyError()});
                return;
            }
            if (reply->error() != QNetworkReply::NoError) {
                auto error = replyError(*reply);
                reply->deleteLater();
                callback({.error = std::move(error)});
                return;
            }
            auto result = parseSearchResponse(body->bytes, server, authentication);
            reply->deleteLater();
            callback(std::move(result));
        });
}

ProviderTracksResult OpenSubsonicClient::parseSearchResponse(
    const QByteArray& json,
    const QUrl& serverUrl,
    const QUrlQuery& authentication)
{
    if (json.size() > maximumProviderResponseBytes) {
        return {.error = oversizedReplyError()};
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return {.error = jsonError(parseError)};
    }
    const auto root = document.object().value("subsonic-response").toObject();
    if (root.value("status").toString() != "ok") {
        return {.error = root.value("error").toObject().value("message").toString(
            "OpenSubsonic server rejected the request.")};
    }

    ProviderTracksResult result;
    const auto songs = root.value("searchResult3").toObject().value("song").toArray();
    for (const auto& value : songs) {
        const auto song = value.toObject();
        const auto id = song.value("id").toString();
        QUrlQuery streamQuery = authentication;
        streamQuery.removeAllQueryItems("query");
        streamQuery.removeAllQueryItems("songCount");
        streamQuery.removeAllQueryItems("albumCount");
        streamQuery.removeAllQueryItems("artistCount");
        streamQuery.addQueryItem("id", id);
        auto streamUrl = appendPath(serverUrl, "/rest/stream");
        streamUrl.setQuery(streamQuery);
        result.tracks.push_back({
            .id = "opensubsonic:" + id.toStdString(),
            .kind = TrackKind::OpenSubsonic,
            .providerId = "opensubsonic",
            .source = streamUrl.toString(QUrl::FullyEncoded).toStdString(),
            .title = text(song, "title"),
            .artist = text(song, "artist"),
            .album = text(song, "album"),
            .durationMilliseconds = song.value("duration").toInteger() * 1'000});
    }
    return result;
}

JellyfinClient::JellyfinClient(CredentialStore& credentials, QObject* parent)
    : QObject(parent)
    , m_credentials(credentials)
{
}

struct JellyfinClient::FetchState final {
    QString query;
    int startIndex{};
    std::vector<Track> tracks;
    TracksCallback callback;
};

void JellyfinClient::setConfiguration(JellyfinConfiguration configuration)
{
    m_configuration = std::move(configuration);
    m_accessToken.clear();
    m_userId.clear();
}

QUrl JellyfinClient::endpoint(const QString& path) const
{
    return appendPath(m_configuration.serverUrl, path);
}

QNetworkRequest JellyfinClient::request(const QUrl& url) const
{
    QNetworkRequest result{url};
    result.setTransferTimeout(requestTimeoutMilliseconds);
    result.setHeader(QNetworkRequest::UserAgentHeader, "Yaap/0.2");
    auto authorization = QByteArray{"MediaBrowser Client=\"Yaap\", Device=\"Desktop\", "
        "DeviceId=\"YaapDesktop\", Version=\"0.2\""};
    if (!m_accessToken.isEmpty()) {
        authorization += ", Token=\"" + m_accessToken.toUtf8() + '"';
    }
    result.setRawHeader("Authorization", authorization);
    return result;
}

void JellyfinClient::authenticate(StatusCallback callback)
{
    QString error;
    if (!m_configuration.serverUrl.isValid() || m_configuration.username.isEmpty()) {
        callback(false, "Jellyfin server URL and username are required.");
        return;
    }
    const auto password = m_credentials.load(m_configuration.credentialKey, error);
    if (!password) {
        callback(false, error.isEmpty() ? "Jellyfin credential was not found." : error);
        return;
    }
    const QJsonObject body{{"Username", m_configuration.username},
        {"Pw", QString::fromUtf8(*password)}};
    auto networkRequest = request(endpoint("/Users/AuthenticateByName"));
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    auto* reply = m_network.post(networkRequest, QJsonDocument{body}.toJson(QJsonDocument::Compact));
    auto responseBody = collectReplyBody(*reply, *this);
    connect(reply, &QNetworkReply::finished, this,
        [this, reply, responseBody, callback = std::move(callback)]() mutable {
        consumeReplyData(*reply, *responseBody);
        if (responseBody->oversized) {
            reply->deleteLater();
            callback(false, oversizedReplyError());
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            auto error = replyError(*reply);
            reply->deleteLater();
            callback(false, std::move(error));
            return;
        }
        QJsonParseError parseError;
        const auto body = QJsonDocument::fromJson(responseBody->bytes, &parseError).object();
        reply->deleteLater();
        m_accessToken = body.value("AccessToken").toString();
        m_userId = body.value("User").toObject().value("Id").toString();
        const auto successful = parseError.error == QJsonParseError::NoError
            && !m_accessToken.isEmpty() && !m_userId.isEmpty();
        callback(successful, successful ? QString{} : "Invalid Jellyfin authentication response.");
    });
}

void JellyfinClient::fetchTracks(TracksCallback callback)
{
    search({}, std::move(callback));
}

void JellyfinClient::search(const QString& query, TracksCallback callback)
{
    if (m_accessToken.isEmpty() || m_userId.isEmpty()) {
        callback({.error = "Authenticate with Jellyfin before loading tracks."});
        return;
    }
    auto state = std::make_shared<FetchState>();
    state->query = query.trimmed().left(200);
    state->callback = std::move(callback);
    state->tracks.reserve(jellyfinPageSize);
    fetchTrackPage(state);
}

void JellyfinClient::fetchTrackPage(const std::shared_ptr<FetchState>& state)
{
    auto url = endpoint("/Users/" + m_userId + "/Items");
    QUrlQuery query;
    query.addQueryItem("IncludeItemTypes", "Audio");
    query.addQueryItem("Recursive", "true");
    query.addQueryItem("Fields", "Album,Artists,RunTimeTicks");
    query.addQueryItem("StartIndex", QString::number(state->startIndex));
    query.addQueryItem("Limit", QString::number(jellyfinPageSize));
    if (!state->query.isEmpty()) {
        query.addQueryItem("SearchTerm", state->query);
    }
    url.setQuery(query);
    auto* reply = m_network.get(request(url));
    auto body = collectReplyBody(*reply, *this);
    connect(reply, &QNetworkReply::finished, this,
        [this, reply, body, state, server = m_configuration.serverUrl,
            token = m_accessToken]() mutable {
            consumeReplyData(*reply, *body);
            if (body->oversized) {
                reply->deleteLater();
                state->callback({.error = oversizedReplyError()});
                return;
            }
            if (reply->error() != QNetworkReply::NoError) {
                auto error = replyError(*reply);
                reply->deleteLater();
                state->callback({.error = std::move(error)});
                return;
            }
            QJsonParseError parseError;
            const auto document = QJsonDocument::fromJson(body->bytes, &parseError);
            const auto itemCount = document.object().value("Items").toArray().size();
            const auto totalCount = document.object().value("TotalRecordCount").toInteger(-1);
            auto result = parseItemsResponse(body->bytes, server, token);
            reply->deleteLater();
            if (!result.succeeded()) {
                state->callback(std::move(result));
                return;
            }
            if (state->tracks.size() + result.tracks.size() > maximumJellyfinTracks) {
                state->callback({.error =
                    "Jellyfin search exceeds the 20000-track safety limit; narrow the query."});
                return;
            }
            state->tracks.insert(state->tracks.end(),
                std::make_move_iterator(result.tracks.begin()),
                std::make_move_iterator(result.tracks.end()));
            state->startIndex += itemCount;
            if (itemCount > 0
                && ((totalCount >= 0 && state->startIndex < totalCount)
                    || (totalCount < 0 && itemCount == jellyfinPageSize))) {
                fetchTrackPage(state);
                return;
            }
            state->callback({.tracks = std::move(state->tracks)});
        });
}

ProviderTracksResult JellyfinClient::parseItemsResponse(
    const QByteArray& json,
    const QUrl& serverUrl,
    const QString& accessToken)
{
    if (json.size() > maximumProviderResponseBytes) {
        return {.error = oversizedReplyError()};
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return {.error = jsonError(parseError)};
    }
    ProviderTracksResult result;
    const auto items = document.object().value("Items").toArray();
    if (items.size() > static_cast<qsizetype>(maximumJellyfinTracks)) {
        return {.error = "Jellyfin response contains too many tracks."};
    }
    result.tracks.reserve(static_cast<std::size_t>(items.size()));
    for (const auto& value : items) {
        const auto item = value.toObject();
        const auto id = item.value("Id").toString();
        auto streamUrl = appendPath(serverUrl, "/Audio/" + id + "/stream");
        QUrlQuery query;
        query.addQueryItem("api_key", accessToken);
        streamUrl.setQuery(query);
        const auto artists = item.value("Artists").toArray();
        result.tracks.push_back({
            .id = "jellyfin:" + id.toStdString(),
            .kind = TrackKind::Jellyfin,
            .providerId = "jellyfin",
            .source = streamUrl.toString(QUrl::FullyEncoded).toStdString(),
            .title = text(item, "Name"),
            .artist = artists.isEmpty() ? std::string{} : artists.first().toString().toStdString(),
            .album = text(item, "Album"),
            .durationMilliseconds = item.value("RunTimeTicks").toInteger() / 10'000});
    }
    return result;
}

} // namespace yaap
