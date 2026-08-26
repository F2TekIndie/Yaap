#include "provider_runtime/ProviderProcessSupervisor.hpp"

#include "extension_api/ProviderProtocol.hpp"

#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QJsonArray>
#include <QLocalSocket>
#include <QProcessEnvironment>
#include <QUuid>

#include <utility>

#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/resource.h>
#endif

namespace yaap {
namespace {
constexpr int requestTimeoutMilliseconds = 20'000;
constexpr qsizetype maximumDiagnosticBytes = 64 * 1024;

void appendDiagnostic(QByteArray& destination, QByteArray data)
{
    if (data.size() > maximumDiagnosticBytes) {
        data = data.last(maximumDiagnosticBytes);
    }
    destination += data;
    if (destination.size() > maximumDiagnosticBytes) {
        destination = destination.last(maximumDiagnosticBytes);
    }
}
} // namespace

ProviderProcessSupervisor::ProviderProcessSupervisor(QObject* parent)
    : QObject(parent)
{
    m_startupTimer.setSingleShot(true);
    m_startupTimer.setInterval(10'000);
    connect(&m_startupTimer, &QTimer::timeout, this,
        [this] { fail("Provider process handshake timed out."); stop(); });
    connect(&m_server, &QLocalServer::newConnection,
        this, &ProviderProcessSupervisor::acceptConnection);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (!m_stopping) {
            fail("Provider process error: " + m_process.errorString());
            stop();
        }
    });
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
        this, [this](int, QProcess::ExitStatus) {
            const auto wasReady = m_ready;
            m_ready = false;
            cleanupSocket();
            m_server.close();
            if (!m_serverName.isEmpty()) {
                QLocalServer::removeServer(m_serverName);
            }
            m_outstandingRequests.clear();
#ifdef _WIN32
            if (m_nativeContainmentHandle != nullptr) {
                CloseHandle(static_cast<HANDLE>(m_nativeContainmentHandle));
                m_nativeContainmentHandle = nullptr;
            }
#endif
            if (wasReady) {
                emit readyChanged();
            }
            emit processStopped();
        });
    connect(&m_process, &QProcess::readyReadStandardOutput,
        &m_process, [this] {
            appendDiagnostic(m_diagnosticTail, m_process.readAllStandardOutput());
        });
    connect(&m_process, &QProcess::readyReadStandardError,
        &m_process, [this] {
            appendDiagnostic(m_diagnosticTail, m_process.readAllStandardError());
        });
#ifdef _WIN32
    connect(&m_process, &QProcess::started, this, [this] {
        auto* job = CreateJobObjectW(nullptr, nullptr);
        if (job == nullptr) {
            appendDiagnostic(m_diagnosticTail, "Host could not create a provider job object.\n");
            return;
        }
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
            | JOB_OBJECT_LIMIT_PROCESS_MEMORY;
        limits.ProcessMemoryLimit = 512ULL * 1024ULL * 1024ULL;
        JOBOBJECT_BASIC_UI_RESTRICTIONS ui{};
        ui.UIRestrictionsClass = JOB_OBJECT_UILIMIT_EXITWINDOWS
            | JOB_OBJECT_UILIMIT_HANDLES | JOB_OBJECT_UILIMIT_READCLIPBOARD
            | JOB_OBJECT_UILIMIT_WRITECLIPBOARD | JOB_OBJECT_UILIMIT_SYSTEMPARAMETERS;
        auto* process = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE,
            FALSE, static_cast<DWORD>(m_process.processId()));
        const auto applied = SetInformationJobObject(job, JobObjectExtendedLimitInformation,
                &limits, sizeof(limits))
            && SetInformationJobObject(job, JobObjectBasicUIRestrictions, &ui, sizeof(ui))
            && process != nullptr && AssignProcessToJobObject(job, process);
        if (process != nullptr) {
            CloseHandle(process);
        }
        if (!applied) {
            appendDiagnostic(m_diagnosticTail,
                "Host could not apply all provider job restrictions.\n");
            CloseHandle(job);
            return;
        }
        m_nativeContainmentHandle = job;
    });
#endif
}

ProviderProcessSupervisor::~ProviderProcessSupervisor()
{
    m_stopping = true;
    m_startupTimer.stop();
    cleanupSocket();
    m_server.close();
    if (!m_serverName.isEmpty()) {
        QLocalServer::removeServer(m_serverName);
    }
    if (m_process.state() != QProcess::NotRunning) {
        m_process.terminate();
        if (!m_process.waitForFinished(500)) {
            m_process.kill();
            m_process.waitForFinished(500);
        }
    }
#ifdef _WIN32
    if (m_nativeContainmentHandle != nullptr) {
        CloseHandle(static_cast<HANDLE>(m_nativeContainmentHandle));
        m_nativeContainmentHandle = nullptr;
    }
#endif
}

