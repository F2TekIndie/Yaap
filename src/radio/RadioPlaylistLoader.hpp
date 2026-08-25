#pragma once

#include "radio/RadioPlaylist.hpp"

#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>

#include <functional>

namespace yaap {

class RadioPlaylistLoader final : public QObject {
    Q_OBJECT

public:
    using Callback = std::function<void(RadioPlaylistResult)>;

    explicit RadioPlaylistLoader(QObject* parent = nullptr);
    void load(const QUrl& url, Callback callback);

private:
    QNetworkAccessManager m_network;
};

} // namespace yaap
