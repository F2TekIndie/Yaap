# Yaap prototype

Yaap is a C++20/Qt Quick music-player prototype. It plays local files and HTTP
streams through FFmpeg, a bounded SPSC PCM ring, and miniaudio. It also contains
the first complete service layer for a watched SQLite music library, station
playlists, OpenSubsonic, Jellyfin, and operating-system credential storage.

## Implemented architecture

- Continuous FFmpeg producer with interruptible I/O, 15-second deadlines,
  seeking, prebuffering, cancellation, and network reconnect options.
- Real-time-safe miniaudio consumer backed by a bounded 48 kHz stereo float
  SPSC ring. Decoder shutdown is retired to a reaper thread, never joined by the
  GUI thread.
- Explicit track, queue, provider, playlist, and playback-session domain models.
- SQLite library synchronization, FFmpeg metadata and embedded-artwork
  extraction, recursive folder scans, filesystem watching, and playlists.
- M3U/M3U8, PLS, and XSPF parsing; incremental ICY metadata demuxing; bounded
  HTTP playlist loading; and capped exponential reconnect behavior.
- Radio Browser discovery with SRV mirror failover, country-popular and
  name-search views, click reporting, bounded responses, and 24-hour SQLite
  cache fallback. Yaap bundles no default stations; saving is always explicit.
- Asynchronous OpenSubsonic search/stream URL generation and Jellyfin
  authentication/library loading through Qt Network.
- Windows Credential Manager, macOS Keychain, and Linux Secret Service
  credential backends. Passwords are never written to Yaap's database.
- Extension API 1.0 with validated manifests, data-only theme packs, declared
  QML extension slots, explicit permission grants, and sample mod packages.
- Length-framed, bounded local IPC plus an asynchronous C++ provider SDK and
  supervised out-of-process sample provider.
- Federated provider search and playback resolution across external providers,
  OpenSubsonic, and Jellyfin; secure multi-account settings; station and local
  library browsers; bounded offline SQLite caches.
- Digest-bound mod grants, provider request deadlines, bounded logs,
  crash-loop suppression, Windows Job/Unix resource limits, and a standalone
  provider/package conformance runner.

## Dependencies

- CMake 4.2 or newer
- Visual Studio 2026 or 2022 on Windows
- Qt 6.5 or newer with Core, Concurrent, Network, SQL/SQLite, Qt Quick, Quick
  Controls, and Quick Dialogs
- FFmpeg development libraries: `avformat`, `avcodec`, `avutil`, `swresample`
- miniaudio
- Catch2 3
- A global vcpkg installation

FFmpeg, miniaudio, and Catch2 are installed once in the global vcpkg classic
tree. Yaap explicitly disables manifest mode so it does not create duplicate
`vcpkg_installed` directories inside the project. Qt also comes from the global
official prebuilt SDK.

For the current machine, the intended roots are:

```powershell
$env:VCPKG_ROOT = 'G:\CodingLibraries\vcpkg'
$env:CMAKE_PREFIX_PATH = 'G:\CodingLibraries\Qt\6.11.1\msvc2022_64'
& "$env:VCPKG_ROOT\vcpkg.exe" install catch2:x64-windows ffmpeg:x64-windows miniaudio:x64-windows --classic
cmake --preset windows-vs2026
cmake --build --preset windows-debug
ctest --preset windows-debug
```

Every application build recreates a self-contained, per-configuration
development distribution using Qt's deployment tool. Run it directly without
adding Qt or vcpkg to `PATH`:

```powershell
& '.\build\windows-vs2026\distribution\Debug\Yaap.exe'
```

Release builds are placed in `build/windows-vs2026/distribution/Release`. Set
the `YAAP_DISTRIBUTION_ROOT` CMake cache variable to choose a different root.
These folders are development artifacts; installers, signing, license bundles,
and clean-machine release validation remain part of the production packaging
work.

The generated Visual Studio workspace is:

```text
build/windows-vs2026/Yaap.slnx
```

The VS 2022 preset generates the traditional `Yaap.sln` format instead.

## Deliberate prototype measures

Every shortcut is also marked with `PROTOTYPE` at its implementation site.

1. **Polled playback state:** `PlayerController` reads position and completion
   every 100 ms. Replace this with coalesced playback snapshots from a dedicated
   playback-control layer.
2. **Polled producer backpressure:** a full PCM ring makes the non-real-time
   decoder worker wait in short intervals. Fold this into the asynchronous
   playback-session cancellation mechanism without adding work to the audio
   callback.
3. **Full rescans for watched changes:** directory notifications trigger a
   debounced recursive rescan. Replace this with an incremental change journal
   for very large libraries.
4. **Linux credential adapter:** Linux currently invokes `secret-tool` as an
   adapter to Secret Service. Replace this with a directly linked libsecret or
   D-Bus backend for product packaging.
5. **First-station playlist action:** the prototype UI opens the first valid
   entry in a station playlist. The parser and loader return every entry; a full
   station browser still needs to expose that collection.
6. **Single fixed output format:** all audio is currently 48 kHz stereo float.
   Keep this as the internal mixer format initially, then add explicit device
   negotiation and a measured resampling policy.
7. **Manual mod discovery:** packages are scanned at startup or through the Mods
   dialog. Add atomic install/update/remove operations before watching this
   directory or accepting remotely obtained packages.
8. **Trusted extension execution:** QML extensions run in-process and native
   providers run as the current user. Permissions currently express consent and
   feature gating, not OS-enforced containment. The post-1.0 roadmap moves
   package identity and process hardening ahead of remote distribution.

The miniaudio callback is not a shortcut: it performs no allocation, locking,
logging, decoding, Qt calls, or file access.

## License note

The application must distribute an LGPL-compatible FFmpeg build without GPL or
nonfree components, retain its exact build configuration and corresponding
source, and comply with Qt's selected license. Codec patent obligations require
a separate release review.

## Next steps toward the full version

The locally implementable work in reevaluated [steps 8–15](docs/roadmap.md) is
now integrated. Cryptographically signed artifacts still require a real signing
identity, and live OpenSubsonic/Jellyfin compatibility jobs require pinned test
servers; the build exposes release gates without inventing either credential.

Audio-product work remains parallel to that extension roadmap: ReplayGain,
gapless playback, crossfade, EQ, visualizers, output selection, and OS media
session controls.
