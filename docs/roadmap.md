# Roadmap after extension API 1.0

Steps 1–7 established the versioned contracts, validated packages, data-only
themes, declared QML slots, persisted permission grants, bounded provider IPC,
and the asynchronous C++ provider SDK. The remaining work is reordered below
using what the implementation exposed about the actual trust and lifecycle
boundaries.

## 8. Connect providers to the application domain

Add a host-side provider gateway that maps protocol DTOs into `Track`,
`Playlist`, and provider/account models. Route browse, search, resolve, artwork,
and playlist requests through it with cancellation and generation IDs so stale
responses cannot update the UI. Keep existing built-in OpenSubsonic and
Jellyfin clients behind the same interface before moving them out of process.

**Exit:** the sample provider can browse, search, and resolve a playable track
through the normal application UI and playback session.

## 9. Add account configuration and credential handles

Create provider/account settings, connection tests, and per-account capability
negotiation. Secrets stay in the platform credential store; provider messages
receive short-lived opaque credential handles or scoped results, never values
from another provider or account.

**Exit:** multiple accounts can be added, tested, disabled, and removed without
placing a secret in JSON, SQLite, logs, command lines, or mod settings.

## 10. Bind trust grants to package identity

Hash installed package contents, persist the accepted digest and version with
grants, and require review when executable or QML content changes. Add signed
package metadata and publisher identity before any remote catalog or automatic
update feature. Treat QML extensions and native providers as trusted code until
an enforceable OS/WASM sandbox exists.

**Exit:** changing a granted package invalidates its active grant, and signed
updates have an auditable publisher and content identity.

## 11. Harden provider process containment

Add OS-specific launch policies, resource ceilings, log quotas, crash-loop
suppression, health checks, and explicit network-origin enforcement where the
platform permits it. Preserve graceful cancellation and shutdown, but recover
the host independently from provider hangs or crashes.

**Exit:** malformed, noisy, repeatedly crashing, or unresponsive providers are
bounded and diagnosable without destabilizing Yaap.

## 12. Build server, radio, and federated-search UI

Implement provider/account navigation, station collections, paged server
browsing, unified search, result provenance, loading/empty/error states, and
keyboard/accessibility behavior. Keep provider UI host-owned; providers return
data and declared actions rather than arbitrary account screens.

**Exit:** local library, radio, OpenSubsonic, Jellyfin, and sample-provider
content can be navigated consistently without entering URLs manually.

## 13. Add cache and offline policy

Introduce schema-versioned metadata/artwork caches with provider namespaces,
ETags or revision tokens, size limits, eviction, invalidation, and explicit
offline behavior. Do not cache credentials or unconstrained provider payloads.

**Exit:** startup and repeat browsing work from bounded caches, stale data is
visible as stale, and account removal deletes its scoped cache.

## 14. Publish compatibility and conformance tooling

Turn protocol and manifest fixtures into a provider conformance runner. Add
contract tests for cancellation races, late responses, paging, malformed JSON,
oversized frames, slow handshakes, process death, API-range negotiation, and
real OpenSubsonic/Jellyfin version matrices. Add parser fuzzing and sanitizers.

**Exit:** third-party providers can validate a package independently, and CI
covers Windows, Linux, and macOS against supported Qt/FFmpeg combinations.

## 15. Stabilize and release Extension API 1.x

Generate SDK reference documentation and package templates, define deprecation
and compatibility rules, freeze protocol error codes and DTO schemas, bundle
license/SBOM material, and perform signed clean-machine installer tests. Defer
automatic mod distribution until steps 10, 11, and 14 are complete.

**Exit:** API 1.x has reproducible artifacts, compatibility policy, examples,
and release gates suitable for third-party development.

