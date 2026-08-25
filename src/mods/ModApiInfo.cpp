#include "mods/ModApiInfo.hpp"

#include "extension_api/ApiVersion.hpp"

namespace yaap {

ModApiInfo::ModApiInfo(QObject* parent)
    : QObject(parent)
{
}

QString ModApiInfo::version() const
{
    return extensionApiVersion.toString();
}

QString ModApiInfo::trustWarning() const
{
    return "QML extensions execute inside Yaap. Enable only extensions you trust.";
}

} // namespace yaap
