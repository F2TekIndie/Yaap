#include "extension_api/ApiVersion.hpp"

#include <QRegularExpression>

namespace yaap {

QString ApiVersion::toString() const
{
    return QString::number(major) + '.' + QString::number(minor);
}

std::optional<ApiVersion> ApiVersion::parse(const QString& value)
{
    static const QRegularExpression pattern{R"(^(0|[1-9]\d*)\.(0|[1-9]\d*)$)"};
    const auto match = pattern.match(value.trimmed());
    if (!match.hasMatch()) {
        return std::nullopt;
    }
    bool majorOk{};
    bool minorOk{};
    const auto major = match.captured(1).toInt(&majorOk);
    const auto minor = match.captured(2).toInt(&minorOk);
    if (!majorOk || !minorOk) {
        return std::nullopt;
    }
    return ApiVersion{major, minor};
}

} // namespace yaap
