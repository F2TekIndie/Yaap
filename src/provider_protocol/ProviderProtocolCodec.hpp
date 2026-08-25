#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

#include <vector>

namespace yaap {

struct ProtocolDecodeResult final {
    std::vector<QJsonObject> messages;
    QString error;
};

class ProviderProtocolCodec final {
public:
    [[nodiscard]] static QByteArray encode(const QJsonObject& message, QString& error);
    [[nodiscard]] ProtocolDecodeResult append(const QByteArray& bytes);
    void reset();

private:
    QByteArray m_buffer;
};

} // namespace yaap
