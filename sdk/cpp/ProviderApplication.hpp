#pragma once

#include "provider_protocol/ProviderProtocolCodec.hpp"

#include <QLocalSocket>
#include <QObject>
#include <QSet>
#include <QHash>
#include <QString>

#include <functional>

namespace yaap::sdk {

class ProviderService;

class ProviderApplication final : public QObject {
    Q_OBJECT

public:
    using HostCompletion = std::function<void(QJsonValue result, QJsonObject error)>;
    ProviderApplication(ProviderService& service, QObject* parent = nullptr);
    bool start(const QStringList& arguments, QString& error);
    quint64 requestHost(
        const QString& method,
        const QJsonObject& parameters,
        HostCompletion completion);

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
    QHash<quint64, HostCompletion> m_hostRequests;
    quint64 m_nextHostRequestId{1};
};

} // namespace yaap::sdk
