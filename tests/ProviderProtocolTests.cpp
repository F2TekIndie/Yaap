#include "extension_api/ProviderProtocol.hpp"
#include "provider_protocol/ProviderProtocolCodec.hpp"
#include "provider_runtime/ProviderProcessSupervisor.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonObject>
#include <QThread>
#include <QtEndian>

namespace yaap {
namespace {

template <typename Predicate>
bool waitUntil(Predicate predicate, const int timeoutMilliseconds = 5'000)
{
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < timeoutMilliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(1);
    }
    return predicate();
}

} // namespace

TEST_CASE("Provider protocol codec handles fragmented and combined frames")
{
    QString error;
    const auto first = ProviderProtocolCodec::encode({{"type", "one"}}, error);
    const auto second = ProviderProtocolCodec::encode({{"type", "two"}}, error);
    REQUIRE(error.isEmpty());

    ProviderProtocolCodec codec;
    auto result = codec.append(first.first(3));
    CHECK(result.messages.empty());
    result = codec.append(first.sliced(3) + second);
    REQUIRE(result.error.isEmpty());
    REQUIRE(result.messages.size() == 2);
    CHECK(result.messages[0].value("type") == "one");
    CHECK(result.messages[1].value("type") == "two");
}

TEST_CASE("Provider protocol rejects oversized frames before buffering payload")
{
    QByteArray header(4, '\0');
    qToBigEndian<quint32>(
        static_cast<quint32>(provider_protocol::maximumMessageBytes + 1), header.data());
    ProviderProtocolCodec codec;
    CHECK_FALSE(codec.append(header).error.isEmpty());
}

TEST_CASE("Sample provider completes handshake and describe request out of process")
{
    const QString executable{QString::fromUtf8(YAAP_SAMPLE_PROVIDER_PATH)};
    REQUIRE(QFileInfo::exists(executable));
    ProviderProcessSupervisor supervisor;
    QString error;
    REQUIRE(supervisor.start(executable, "org.yaap.sample-provider", {}, error));
    REQUIRE(waitUntil([&] { return supervisor.isReady() || !supervisor.errorMessage().isEmpty(); }));
    REQUIRE(supervisor.errorMessage().isEmpty());

    quint64 completedId{};
    QJsonValue response;
    QObject::connect(&supervisor, &ProviderProcessSupervisor::responseReceived,
        &supervisor, [&](const quint64 id, const QJsonValue& value, const QJsonObject&) {
            completedId = id;
            response = value;
        });
    const auto requestId = supervisor.sendRequest(provider_protocol::describe);
    REQUIRE(requestId != 0);
    REQUIRE(waitUntil([&] { return completedId == requestId; }));
    CHECK(response.toObject().value("id") == "org.yaap.sample-provider");
    supervisor.stop();
    CHECK(waitUntil([&] { return !supervisor.isReady(); }));
}

} // namespace yaap
