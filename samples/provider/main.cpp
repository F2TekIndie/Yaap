#include "extension_api/ProviderProtocol.hpp"
#include "sdk/cpp/ProviderApplication.hpp"
#include "sdk/cpp/ProviderService.hpp"

#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#include <cmath>

namespace {

[[nodiscard]] QUrl sampleToneUrl()
{
    constexpr quint32 sampleRate = 48'000;
    constexpr quint16 channels = 2;
    constexpr quint16 bitsPerSample = 16;
    constexpr quint32 frames = sampleRate * 3;
    constexpr quint32 dataBytes = frames * channels * (bitsPerSample / 8);
    const auto directory = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + "/YaapSampleProvider";
    QDir{}.mkpath(directory);
    const auto path = directory + "/sample-tone.wav";
    if (QFileInfo{path}.size() == static_cast<qint64>(44 + dataBytes)) {
        return QUrl::fromLocalFile(path);
    }
    QFile file{path};
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return {};
    }
    QDataStream stream{&file};
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.writeRawData("RIFF", 4);
    stream << quint32{36 + dataBytes};
    stream.writeRawData("WAVEfmt ", 8);
    stream << quint32{16} << quint16{1} << channels << sampleRate
           << quint32{sampleRate * channels * (bitsPerSample / 8)}
           << quint16{channels * (bitsPerSample / 8)} << bitsPerSample;
    stream.writeRawData("data", 4);
    stream << dataBytes;
    for (quint32 frame = 0; frame < frames; ++frame) {
        const auto phase = 2.0 * 3.14159265358979323846 * 440.0
            * static_cast<double>(frame) / sampleRate;
        const auto sample = static_cast<qint16>(std::sin(phase) * 5'000.0);
        stream << sample << sample;
    }
    return QUrl::fromLocalFile(path);
}

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
            completion(QJsonObject{{"url", sampleToneUrl().toString(QUrl::FullyEncoded)},
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
