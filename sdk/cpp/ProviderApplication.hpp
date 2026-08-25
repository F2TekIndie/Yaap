#pragma once

#include "provider_protocol/ProviderProtocolCodec.hpp"

#include <QLocalSocket>
#include <QObject>
#include <QSet>
#include <QString>

namespace yaap::sdk {

class ProviderService;

class ProviderApplication final : public QObject {
    Q_OBJECT

public:
    ProviderApplication(ProviderService& service, QObject* parent = nullptr);
    bool start(const QStringList& arguments, QString& error);

signals:
    void fatalError(const QString& error);

private:
    void connected();
    void readMessages();
    void handleMessage(const QJsonObject& message);
    void respond(quint64 requestId, const QJsonValue& result, const QString& error);
    bool send(const QJsonObject& message);
    [[nodiscard]] static QString argumentValue(
        const QStringList& arguments,
        const QString& name);

    ProviderService& m_service;
    QLocalSocket m_socket;
    ProviderProtocolCodec m_codec;
    QString m_nonce;
    bool m_hostAccepted{};
    QSet<quint64> m_outstandingRequests;
};

} // namespace yaap::sdk
