#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QJsonObject>
#include <QUrl>

#include <vector>

namespace yaap {

class ProviderExtensionManager;
class ProviderCache;
class ProviderAccountStore;

class ProviderGateway final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY stateChanged)
    Q_PROPERTY(int availableProviderCount READ availableProviderCount NOTIFY stateChanged)
    Q_PROPERTY(bool offlineMode READ offlineMode WRITE setOfflineMode NOTIFY stateChanged)

public:
    enum Role {
        TrackIdRole = Qt::UserRole + 1,
        TitleRole,
        ArtistRole,
        AlbumRole,
        ProviderIdRole,
        DurationRole,
    };
    Q_ENUM(Role)

    ProviderGateway(
        ProviderExtensionManager& providers,
        ProviderCache& cache,
        ProviderAccountStore& accounts,
        QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] int count() const noexcept;
    [[nodiscard]] bool loading() const noexcept;
    [[nodiscard]] QString errorMessage() const;
    [[nodiscard]] int availableProviderCount() const;
    [[nodiscard]] bool offlineMode() const noexcept;
    void setOfflineMode(bool offline);

    Q_INVOKABLE void search(const QString& query);
    Q_INVOKABLE void browse(const QString& providerId, const QString& cursor = {});
    Q_INVOKABLE void play(int row);
    Q_INVOKABLE void cancel();

signals:
    void countChanged();
    void stateChanged();
    void playbackRequested(const QUrl& url, const QString& title);

private:
    struct Item final {
        QString id;
        QString title;
        QString artist;
        QString album;
        QString providerId;
        QUrl playbackUrl;
        qint64 durationMilliseconds{};
    };
    enum class RequestKind { Items, Resolve };
    struct RequestContext final {
        quint64 generation{};
        RequestKind kind{RequestKind::Items};
        Item item;
        QString cacheKey;
    };

    void startItemsRequest(
        const QString& providerId,
        const QString& method,
        const QJsonObject& parameters,
        QString cacheKey = {});
    void handleResponse(
        const QString& providerId,
        quint64 requestId,
        const QJsonValue& result,
        const QJsonObject& error);
    void finishRequest(const QString& key);
    void appendItems(const QString& providerId, const QJsonObject& result);
    [[nodiscard]] static QString requestKey(const QString& providerId, quint64 requestId);

    ProviderExtensionManager& m_providers;
    ProviderCache& m_cache;
    ProviderAccountStore& m_accounts;
    std::vector<Item> m_items;
    QHash<QString, RequestContext> m_requests;
    QString m_errorMessage;
    quint64 m_generation{};
    int m_pending{};
    bool m_offlineMode{};
};

} // namespace yaap
