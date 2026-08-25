#include "extension_api/ModPermission.hpp"

#include <QSet>
#include <QUrl>

namespace yaap::permission {

bool isKnown(const QString& value)
{
    static const QSet<QString> exact{themeInstall, playbackRead, playbackControl,
        queueRead, queueModify, libraryMetadataRead, libraryPlaylistsRead,
        libraryPlaylistsModify, providerAccountRead, modScopedStorage,
        notificationsShow};
    if (exact.contains(value)) {
        return true;
    }
    if (value.startsWith("ui.extend:")) {
        return isKnownUiSlot(value.sliced(10).trimmed());
    }
    if (value.startsWith("provider.network:")) {
        const auto host = value.sliced(17).trimmed();
        return !host.isEmpty() && !host.contains('/') && !host.contains('\\');
    }
    return false;
}

bool isKnownUiSlot(const QString& slotId)
{
    static const QSet<QString> knownSlots{
        "navigation.primary",
        "nowPlaying.aboveTransport",
        "nowPlaying.toolbar.after"};
    return knownSlots.contains(slotId);
}

bool permitsUiSlot(const QString& value, const QString& slotId)
{
    return value == uiSlot(slotId);
}

QString uiSlot(const QString& slotId)
{
    return "ui.extend:" + slotId;
}

QString providerNetworkHost(const QString& host)
{
    return "provider.network:" + host.toLower();
}

} // namespace yaap::permission