bool ProviderProcessSupervisor::start(
    const QString& executable,
    const QString& expectedProviderId,
    const QStringList& grantedPermissions,
    QString& error)
{
    if (m_process.state() != QProcess::NotRunning || m_server.isListening()) {
        error = "Provider process is already running.";
        fail(error);
        return false;
    }
    const QFileInfo executableInfo{executable};
    if (!executableInfo.isFile() || expectedProviderId.trimmed().isEmpty()) {
        error = "Provider executable and expected provider ID are required.";
        fail(error);
        return false;
    }

    m_stopping = false;
    m_ready = false;
    m_errorMessage.clear();
    m_diagnosticTail.clear();
    m_expectedProviderId = expectedProviderId;
    m_grantedPermissions = grantedPermissions;
    m_nonce = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_serverName = "yaap-provider-" + QString::number(QCoreApplication::applicationPid())
        + '-' + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QLocalServer::removeServer(m_serverName);
    if (!m_server.listen(m_serverName)) {
        error = "Could not create provider IPC server: " + m_server.errorString();
        fail(error);
        return false;
    }

    m_process.setProgram(executableInfo.absoluteFilePath());
    m_process.setWorkingDirectory(executableInfo.absolutePath());
    m_process.setArguments({"--yaap-socket", m_serverName, "--yaap-nonce", m_nonce,
        "--yaap-api", QString::number(providerProtocolVersion)});
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    auto environment = QProcessEnvironment::systemEnvironment();
    const auto applicationPath = QCoreApplication::applicationDirPath();
    environment.insert("PATH", applicationPath + QDir::listSeparator()
        + environment.value("PATH"));
#ifndef _WIN32
    environment.insert("LD_LIBRARY_PATH", applicationPath + QDir::listSeparator()
        + environment.value("LD_LIBRARY_PATH"));
#endif
    m_process.setProcessEnvironment(environment);
#ifndef _WIN32
    m_process.setChildProcessModifier([] {
        const rlimit noCore{0, 0};
        setrlimit(RLIMIT_CORE, &noCore);
        const rlimit fileLimit{256, 256};
        setrlimit(RLIMIT_NOFILE, &fileLimit);
#ifdef RLIMIT_AS
        const rlimit addressSpace{1024ULL * 1024ULL * 1024ULL,
            1024ULL * 1024ULL * 1024ULL};
        setrlimit(RLIMIT_AS, &addressSpace);
#endif
    });
#endif
    m_process.start();
    m_startupTimer.start();
    return true;
}

quint64 ProviderProcessSupervisor::sendRequest(
    const QString& method,
    const QJsonObject& parameters)
{
    constexpr qsizetype maximumOutstandingRequests = 128;
    if (!m_ready || method.trimmed().isEmpty()
        || m_outstandingRequests.size() >= maximumOutstandingRequests) {
        return 0;
    }
    const auto requestId = m_nextRequestId++;
    if (!sendMessage({{"type", "request"}, {"protocol", providerProtocolVersion},
            {"id", QString::number(requestId)}, {"method", method}, {"params", parameters}})) {
        return 0;
    }
    m_outstandingRequests.insert(requestId);
    QTimer::singleShot(requestTimeoutMilliseconds, this, [this, requestId] {
        if (!m_outstandingRequests.remove(requestId)) {
            return;
        }
        emit responseReceived(requestId, {},
            QJsonObject{{"code", provider_protocol::error_code::timeout},
                {"message", "Provider request timed out."}});
        fail("Provider became unresponsive and was stopped.");
        stop();
    });
    return requestId;
}

void ProviderProcessSupervisor::cancel(const quint64 requestId)
{
    if (m_ready && requestId != 0 && m_outstandingRequests.remove(requestId)) {
        sendRequest(provider_protocol::cancel,
            QJsonObject{{"requestId", QString::number(requestId)}});
    }
}

bool ProviderProcessSupervisor::sendHostResponse(
    const quint64 requestId,
    const QJsonValue& result,
    const QJsonObject& error)
{
    QJsonObject message{{"type", "response"}, {"protocol", providerProtocolVersion},
        {"id", QString::number(requestId)}};
    if (error.isEmpty()) {
        message.insert("result", result);
    } else {
        message.insert("error", error);
    }
    return sendMessage(message);
}

