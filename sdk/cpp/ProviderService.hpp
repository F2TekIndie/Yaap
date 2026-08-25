#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>

#include <functional>

namespace yaap::sdk {

class ProviderService {
public:
    using Completion = std::function<void(QJsonValue result, QString error)>;
    virtual ~ProviderService() = default;

    [[nodiscard]] virtual QString providerId() const = 0;
    [[nodiscard]] virtual QString displayName() const = 0;
    [[nodiscard]] virtual QString version() const = 0;
    [[nodiscard]] virtual QStringList capabilities() const = 0;
    virtual void invoke(
        const QString& method,
        const QJsonObject& parameters,
        quint64 requestId,
        Completion completion) = 0;
    virtual void cancel(quint64 requestId) { Q_UNUSED(requestId); }
};

} // namespace yaap::sdk
