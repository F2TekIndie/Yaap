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

} // namespace yaap
