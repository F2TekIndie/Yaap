#include "provider_protocol/ProviderProtocolCodec.hpp"

#include "extension_api/ProviderProtocol.hpp"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QtEndian>

#include <cstring>

namespace yaap {

QByteArray ProviderProtocolCodec::encode(const QJsonObject& message, QString& error)
{
    const auto payload = QJsonDocument{message}.toJson(QJsonDocument::Compact);
    if (payload.isEmpty() || payload.size() > provider_protocol::maximumMessageBytes) {
        error = "Provider protocol message exceeds the 8 MiB limit.";
        return {};
    }
    QByteArray frame;
    frame.resize(static_cast<qsizetype>(sizeof(quint32)) + payload.size());
    qToBigEndian<quint32>(static_cast<quint32>(payload.size()), frame.data());
    std::memcpy(frame.data() + sizeof(quint32), payload.constData(),
        static_cast<std::size_t>(payload.size()));
    return frame;
}

ProtocolDecodeResult ProviderProtocolCodec::append(const QByteArray& bytes)
{
    m_buffer.append(bytes);
    ProtocolDecodeResult result;
    while (m_buffer.size() >= static_cast<qsizetype>(sizeof(quint32))) {
        const auto payloadSize = qFromBigEndian<quint32>(m_buffer.constData());
        if (payloadSize == 0 || payloadSize > provider_protocol::maximumMessageBytes) {
            reset();
            result.error = "Invalid provider protocol frame length.";
            return result;
        }
        const auto frameSize = static_cast<qsizetype>(sizeof(quint32))
            + static_cast<qsizetype>(payloadSize);
        if (m_buffer.size() < frameSize) {
            break;
        }
        const auto payload = m_buffer.sliced(
            static_cast<qsizetype>(sizeof(quint32)), static_cast<qsizetype>(payloadSize));
        m_buffer.remove(0, frameSize);
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            reset();
            result.error = "Invalid provider protocol JSON: " + parseError.errorString();
            return result;
        }
        result.messages.push_back(document.object());
    }
    return result;
}

void ProviderProtocolCodec::reset()
{
    m_buffer.clear();
}

} // namespace yaap
