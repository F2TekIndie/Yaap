#pragma once

#include "radio/RadioBrowserClient.hpp"

#include <QAbstractListModel>
#include <QString>

#include <functional>
#include <vector>

namespace yaap {

class ProviderCache;

class RadioBrowserDirectoryModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool stale READ stale NOTIFY staleChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QString countryCode READ countryCode CONSTANT)

public:
    enum Role {
        StationUuidRole = Qt::UserRole + 1,
        NameRole,
        StreamUrlRole,
        HomepageUrlRole,
        FaviconUrlRole,
        CountryCodeRole,
        LanguageRole,
        TagsRole,
        CodecRole,
        BitrateRole,
        HlsRole,
    };
    Q_ENUM(Role)

    RadioBrowserDirectoryModel(RadioBrowserClient& client, ProviderCache& cache,
        QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] int count() const noexcept;
    [[nodiscard]] bool loading() const noexcept;
    [[nodiscard]] bool stale() const noexcept;
    [[nodiscard]] QString errorMessage() const;
    [[nodiscard]] QString countryCode() const;

    Q_INVOKABLE void refreshPopular();
    Q_INVOKABLE void search(const QString& name);
    Q_INVOKABLE void play(int row);
    Q_INVOKABLE void save(int row);
    Q_INVOKABLE void cancel();

signals:
    void countChanged();
    void loadingChanged();
    void staleChanged();
    void errorMessageChanged();
    void playbackRequested(const QUrl& url, const QString& title);
    void saveRequested(const yaap::RadioBrowserStation& station);

private:
    void request(QString cacheKey,
        std::function<void(RadioBrowserClient::Callback)> startRequest);
    bool restoreCache(const QString& key);
    void replaceStations(std::vector<RadioBrowserStation> stations);
    void setLoading(bool loading);
    void setStale(bool stale);
    void setError(QString error);

    RadioBrowserClient& m_client;
    ProviderCache& m_cache;
    std::vector<RadioBrowserStation> m_stations;
    QString m_countryCode;
    QString m_errorMessage;
    quint64 m_generation{};
    bool m_loading{};
    bool m_stale{};
};

} // namespace yaap
