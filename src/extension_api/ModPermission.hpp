#pragma once

#include <QString>
#include <QStringList>

namespace yaap {

namespace permission {
inline const QString themeInstall = QStringLiteral("theme.install");
inline const QString playbackRead = QStringLiteral("playback.read");
inline const QString playbackControl = QStringLiteral("playback.control");
inline const QString queueRead = QStringLiteral("queue.read");
inline const QString queueModify = QStringLiteral("queue.modify");
inline const QString libraryMetadataRead = QStringLiteral("library.metadata.read");
inline const QString libraryPlaylistsRead = QStringLiteral("library.playlists.read");
inline const QString libraryPlaylistsModify = QStringLiteral("library.playlists.modify");
inline const QString providerAccountRead = QStringLiteral("provider.account.read");
inline const QString modScopedStorage = QStringLiteral("storage.mod-scoped");
inline const QString notificationsShow = QStringLiteral("notifications.show");

[[nodiscard]] bool isKnown(const QString& value);
[[nodiscard]] bool isKnownUiSlot(const QString& slotId);
[[nodiscard]] bool permitsUiSlot(const QString& value, const QString& slotId);
[[nodiscard]] QString uiSlot(const QString& slotId);
[[nodiscard]] QString providerNetworkHost(const QString& host);
} // namespace permission

} // namespace yaap
