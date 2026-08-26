# Yaap provider protocol 1

Provider processes communicate over a Qt local socket. The wire format is
language-neutral: a four-byte unsigned big-endian payload length followed by a
UTF-8 JSON object. Payloads are limited to 8 MiB and each side allows no more
than 128 outstanding requests.

## Launch and handshake

Yaap launches the executable with:

```text
--yaap-socket <name> --yaap-nonce <nonce> --yaap-api 1
```

The provider connects and sends `provider.hello` containing the same nonce,
protocol number, provider ID, version, and capability list. Yaap validates it
and replies with `host.hello`, the extension API version, and granted
permissions. Requests are rejected until the handshake completes.

## Envelopes

Request IDs are decimal strings so they remain exact in every JSON runtime.

```json
{"type":"request","protocol":1,"id":"42","method":"provider.search","params":{}}
```

```json
{"type":"response","protocol":1,"id":"42","result":{}}
```

```json
{"type":"response","protocol":1,"id":"42","error":{"code":"provider_error","message":"..."}}
```

Protocol 1 methods are `provider.describe`, `provider.testConnection`,
`provider.browse`, `provider.search`, `provider.resolvePlayback`,
`provider.fetchArtwork`, `provider.listPlaylists`, `provider.cancel`, and
`provider.shutdown`. Unknown methods must return a typed error. Unknown optional
object fields must be ignored.

Providers with `provider.account.read` may receive a short-lived, provider- and
account-scoped credential handle in a host request. They redeem it once through
`host.credential.read`; the host rejects expired, reused, cross-account, or
cross-provider handles. The returned `secretBase64` exists only in local IPC
memory and must be decoded into short-lived mutable storage and cleared after
use. Handles and secrets must never be logged or cached.

The host applies a 20-second request deadline, bounds diagnostic output to a
64-KiB tail, and suppresses crash loops after three failures in 60 seconds.

## C++ SDK

Link `Yaap::ProviderSdk` from the installed `YaapProviderSdk` CMake
package, implement `yaap::sdk::ProviderService`, and create
`yaap::sdk::ProviderApplication` after `QCoreApplication`. The completion-based
service interface supports asynchronous network operations; late completions
after cancellation are discarded by the SDK.

The sample can be compiled as an independent SDK consumer after installing
Yaap to a staging prefix:

```powershell
cmake -S samples/provider -B build/provider-sample `
  -DCMAKE_PREFIX_PATH="<yaap-prefix>;<qt-prefix>"
cmake --build build/provider-sample --config Release
```

The SDK owns socket framing and handshake validation. Provider implementations
own remote-server timeouts, response validation, and cancellation of their own
work.