void ProviderProcessSupervisor::stop()
{
    m_stopping = true;
    m_startupTimer.stop();
    if (m_process.state() == QProcess::NotRunning) {
        QTimer::singleShot(0, this, &ProviderProcessSupervisor::processStopped);
        return;
    }
    if (m_ready) {
        sendRequest(provider_protocol::shutdown);
    }
    QTimer::singleShot(500, this, [this] {
        if (m_process.state() != QProcess::NotRunning) {
            m_process.terminate();
        }
    });
    QTimer::singleShot(1'500, this, [this] {
        if (m_process.state() != QProcess::NotRunning) {
            m_process.kill();
        }
    });
}

bool ProviderProcessSupervisor::isReady() const noexcept { return m_ready; }
QString ProviderProcessSupervisor::providerId() const { return m_providerId; }
QString ProviderProcessSupervisor::errorMessage() const { return m_errorMessage; }
QString ProviderProcessSupervisor::diagnosticTail() const
{
    return QString::fromUtf8(m_diagnosticTail);
}

void ProviderProcessSupervisor::acceptConnection()
{
    auto* candidate = m_server.nextPendingConnection();
    if (candidate == nullptr) {
        return;
    }
    if (m_socket != nullptr) {
        candidate->disconnectFromServer();
        candidate->deleteLater();
        return;
    }
    candidate->setParent(this);
    m_socket = candidate;
    connect(candidate, &QLocalSocket::readyRead, this, &ProviderProcessSupervisor::readMessages);
    connect(candidate, &QLocalSocket::disconnected, this, [this] {
        if (!m_stopping) {
            fail("Provider IPC connection closed unexpectedly.");
            stop();
        }
        cleanupSocket();
    });
}

void ProviderProcessSupervisor::readMessages()
{
    if (m_socket == nullptr) {
        return;
    }
    auto decoded = m_codec.append(m_socket->readAll());
    if (!decoded.error.isEmpty()) {
        fail(std::move(decoded.error));
        stop();
        return;
    }
    for (const auto& message : decoded.messages) {
        handleMessage(message);
    }
}

void ProviderProcessSupervisor::handleMessage(const QJsonObject& message)
{
    if (message.value("protocol").toInt(-1) != providerProtocolVersion) {
        fail("Provider sent an incompatible protocol version.");
        stop();
        return;
    }
    const auto type = message.value("type").toString();
    if (!m_ready) {
        if (type != provider_protocol::providerHello
            || message.value("nonce").toString() != m_nonce
            || message.value("providerId").toString() != m_expectedProviderId) {
            fail("Provider handshake validation failed.");
            stop();
            return;
        }
        m_providerId = m_expectedProviderId;
        m_ready = true;
        m_startupTimer.stop();
        sendMessage({{"type", provider_protocol::hostHello},
            {"protocol", providerProtocolVersion},
            {"extensionApi", extensionApiVersion.toString()},
            {"permissions", QJsonArray::fromStringList(m_grantedPermissions)}});
        emit readyChanged();
        return;
    }
    if (type == "request") {
        bool idOk{};
        const auto requestId = message.value("id").toString().toULongLong(&idOk);
        const auto method = message.value("method").toString();
        if (!idOk || requestId == 0 || method.isEmpty()) {
            fail("Provider sent an invalid host request.");
            return;
        }
        emit hostRequestReceived(requestId, method, message.value("params").toObject());
        return;
    }
    if (type != "response") {
        fail("Provider sent an unexpected message type.");
        return;
    }
    bool idOk{};
    const auto requestId = message.value("id").toString().toULongLong(&idOk);
    if (!idOk || requestId == 0) {
        fail("Provider response has an invalid request ID.");
        return;
    }
    if (!m_outstandingRequests.remove(requestId)) {
        fail("Provider response refers to an unknown or completed request.");
        return;
    }
    emit responseReceived(requestId, message.value("result"), message.value("error").toObject());
}

bool ProviderProcessSupervisor::sendMessage(const QJsonObject& message)
{
    if (m_socket == nullptr || m_socket->state() != QLocalSocket::ConnectedState) {
        return false;
    }
    QString error;
    const auto frame = ProviderProtocolCodec::encode(message, error);
    if (!error.isEmpty() || m_socket->write(frame) != frame.size()) {
        fail(error.isEmpty() ? "Could not write provider IPC message." : error);
        return false;
    }
    return true;
}

void ProviderProcessSupervisor::fail(QString error)
{
    m_errorMessage = std::move(error);
    emit errorMessageChanged();
}

void ProviderProcessSupervisor::cleanupSocket()
{
    if (m_socket != nullptr) {
        auto* socket = m_socket.data();
        m_socket = nullptr;
        socket->abort();
        socket->deleteLater();
    }
    m_codec.reset();
}

} // namespace yaap
