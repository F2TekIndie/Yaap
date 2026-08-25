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

namespace yaap {
namespace {

constexpr int requestTimeoutMilliseconds = 15'000;

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
    request.setHeader(QNetworkRequest::UserAgentHeader, "Yaap/0.1");
    auto* reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [reply, callback = std::move(callback)]() mutable {
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
    request.setHeader(QNetworkRequest::UserAgentHeader, "Yaap/0.1");
    auto* reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this,
        [reply, callback = std::move(callback), server = m_configuration.serverUrl, authentication]() mutable {
            if (reply->error() != QNetworkReply::NoError) {
                auto error = replyError(*reply);
                reply->deleteLater();
                callback({.error = std::move(error)});
                return;
            }
            auto result = parseSearchResponse(reply->readAll(), server, authentication);
            reply->deleteLater();
            callback(std::move(result));
        });
}

ProviderTracksResult OpenSubsonicClient::parseSearchResponse(
    const QByteArray& json,
    const QUrl& serverUrl,
    const QUrlQuery& authentication)
{
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
    result.setHeader(QNetworkRequest::UserAgentHeader, "Yaap/0.1");
    auto authorization = QByteArray{"MediaBrowser Client=\"Yaap\", Device=\"Desktop\", "
        "DeviceId=\"YaapDesktop\", Version=\"0.1\""};
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
    connect(reply, &QNetworkReply::finished, this, [this, reply, callback = std::move(callback)]() mutable {
        if (reply->error() != QNetworkReply::NoError) {
            auto error = replyError(*reply);
            reply->deleteLater();
            callback(false, std::move(error));
            return;
        }
        QJsonParseError parseError;
        const auto body = QJsonDocument::fromJson(reply->readAll(), &parseError).object();
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
    if (m_accessToken.isEmpty() || m_userId.isEmpty()) {
        callback({.error = "Authenticate with Jellyfin before loading tracks."});
        return;
    }
    auto url = endpoint("/Users/" + m_userId + "/Items");
    QUrlQuery query;
    query.addQueryItem("IncludeItemTypes", "Audio");
    query.addQueryItem("Recursive", "true");
    query.addQueryItem("Fields", "Album,Artists,RunTimeTicks");
    url.setQuery(query);
    auto* reply = m_network.get(request(url));
    connect(reply, &QNetworkReply::finished, this,
        [reply, callback = std::move(callback), server = m_configuration.serverUrl,
            token = m_accessToken]() mutable {
            if (reply->error() != QNetworkReply::NoError) {
                auto error = replyError(*reply);
                reply->deleteLater();
                callback({.error = std::move(error)});
                return;
            }
            auto result = parseItemsResponse(reply->readAll(), server, token);
            reply->deleteLater();
            callback(std::move(result));
        });
}

ProviderTracksResult JellyfinClient::parseItemsResponse(
    const QByteArray& json,
    const QUrl& serverUrl,
    const QString& accessToken)
{
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return {.error = jsonError(parseError)};
    }
    ProviderTracksResult result;
    for (const auto& value : document.object().value("Items").toArray()) {
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
