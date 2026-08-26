#include "sdk/cpp/ProviderApplication.hpp"

#include "extension_api/ProviderProtocol.hpp"
#include "sdk/cpp/ProviderService.hpp"

#include <QCoreApplication>
#include <QJsonArray>
#include <QMetaObject>
#include <QPointer>

#include <utility>

namespace yaap::sdk {

ProviderApplication::ProviderApplication(ProviderService& service, QObject* parent)
    : QObject(parent)
    , m_service(service)
{
    connect(&m_socket, &QLocalSocket::connected, this, &ProviderApplication::connected);
    connect(&m_socket, &QLocalSocket::readyRead, this, &ProviderApplication::readMessages);
    connect(&m_socket, &QLocalSocket::errorOccurred, this, [this](QLocalSocket::LocalSocketError) {
        emit fatalError("Provider SDK socket error: " + m_socket.errorString());
    });
}

bool ProviderApplication::start(const QStringList& arguments, QString& error)
{
    const auto socketName = argumentValue(arguments, "--yaap-socket");
    m_nonce = argumentValue(arguments, "--yaap-nonce");
    bool protocolOk{};
    const auto protocol = argumentValue(arguments, "--yaap-api").toInt(&protocolOk);
    if (socketName.isEmpty() || m_nonce.isEmpty() || !protocolOk
        || protocol != providerProtocolVersion) {
        error = "Provider SDK launch arguments are missing or incompatible.";
        return false;
    }
    m_socket.connectToServer(socketName);
    return true;
}

quint64 ProviderApplication::requestHost(
    const QString& method,
    const QJsonObject& parameters,
    HostCompletion completion)
{
    constexpr qsizetype maximumOutstandingRequests = 128;
    if (!m_hostAccepted || method.isEmpty() || !completion
        || m_hostRequests.size() >= maximumOutstandingRequests) {
        return 0;
    }
    const auto requestId = m_nextHostRequestId++;
    if (!send({{"type", "request"}, {"protocol", providerProtocolVersion},
            {"id", QString::number(requestId)}, {"method", method}, {"params", parameters}})) {
        return 0;
    }
    m_hostRequests.insert(requestId, std::move(completion));
    return requestId;
}

void ProviderApplication::connected()
{
    send({{"type", provider_protocol::providerHello},
        {"protocol", providerProtocolVersion}, {"nonce", m_nonce},
        {"providerId", m_service.providerId()}, {"name", m_service.displayName()},
        {"version", m_service.version()},
        {"capabilities", QJsonArray::fromStringList(m_service.capabilities())}});
}

void ProviderApplication::readMessages()
{
    auto decoded = m_codec.append(m_socket.readAll());
    if (!decoded.error.isEmpty()) {
        emit fatalError(decoded.error);
        m_socket.abort();
        return;
    }
    for (const auto& message : decoded.messages) {
        handleMessage(message);
    }
}

void ProviderApplication::handleMessage(const QJsonObject& message)
{
    if (message.value("protocol").toInt(-1) != providerProtocolVersion) {
        emit fatalError("Host selected an incompatible provider protocol.");
        return;
    }
    const auto type = message.value("type").toString();
    if (!m_hostAccepted) {
        if (type != provider_protocol::hostHello) {
            emit fatalError("Provider SDK expected host.hello.");
            return;
        }
        m_hostAccepted = true;
        return;
    }
    if (type == "response") {
        bool idOk{};
        const auto requestId = message.value("id").toString().toULongLong(&idOk);
        if (!idOk || requestId == 0) {
            emit fatalError("Host response contains an invalid request ID.");
            return;
        }
        const auto iterator = m_hostRequests.find(requestId);
        if (iterator == m_hostRequests.end()) {
            emit fatalError("Host response refers to an unknown request.");
            return;
        }
        auto completion = std::move(iterator.value());
        m_hostRequests.erase(iterator);
        completion(message.value("result"), message.value("error").toObject());
        return;
    }
    if (type != "request") {
        emit fatalError("Provider SDK received an unexpected message type.");
        return;
    }
    bool idOk{};
    const auto requestId = message.value("id").toString().toULongLong(&idOk);
    const auto method = message.value("method").toString();
    if (!idOk || requestId == 0 || method.isEmpty()) {
        emit fatalError("Provider SDK received an invalid request envelope.");
        return;
    }
    const auto parameters = message.value("params").toObject();
    if (method == provider_protocol::cancel) {
        bool targetOk{};
        const auto target = parameters.value("requestId").toString().toULongLong(&targetOk);
        if (targetOk) {
            m_service.cancel(target);
            m_outstandingRequests.remove(target);
        }
        respond(requestId, QJsonObject{{"cancelled", targetOk}}, {});
        return;
    }
    if (method == provider_protocol::shutdown) {
        respond(requestId, QJsonObject{{"accepted", true}}, {});
        m_socket.flush();
        QCoreApplication::quit();
        return;
    }
    constexpr qsizetype maximumOutstandingRequests = 128;
    if (m_outstandingRequests.size() >= maximumOutstandingRequests) {
        respond(requestId, {}, "Provider has too many outstanding requests.");
        return;
    }
    m_outstandingRequests.insert(requestId);
    QPointer<ProviderApplication> guardedThis{this};
    m_service.invoke(method, parameters, requestId,
        [guardedThis, requestId](QJsonValue result, QString error) mutable {
            if (guardedThis == nullptr) {
                return;
            }
            QMetaObject::invokeMethod(guardedThis, [guardedThis, requestId,
                    result = std::move(result), error = std::move(error)]() mutable {
                if (guardedThis == nullptr
                    || !guardedThis->m_outstandingRequests.remove(requestId)) {
                    return;
                }
                guardedThis->respond(requestId, result, error);
            }, Qt::QueuedConnection);
        });
}

void ProviderApplication::respond(
    const quint64 requestId,
    const QJsonValue& result,
    const QString& error)
{
    QJsonObject message{{"type", "response"}, {"protocol", providerProtocolVersion},
        {"id", QString::number(requestId)}};
    if (error.isEmpty()) {
        message.insert("result", result);
    } else {
        message.insert("error", QJsonObject{{"code", provider_protocol::error_code::providerError},
            {"message", error}});
    }
    send(message);
}

bool ProviderApplication::send(const QJsonObject& message)
{
    QString error;
    const auto frame = ProviderProtocolCodec::encode(message, error);
    if (!error.isEmpty() || m_socket.write(frame) != frame.size()) {
        emit fatalError(error.isEmpty() ? "Provider SDK could not write IPC data." : error);
        return false;
    }
    return true;
}

QString ProviderApplication::argumentValue(
    const QStringList& arguments,
    const QString& name)
{
    const auto index = arguments.indexOf(name);
    return index >= 0 && index + 1 < arguments.size() ? arguments[index + 1] : QString{};
}

} // namespace yaap::sdk
