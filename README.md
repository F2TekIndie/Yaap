# Yaap prototype

Yaap is a C++20/Qt Quick music-player prototype. Its first vertical slice opens
a local file, decodes it through FFmpeg to 48 kHz stereo float PCM, and sends
that PCM to a miniaudio playback device.

## Dependencies

- CMake 4.2 or newer
- Visual Studio 2026 or 2022 on Windows
- Qt 6.5 or newer with Qt Quick, Quick Controls, and Quick Dialogs
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

1. **Whole-file decoding:** `FFmpegDecoder` decodes the complete track into one
   PCM vector. Replace it with a decode worker feeding a bounded SPSC ring
   buffer before adding radio, long tracks, seeking, or gapless playback.
2. **Polled playback state:** `PlayerController` reads position and completion
   every 100 ms. Replace this with coalesced playback snapshots from a dedicated
   playback-control layer.
3. **Synchronous worker cancellation:** opening a second file joins the previous
   local-file decoder from the GUI thread. Replace it with asynchronous shutdown,
   FFmpeg interrupt callbacks, and I/O deadlines before network playback.
4. **Single fixed output format:** all audio is currently 48 kHz stereo float.
   Keep this as the internal mixer format initially, then add explicit device
   negotiation and a measured resampling policy.
5. **One controller exposed to QML:** this is a narrow vertical-slice boundary,
   not the future plugin API. Mods will receive versioned models and declared UI
   extension points rather than the application controller.

The miniaudio callback is not a shortcut: it performs no allocation, locking,
logging, decoding, Qt calls, or file access.

## License note

The application must distribute an LGPL-compatible FFmpeg build without GPL or
nonfree components, retain its exact build configuration and corresponding
source, and comply with Qt's selected license. Codec patent obligations require
a separate release review.

## Next steps toward the full version

1. Replace whole-file PCM storage with a bounded SPSC ring buffer and continuous
   FFmpeg producer thread.
2. Add interruptible FFmpeg I/O, buffering state, timeouts, seeking, and clean
   asynchronous shutdown.
3. Introduce queue, track, provider, and playback-session domain models outside
   the Qt-facing controller.
4. Add SQLite library indexing, metadata/artwork extraction, folder watching,
   playlists, and durable settings.
5. Add HTTP radio, M3U/PLS/XSPF parsing, ICY metadata, reconnect, and backoff.
6. Implement OpenSubsonic and Jellyfin providers with credentials stored in each
   operating system's secure credential store.
7. Create versioned theme packs, declared QML extension points, plugin manifests,
   permissions, and an out-of-process provider SDK.
8. Add seeking, ReplayGain, gapless playback, crossfade, EQ, visualizers, output
   selection, and OS media-session controls.
9. Add Linux/macOS CI, sanitizers, fuzz tests for parsers, accessibility,
   localization, crash recovery, telemetry policy, and performance budgets.
10. Promote the development distribution to CMake install rules and CPack
    installers; add signing/notarization, SBOM, third-party notices, update
    delivery, and clean-machine release tests.
