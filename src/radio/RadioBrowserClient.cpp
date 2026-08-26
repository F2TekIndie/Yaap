#include "radio/RadioBrowserClient.hpp"

#include <QCoreApplication>
#include <QDnsLookup>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTimer>
#include <QUrlQuery>

#include <algorithm>
#include <memory>
#include <utility>

namespace yaap {
namespace {

constexpr qsizetype maximumResponseBytes = 2 * 1024 * 1024;
constexpr std::size_t maximumStations = 200;
constexpr int requestTimeoutMilliseconds = 12'000;

[[nodiscard]] QString boundedString(const QJsonObject& object, const char* key,
    const qsizetype maximumLength = 512)
{
    return object.value(QLatin1String{key}).toString().trimmed().left(maximumLength);
}

[[nodiscard]] QUrl webUrl(const QString& value)
{
    const QUrl url{value};
    return url.isValid() && (url.scheme() == "http" || url.scheme() == "https")
        ? url : QUrl{};
}

[[nodiscard]] bool validStationUuid(const QString& value)
{
    static const QRegularExpression expression{
        QStringLiteral("^[A-Fa-f0-9-]{8,64}$")};
    return expression.match(value).hasMatch();
}

} // namespace

RadioBrowserResult RadioBrowserParser::parseStations(const QByteArray& payload)
{
    if (payload.isEmpty()) {
        return {.error = "Radio Browser returned an empty response."};
    }
    if (payload.size() > maximumResponseBytes) {
        return {.error = "Radio Browser response exceeds the 2 MiB safety limit."};
    }

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return {.error = "Invalid Radio Browser response: " + parseError.errorString()};
    }
    if (!document.isArray()) {
        return {.error = "Invalid Radio Browser response: expected a station array."};
    }

    RadioBrowserResult result;
    result.payload = payload;
    const auto values = document.array();
    result.stations.reserve(std::min<std::size_t>(
        static_cast<std::size_t>(values.size()), maximumStations));
    for (const auto& value : values) {
        if (result.stations.size() >= maximumStations || !value.isObject()) {
            break;
        }
        const auto object = value.toObject();
        const auto uuid = boundedString(object, "stationuuid", 64);
        auto streamUrl = webUrl(boundedString(object, "url_resolved", 4096));
        if (streamUrl.isEmpty()) {
            streamUrl = webUrl(boundedString(object, "url", 4096));
        }
        if (!validStationUuid(uuid) || streamUrl.isEmpty()) {
            continue;
        }

        auto name = boundedString(object, "name");
        if (name.isEmpty()) {
            name = streamUrl.host();
        }
        result.stations.push_back({
            uuid,
            std::move(name),
            std::move(streamUrl),
            webUrl(boundedString(object, "homepage", 4096)),
            webUrl(boundedString(object, "favicon", 4096)),
            boundedString(object, "countrycode", 2).toUpper(),
            boundedString(object, "language"),
            boundedString(object, "tags", 1024),
            boundedString(object, "codec", 64),
            std::max(0, object.value("bitrate").toInt()),
            object.value("hls").toInt() == 1,
        });
    }
    return result;
}

