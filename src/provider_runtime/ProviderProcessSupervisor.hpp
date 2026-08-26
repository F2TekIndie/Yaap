#pragma once

#include "provider_protocol/ProviderProtocolCodec.hpp"

#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QPointer>
#include <QProcess>
#include <QSet>
#include <QTimer>

namespace yaap {

class ProviderProcessSupervisor final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool ready READ isReady NOTIFY readyChanged)
    Q_PROPERTY(QString providerId READ providerId NOTIFY readyChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

public:
    explicit ProviderProcessSupervisor(QObject* parent = nullptr);
    ~ProviderProcessSupervisor() override;

    ProviderProcessSupervisor(const ProviderProcessSupervisor&) = delete;
    ProviderProcessSupervisor& operator=(const ProviderProcessSupervisor&) = delete;

    bool start(
        const QString& executable,
        const QString& expectedProviderId,
        const QStringList& grantedPermissions,
        QString& error);
    quint64 sendRequest(const QString& method, const QJsonObject& parameters = {});
    void cancel(quint64 requestId);
    bool sendHostResponse(
        quint64 requestId,
        const QJsonValue& result,
        const QJsonObject& error = {});
    void stop();

    [[nodiscard]] bool isReady() const noexcept;
    [[nodiscard]] QString providerId() const;
    [[nodiscard]] QString errorMessage() const;
    [[nodiscard]] QString diagnosticTail() const;

signals:
    void readyChanged();
    void errorMessageChanged();
    void responseReceived(quint64 requestId, const QJsonValue& result, const QJsonObject& error);
    void hostRequestReceived(
        quint64 requestId,
        const QString& method,
        const QJsonObject& parameters);
    void processStopped();

private:
    void acceptConnection();
    void readMessages();
    void handleMessage(const QJsonObject& message);
    bool sendMessage(const QJsonObject& message);
    void fail(QString error);
    void cleanupSocket();

    QLocalServer m_server;
    QProcess m_process;
    QPointer<QLocalSocket> m_socket;
    QTimer m_startupTimer;
    ProviderProtocolCodec m_codec;
    QString m_serverName;
    QString m_nonce;
    QString m_expectedProviderId;
    QString m_providerId;
    QStringList m_grantedPermissions;
    QString m_errorMessage;
    QByteArray m_diagnosticTail;
    quint64 m_nextRequestId{1};
    bool m_ready{};
    bool m_stopping{};
    QSet<quint64> m_outstandingRequests;
    void* m_nativeContainmentHandle{};
};

} // namespace yaap
