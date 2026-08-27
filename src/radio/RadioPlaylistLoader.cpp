#include "radio/RadioPlaylistLoader.hpp"

#include <QNetworkReply>
#include <QNetworkRequest>

#include <memory>
#include <utility>

namespace yaap {

RadioPlaylistLoader::RadioPlaylistLoader(QObject* parent)
    : QObject(parent)
{
}

void RadioPlaylistLoader::load(const QUrl& url, Callback callback)
{
    if (!url.isValid() || (url.scheme() != "http" && url.scheme() != "https")) {
        callback({.error = "A valid HTTP or HTTPS radio playlist URL is required."});
        return;
    }
    QNetworkRequest request{url};
    request.setTransferTimeout(15'000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Yaap/0.2");
    auto* reply = m_network.get(request);
    auto content = std::make_shared<QByteArray>();
    connect(reply, &QNetworkReply::readyRead, this, [reply, content] {
        content->append(reply->readAll());
        if (content->size() > 2 * 1024 * 1024) {
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this,
        [reply, content, callback = std::move(callback), url]() mutable {
            if (reply->error() != QNetworkReply::NoError) {
                const auto oversized = content->size() > 2 * 1024 * 1024;
                auto error = oversized
                    ? QString{"Radio playlist exceeds the 2 MiB safety limit."}
                    : reply->errorString();
                reply->deleteLater();
                callback({.error = std::move(error)});
                return;
            }
            content->append(reply->readAll());
            const auto contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
            const auto finalUrl = reply->url().isValid() ? reply->url() : url;
            reply->deleteLater();
            callback(RadioPlaylistParser::parse(*content, finalUrl, contentType));
        });
}

} // namespace yaap
