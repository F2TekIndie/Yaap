#include "extension_api/ProviderProtocol.hpp"
#include "mods/ModManifestParser.hpp"
#include "provider_runtime/ProviderProcessSupervisor.hpp"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QTimer>
#include <QUrl>

namespace {

class ConformanceRun final : public QObject {
    Q_OBJECT

public:
    ConformanceRun(QString executable, QString providerId, QObject* parent = nullptr)
        : QObject(parent)
        , m_executable(std::move(executable))
        , m_providerId(std::move(providerId))
    {
        connect(&m_provider, &yaap::ProviderProcessSupervisor::readyChanged,
            this, [this] {
                if (m_provider.isReady()) {
                    m_stage = Stage::Describe;
                    m_requestId = m_provider.sendRequest(yaap::provider_protocol::describe);
                }
            });
        connect(&m_provider, &yaap::ProviderProcessSupervisor::responseReceived,
            this, &ConformanceRun::response);
        connect(&m_provider, &yaap::ProviderProcessSupervisor::errorMessageChanged,
            this, [this] {
                if (!m_provider.errorMessage().isEmpty()) {
                    fail(m_provider.errorMessage());
                }
            });
    }

    void start()
    {
        QString error;
        if (!m_provider.start(m_executable, m_providerId, {}, error)) {
            fail(error);
            return;
        }
        QTimer::singleShot(15'000, this, [this] { fail("Conformance run timed out."); });
    }

private:
    enum class Stage { Starting, Describe, Search, Resolve, Complete };

    void response(
        const quint64 requestId,
        const QJsonValue& result,
        const QJsonObject& error)
    {
        if (requestId != m_requestId || !error.isEmpty()) {
            fail(error.value("message").toString("Unexpected provider response."));
            return;
        }
        if (m_stage == Stage::Describe) {
            const auto object = result.toObject();
            if (object.value("id").toString() != m_providerId
                || !object.value("capabilities").isArray()) {
                fail("provider.describe returned an invalid descriptor.");
                return;
            }
            m_stage = Stage::Search;
            m_requestId = m_provider.sendRequest(yaap::provider_protocol::search,
                QJsonObject{{"query", "conformance"}, {"limit", 1}});
            return;
        }
        if (m_stage == Stage::Search) {
            const auto items = result.toObject().value("items").toArray();
            if (items.isEmpty() || items.first().toObject().value("id").toString().isEmpty()) {
                fail("provider.search returned no valid track fixture.");
                return;
            }
            m_stage = Stage::Resolve;
            m_requestId = m_provider.sendRequest(yaap::provider_protocol::resolvePlayback,
                QJsonObject{{"trackId", items.first().toObject().value("id").toString()}});
            return;
        }
        if (m_stage == Stage::Resolve) {
            const QUrl url{result.toObject().value("url").toString()};
            if (!url.isValid() || (url.scheme() != "file" && url.scheme() != "http"
                    && url.scheme() != "https")) {
                fail("provider.resolvePlayback returned an unsupported URL.");
                return;
            }
            m_stage = Stage::Complete;
            QTextStream{stdout} << QJsonDocument{QJsonObject{{"ok", true},
                {"providerId", m_providerId}, {"protocol", yaap::providerProtocolVersion}}}
                .toJson(QJsonDocument::Compact) << Qt::endl;
            m_provider.stop();
            QCoreApplication::exit(EXIT_SUCCESS);
        }
    }

    void fail(const QString& message)
    {
        if (m_stage == Stage::Complete) {
            return;
        }
        m_stage = Stage::Complete;
        QTextStream{stderr} << QJsonDocument{QJsonObject{{"ok", false}, {"error", message}}}
            .toJson(QJsonDocument::Compact) << Qt::endl;
        m_provider.stop();
        QCoreApplication::exit(EXIT_FAILURE);
    }

    QString m_executable;
    QString m_providerId;
    yaap::ProviderProcessSupervisor m_provider;
    quint64 m_requestId{};
    Stage m_stage{Stage::Starting};
};

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application{argc, argv};
    QCoreApplication::setApplicationName("yaap-provider-conformance");
    QCommandLineParser parser;
    parser.setApplicationDescription("Validate a Yaap mod package or provider protocol executable.");
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption packageOption{{"p", "package"}, "Validate a mod package.", "path"};
    QCommandLineOption executableOption{{"e", "executable"}, "Provider executable.", "path"};
    QCommandLineOption idOption{{"i", "provider-id"}, "Expected provider ID.", "id"};
    parser.addOptions({packageOption, executableOption, idOption});
    parser.process(application);

    if (parser.isSet(packageOption)) {
        const auto result = yaap::ModManifestParser::parsePackage(parser.value(packageOption));
        const auto output = result.succeeded()
            ? QJsonObject{{"ok", true}, {"id", result.manifest.id},
                {"version", result.manifest.version}, {"digest", result.manifest.contentDigest}}
            : QJsonObject{{"ok", false}, {"error", result.error}};
        QTextStream{result.succeeded() ? stdout : stderr}
            << QJsonDocument{output}.toJson(QJsonDocument::Compact) << Qt::endl;
        return result.succeeded() ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (!parser.isSet(executableOption) || !parser.isSet(idOption)) {
        parser.showHelp(EXIT_FAILURE);
    }
    ConformanceRun run{parser.value(executableOption), parser.value(idOption)};
    QTimer::singleShot(0, &run, [&run] { run.start(); });
    return application.exec();
}

#include "main.moc"