RadioBrowserClient::RadioBrowserClient(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

RadioBrowserClient::~RadioBrowserClient()
{
    cancel();
}

void RadioBrowserClient::fetchPopular(
    const QString& countryCode, const int limit, Callback callback)
{
    QList<QPair<QString, QString>> parameters{
        {"hidebroken", "true"},
        {"order", "clickcount"},
        {"reverse", "true"},
        {"limit", QString::number(std::clamp(limit, 1, 200))},
    };
    const auto normalizedCountry = countryCode.trimmed().toUpper();
    if (normalizedCountry.size() == 2) {
        parameters.push_back({"countrycode", normalizedCountry});
    }
    fetch("/json/stations/search", parameters, std::move(callback));
}

void RadioBrowserClient::searchByName(
    const QString& name, const int limit, Callback callback)
{
    const auto term = name.trimmed().left(200);
    if (term.isEmpty()) {
        callback({.error = "Enter a station name to search."});
        return;
    }
    fetch("/json/stations/search",
        {{"name", term},
            {"hidebroken", "true"},
            {"order", "clickcount"},
            {"reverse", "true"},
            {"limit", QString::number(std::clamp(limit, 1, 200))}},
        std::move(callback));
}

void RadioBrowserClient::recordClick(const QString& stationUuid)
{
    if (!validStationUuid(stationUuid)) {
        return;
    }
    m_pendingClicks.push_back(stationUuid);
    ensureMirrors([this] { sendPendingClicks(); });
}

void RadioBrowserClient::cancel()
{
    ++m_generation;
    if (m_activeReply) {
        m_activeReply->abort();
        m_activeReply.clear();
    }
}

void RadioBrowserClient::fetch(const QString& path,
    const QList<QPair<QString, QString>>& parameters, Callback callback)
{
    cancel();
    const auto generation = m_generation;
    ensureMirrors([this, generation, path, parameters,
                      callback = std::move(callback)]() mutable {
        if (generation != m_generation) {
            return;
        }
        requestFromMirror(generation, path, parameters, 0, std::move(callback));
    });
}

void RadioBrowserClient::ensureMirrors(ReadyCallback callback)
{
    if (!m_mirrors.isEmpty()) {
        QTimer::singleShot(0, this, std::move(callback));
        return;
    }
    m_mirrorWaiters.push_back(std::move(callback));
    if (m_dnsLookup) {
        return;
    }

    m_dnsLookup = new QDnsLookup{
        QDnsLookup::SRV, QStringLiteral("_api._tcp.radio-browser.info"), this};
    connect(m_dnsLookup, &QDnsLookup::finished,
        this, &RadioBrowserClient::finishMirrorLookup);
    m_dnsLookup->lookup();
}

void RadioBrowserClient::finishMirrorLookup()
{
    if (m_dnsLookup && m_dnsLookup->error() == QDnsLookup::NoError) {
        for (const auto& record : m_dnsLookup->serviceRecords()) {
            auto target = record.target();
            if (target.endsWith('.')) {
                target.chop(1);
            }
            QUrl mirror{"https://" + target};
            if (record.port() != 0 && record.port() != 443) {
                mirror.setPort(record.port());
            }
            if (mirror.isValid() && !target.isEmpty()) {
                m_mirrors.push_back(std::move(mirror));
            }
        }
    }
    if (m_dnsLookup) {
        m_dnsLookup->deleteLater();
        m_dnsLookup.clear();
    }

    if (m_mirrors.size() > 1) {
        const auto offset = QRandomGenerator::global()->bounded(m_mirrors.size());
        std::rotate(m_mirrors.begin(), m_mirrors.begin() + offset, m_mirrors.end());
    }
    if (m_mirrors.isEmpty()) {
        // Operational fallback for resolvers that block SRV lookups. This remains
        // a load-balanced Radio Browser hostname, not a pinned project mirror.
        m_mirrors.push_back(QUrl{"https://all.api.radio-browser.info"});
    }

    auto waiters = std::exchange(m_mirrorWaiters, {});
    for (auto& waiter : waiters) {
        waiter();
    }
}

void RadioBrowserClient::requestFromMirror(const quint64 generation,
    const QString& path, const QList<QPair<QString, QString>>& parameters,
    const qsizetype mirrorIndex, Callback callback, QString lastError)
{
    if (generation != m_generation) {
        return;
    }
    if (mirrorIndex >= m_mirrors.size()) {
        callback({.error = lastError.isEmpty()
                ? QString{"No Radio Browser server is available."}
                : std::move(lastError)});
        return;
    }

    QNetworkRequest request{requestUrl(m_mirrors[mirrorIndex], path, parameters)};
    request.setTransferTimeout(requestTimeoutMilliseconds);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader,
        QStringLiteral("Yaap/%1").arg(QCoreApplication::applicationVersion()));
    auto* reply = m_network->get(request);
    m_activeReply = reply;
    auto payload = std::make_shared<QByteArray>();
    auto oversized = std::make_shared<bool>(false);
    connect(reply, &QNetworkReply::readyRead, this, [reply, payload, oversized] {
        payload->append(reply->readAll());
        if (payload->size() > maximumResponseBytes) {
            *oversized = true;
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this,
        [this, reply, payload, oversized, generation, path, parameters,
            mirrorIndex, callback = std::move(callback)]() mutable {
            if (m_activeReply == reply) {
                m_activeReply.clear();
            }
            if (generation != m_generation) {
                reply->deleteLater();
                return;
            }
            payload->append(reply->readAll());
            const auto status = reply->attribute(
                QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const auto networkFailed = reply->error() != QNetworkReply::NoError
                || status >= 400;
            const auto networkError = *oversized
                ? QString{"Radio Browser response exceeds the 2 MiB safety limit."}
                : (status >= 400
                        ? QString{"Radio Browser returned HTTP %1."}.arg(status)
                        : reply->errorString());
            const auto etag = QString::fromUtf8(reply->rawHeader("ETag"));
            reply->deleteLater();

            if (networkFailed) {
                requestFromMirror(generation, path, parameters, mirrorIndex + 1,
                    std::move(callback), networkError);
                return;
            }
            auto result = RadioBrowserParser::parseStations(*payload);
            if (!result.succeeded()) {
                requestFromMirror(generation, path, parameters, mirrorIndex + 1,
                    std::move(callback), result.error);
                return;
            }
            result.etag = etag;
            callback(std::move(result));
        });
}

QUrl RadioBrowserClient::requestUrl(const QUrl& mirror, const QString& path,
    const QList<QPair<QString, QString>>& parameters) const
{
    auto url = mirror;
    url.setPath(path);
    QUrlQuery query;
    for (const auto& [name, value] : parameters) {
        query.addQueryItem(name, value);
    }
    url.setQuery(query);
    return url;
}

void RadioBrowserClient::sendPendingClicks()
{
    if (m_mirrors.isEmpty()) {
        return;
    }
    const auto clicks = std::exchange(m_pendingClicks, {});
    for (const auto& uuid : clicks) {
        QNetworkRequest request{requestUrl(m_mirrors.front(),
            "/json/url/" + uuid, {})};
        request.setTransferTimeout(5'000);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
            QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setHeader(QNetworkRequest::UserAgentHeader,
            QStringLiteral("Yaap/%1").arg(QCoreApplication::applicationVersion()));
        auto* reply = m_network->get(request);
        connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
    }
}

} // namespace yaap
