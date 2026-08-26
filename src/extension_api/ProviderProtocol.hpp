#pragma once

#include "extension_api/ApiVersion.hpp"

#include <QString>

namespace yaap::provider_protocol {

inline const QString providerHello = QStringLiteral("provider.hello");
inline const QString hostHello = QStringLiteral("host.hello");
inline const QString describe = QStringLiteral("provider.describe");
inline const QString testConnection = QStringLiteral("provider.testConnection");
inline const QString browse = QStringLiteral("provider.browse");
inline const QString search = QStringLiteral("provider.search");
inline const QString resolvePlayback = QStringLiteral("provider.resolvePlayback");
inline const QString fetchArtwork = QStringLiteral("provider.fetchArtwork");
inline const QString listPlaylists = QStringLiteral("provider.listPlaylists");
inline const QString cancel = QStringLiteral("provider.cancel");
inline const QString shutdown = QStringLiteral("provider.shutdown");
inline const QString hostCredentialRead = QStringLiteral("host.credential.read");

inline constexpr qsizetype maximumMessageBytes = 8 * 1024 * 1024;

namespace error_code {
inline const QString invalidRequest = QStringLiteral("invalid_request");
inline const QString incompatibleVersion = QStringLiteral("incompatible_version");
inline const QString permissionDenied = QStringLiteral("permission_denied");
inline const QString unavailable = QStringLiteral("unavailable");
inline const QString timeout = QStringLiteral("timeout");
inline const QString cancelled = QStringLiteral("cancelled");
inline const QString providerError = QStringLiteral("provider_error");
} // namespace error_code

} // namespace yaap::provider_protocol
