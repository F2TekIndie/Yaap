# Player roadmap and release gates

## Current scope

Yaap supports local music, internet streams, radio discovery, theme packages,
custom appearance settings, and Linux shell integration through MPRIS and the
system tray. DMS appearance can be followed from Settings → Themes.

The fixed 480 × 112 miniplayer supports Waves, Paper planes, and Spectrum,
following the selected theme by default or using a saved per-theme override.
Hidden windows unload effects. Custom settings expose common appearance choices
and conditional Spectrum colors/opacity while preserving the underlying skin.
Linux Release output is isolated in `distribution/linux` from Windows builds.

Provider services, accounts, executable extensions, UI extensions, permission
grants, and their development kit have been removed. Theme parsing and validation
remain internal application components.

## Remaining release gates

- Sign Windows and macOS application artifacts and notarize the macOS bundle
  using real project identities.
- Run clean-machine installer tests on every supported OS and architecture.
- Maintain dependency notices, source obligations, and the SBOM for distributed
  binaries.
- Add publisher verification before offering a remote theme catalog or automatic
  package updates. The current publisher metadata is declarative.

## Product roadmap

1. Gapless playback and playback-queue integration.
2. ReplayGain and loudness normalization with user-visible policy controls.
3. Crossfade and equalization in a dedicated mixer/DSP layer.
4. Output-device selection and explicit format negotiation.
5. Media-session integration on Windows and macOS.
6. A complete playlist/station selection UI instead of automatically opening
   the first valid entry from the basic stream dialog.
7. Filesystem-journal-backed indexing for very large local libraries.
