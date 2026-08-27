# Extension platform delivery record and roadmap

Extension API 1.1 is implemented locally. This document records what the former
steps 8–15 delivered and separates completed engineering from release work that
requires external infrastructure or identities.

## Delivered milestones

| Step | Status | Delivered boundary |
|---|---|---|
| 8 | Implemented | Federated provider gateway, generation cancellation, DTO mapping, and sample-provider playback |
| 9 | Implemented | Multi-account model, platform credential storage, connection tests, and scoped one-shot handles |
| 10 | Implemented locally | Version/content-digest grant binding and declared publisher metadata |
| 11 | Implemented where locally enforceable | Request deadlines, bounded logs, crash-loop suppression, Windows Job restrictions, and Unix resource limits |
| 12 | Implemented | Host-owned provider search, account, local-library, and radio views |
| 13 | Implemented | Namespaced SQLite cache, TTL/stale state, offline reads, bounded LRU pruning, and account cleanup |
| 14 | Implemented locally | Standalone conformance runner, contract tests, parser fuzz target, sanitizer option, and Windows/Linux/macOS CI definitions |
| 15 | Implemented locally | Frozen schemas and error codes, compatibility policy, SDK template, install/CPack rules, SBOM, notices, and release gates |

The sample provider can search and resolve playable media through the same host
gateway used by OpenSubsonic and Jellyfin. Provider processes use bounded local
IPC and can be cancelled or terminated independently of the player UI.

## Remaining release gates

- Add cryptographic publisher signatures before offering a remote mod catalog
  or automatic package updates. The current publisher field is declarative.
- Sign Windows and macOS application artifacts and notarize the macOS bundle
  using real project identities.
- Run clean-machine installer tests on every supported OS and architecture.
- Maintain pinned OpenSubsonic and Jellyfin fixtures for live compatibility
  testing in addition to deterministic parser and protocol tests.
- Evaluate a stronger native sandbox or a WASM provider runtime before treating
  provider permissions as enforceable network or filesystem isolation.

Yaap intentionally contains no remote mod catalog or automatic updater until
publisher verification is available. QML extensions are trusted in-process
code, and native provider restrictions reduce risk without constituting a full
security boundary.

## Product roadmap

The next player-facing work is independent of the extension API release gates:

1. Gapless playback and playback-queue integration across every source type.
2. ReplayGain and loudness normalization with user-visible policy controls.
3. Crossfade and equalization in a dedicated mixer/DSP layer.
4. Output-device selection and explicit format negotiation.
5. Operating-system media-session controls and global shortcuts.
6. A complete playlist/station selection UI instead of automatically opening
   the first valid entry from the basic stream dialog.
7. Filesystem-journal-backed indexing for very large local libraries.
