#pragma once

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

#include <functional>
#include <vector>

class QDnsLookup;
class QNetworkAccessManager;
class QNetworkReply;

namespace yaap {

struct RadioBrowserStation final {
    QString stationUuid;
    QString name;
    QUrl streamUrl;
    QUrl homepageUrl;
    QUrl faviconUrl;
    QString countryCode;
    QString language;
    QString tags;
    QString codec;
    int bitrate{};
    bool hls{};
};

struct RadioBrowserResult final {
    std::vector<RadioBrowserStation> stations;
    QByteArray payload;
    QString etag;
    QString error;

    [[nodiscard]] bool succeeded() const noexcept { return error.isEmpty(); }
};

class RadioBrowserParser final {
public:
    static RadioBrowserResult parseStations(const QByteArray& payload);
};

class RadioBrowserClient final : public QObject {
    Q_OBJECT

public:
    using Callback = std::function<void(RadioBrowserResult)>;

    explicit RadioBrowserClient(QObject* parent = nullptr);
    ~RadioBrowserClient() override;

    void fetchPopular(const QString& countryCode, int limit, Callback callback);
    void searchByName(const QString& name, int limit, Callback callback);
    void recordClick(const QString& stationUuid);
    void cancel();

private:
    using ReadyCallback = std::function<void()>;

    void fetch(const QString& path, const QList<QPair<QString, QString>>& parameters,
        Callback callback);
    void ensureMirrors(ReadyCallback callback);
    void finishMirrorLookup();
    void requestFromMirror(quint64 generation, const QString& path,
        const QList<QPair<QString, QString>>& parameters, qsizetype mirrorIndex,
        Callback callback, QString lastError = {});
    [[nodiscard]] QUrl requestUrl(const QUrl& mirror, const QString& path,
        const QList<QPair<QString, QString>>& parameters) const;
    void sendPendingClicks();

    QNetworkAccessManager* m_network{};
    QPointer<QNetworkReply> m_activeReply;
    QPointer<QDnsLookup> m_dnsLookup;
    QList<QUrl> m_mirrors;
    std::vector<ReadyCallback> m_mirrorWaiters;
    QStringList m_pendingClicks;
    quint64 m_generation{};
};

} // namespace yaap
