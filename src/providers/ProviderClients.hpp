#pragma once

#include "domain/Track.hpp"

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QUrlQuery>

#include <functional>
#include <memory>
#include <vector>

namespace yaap {

class CredentialStore;

struct ProviderTracksResult final {
    std::vector<Track> tracks;
    QString error;

    [[nodiscard]] bool succeeded() const noexcept { return error.isEmpty(); }
};

struct OpenSubsonicConfiguration final {
    QUrl serverUrl;
    QString username;
    QString credentialKey;
};

class OpenSubsonicClient final : public QObject {
    Q_OBJECT

public:
    using StatusCallback = std::function<void(bool, QString)>;
    using TracksCallback = std::function<void(ProviderTracksResult)>;

    explicit OpenSubsonicClient(CredentialStore& credentials, QObject* parent = nullptr);
    void setConfiguration(OpenSubsonicConfiguration configuration);
    void ping(StatusCallback callback);
    void search(const QString& query, TracksCallback callback);

    [[nodiscard]] static ProviderTracksResult parseSearchResponse(
        const QByteArray& json,
        const QUrl& serverUrl,
        const QUrlQuery& authentication);

private:
    [[nodiscard]] bool authenticationQuery(QUrlQuery& query, QString& error) const;
    [[nodiscard]] QUrl endpoint(const QString& path, const QUrlQuery& query) const;

    CredentialStore& m_credentials;
    QNetworkAccessManager m_network;
    OpenSubsonicConfiguration m_configuration;
};

struct JellyfinConfiguration final {
    QUrl serverUrl;
    QString username;
    QString credentialKey;
};

class JellyfinClient final : public QObject {
    Q_OBJECT

public:
    using StatusCallback = std::function<void(bool, QString)>;
    using TracksCallback = std::function<void(ProviderTracksResult)>;

    explicit JellyfinClient(CredentialStore& credentials, QObject* parent = nullptr);
    void setConfiguration(JellyfinConfiguration configuration);
    void authenticate(StatusCallback callback);
    void fetchTracks(TracksCallback callback);
    void search(const QString& query, TracksCallback callback);

    [[nodiscard]] static ProviderTracksResult parseItemsResponse(
        const QByteArray& json,
        const QUrl& serverUrl,
        const QString& accessToken);

private:
    struct FetchState;

    [[nodiscard]] QNetworkRequest request(const QUrl& url) const;
    [[nodiscard]] QUrl endpoint(const QString& path) const;
    void fetchTrackPage(const std::shared_ptr<FetchState>& state);

    CredentialStore& m_credentials;
    QNetworkAccessManager m_network;
    JellyfinConfiguration m_configuration;
    QString m_accessToken;
    QString m_userId;
};

} // namespace yaap
