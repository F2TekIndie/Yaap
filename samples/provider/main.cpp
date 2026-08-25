#include "extension_api/ProviderProtocol.hpp"
#include "sdk/cpp/ProviderApplication.hpp"
#include "sdk/cpp/ProviderService.hpp"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>

namespace {

class SampleProvider final : public yaap::sdk::ProviderService {
public:
    [[nodiscard]] QString providerId() const override { return "org.yaap.sample-provider"; }
    [[nodiscard]] QString displayName() const override { return "Yaap Sample Provider"; }
    [[nodiscard]] QString version() const override { return "1.0.0"; }
    [[nodiscard]] QStringList capabilities() const override
    {
        return {"browse", "search", "stream"};
    }

    void invoke(
        const QString& method,
        const QJsonObject& parameters,
        quint64,
        Completion completion) override
    {
        if (method == yaap::provider_protocol::describe) {
            completion(QJsonObject{{"id", providerId()}, {"name", displayName()},
                {"version", version()},
                {"capabilities", QJsonArray::fromStringList(capabilities())}}, {});
            return;
        }
        if (method == yaap::provider_protocol::browse
            || method == yaap::provider_protocol::search) {
            const auto query = parameters.value("query").toString();
            completion(QJsonObject{{"items", QJsonArray{
                QJsonObject{{"id", "sample:track:1"}, {"type", "track"},
                    {"title", query.isEmpty() ? "Sample Track" : "Result for " + query},
                    {"artist", "Yaap SDK"}, {"durationMilliseconds", 30'000}}}},
                {"nextCursor", QJsonValue::Null}}, {});
            return;
        }
        if (method == yaap::provider_protocol::resolvePlayback) {
            completion(QJsonObject{{"url", "https://example.invalid/sample.mp3"},
                {"expiresAt", QJsonValue::Null}}, {});
            return;
        }
        if (method == yaap::provider_protocol::testConnection) {
            completion(QJsonObject{{"ok", true}, {"server", "sample"}}, {});
            return;
        }
        completion({}, "Sample provider does not implement method " + method);
    }
};

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application{argc, argv};
    SampleProvider service;
    yaap::sdk::ProviderApplication provider{service};
    QObject::connect(&provider, &yaap::sdk::ProviderApplication::fatalError,
        &application, [](const QString&) { QCoreApplication::exit(EXIT_FAILURE); });
    QString error;
    if (!provider.start(application.arguments(), error)) {
        return EXIT_FAILURE;
    }
    return application.exec();
}
