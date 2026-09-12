# Yaap

Yaap is a cross-platform C++20 and Qt Quick music player. The current 0.2
prototype plays local files, internet streams, and radio stations through FFmpeg
and miniaudio. Account services and custom providers have been removed.
Its interface is frameless, supports a compact miniplayer, and can be restyled
with validated third-party theme packages.

## Sample themes

These screenshots were captured from the actual Windows Release application at
its default 900 × 560 logical-pixel size.

| Ocean | Paper |
|---|---|
| ![Yaap Ocean theme](docs/screenshots/ocean.png) | ![Yaap Paper theme](docs/screenshots/paper.png) |

| Synthwave | High Contrast |
|---|---|
| ![Yaap Synthwave theme](docs/screenshots/synthwave.png) | ![Yaap High Contrast theme](docs/screenshots/high-contrast.png) |

Ocean renders animated waves, Paper combines a shaped paper-plane window with
independently moving paper planes, and Synthwave layers its shaped neon frame
over a mirrored audio spectrum with an adjustable hue. The miniplayer follows
the theme's animation by default, with a separate effect choice in Settings.

## Implemented features

### Playback

- Continuous FFmpeg decoding into a bounded 48 kHz stereo float SPSC PCM ring.
- Interruptible network I/O, 15-second deadlines, seeking, prebuffering,
  cancellation, reconnect, and capped exponential backoff.
- Real-time-safe miniaudio output with persistent volume and mute controls.
- ICY and timed metadata support for live station title, artist, and track data.
- M3U/M3U8, PLS, and XSPF station playlist parsing.

### Library and services

- Incremental SQLite library indexing using file size and modification time,
  FFmpeg metadata and artwork extraction, bounded artwork storage, folder
  watching, playlists, and non-destructive handling of incomplete scans.
- Radio Browser discovery with mirror failover, country-popular and name-search
  views, explicit station saving, click reporting, and a bounded offline cache.

### Interface and themes

- Frameless normal and popup windows with persisted sizes and a draggable
  background; shaped themes keep controls inside declared safe areas.
- A fixed-size miniplayer with restore, close, playback, mute, volume, and seek
  controls and subdued animated backgrounds. Hidden windows unload their effects.
- Validated data-only theme packs using theme format compatibility version 1.1,
  built-in animated backgrounds, and audio visualization. No external SDK,
  executable extensions, permission grants, or theme enable/disable switches.
- Settings with General runtime controls and Themes appearance controls,
  including a simplified Custom editor and Linux DMS appearance integration.
- Linux MPRIS controls, single-instance activation, background playback, and
  a system tray menu for opening either player window or quitting.

## Dependencies

- CMake 4.2 or newer
- C++20 compiler
- Visual Studio 2026 or 2022 on Windows, or Ninja on Linux/macOS
- Qt 6.5 or newer with Core, Concurrent, Gui, Widgets, Network, Qml, SQL/SQLite,
  Svg, Quick, Quick Controls, and Quick Dialogs; Linux also requires DBus
- FFmpeg development libraries: `avformat`, `avcodec`, `avutil`, `swresample`
- miniaudio
- Catch2 3
- vcpkg for the supplied presets, or separately installed development dependencies
  with a direct CMake configuration

The supplied presets use the global vcpkg classic tree and explicitly disable manifest mode,
so the project does not create a duplicate `vcpkg_installed` directory. Qt can
come from an official prebuilt SDK or another CMake package location.

## Configure, build, and test

Install vcpkg once outside the repository and point CMake at the global classic
tree and your Qt SDK. For example on Windows:

```powershell
$env:VCPKG_ROOT = 'C:\src\vcpkg'
$env:CMAKE_PREFIX_PATH = 'C:\Qt\6.8.3\msvc2022_64'
& "$env:VCPKG_ROOT\vcpkg.exe" install catch2:x64-windows ffmpeg:x64-windows miniaudio:x64-windows --classic

cmake --preset windows-vs2022
cmake --build --preset windows-vs2022-debug
ctest --preset windows-vs2022-debug
```

Visual Studio 2022 is generated at `build/windows-vs2022/Yaap.sln`. Use the
`windows-vs2026`, `windows-debug`, and `windows-release` presets for Visual
Studio 2026 and its `.slnx` solution.

Qt Creator can open the repository's top-level `CMakeLists.txt` directly. Select
the desired CMake preset and a kit whose Qt installation matches
`CMAKE_PREFIX_PATH`; no separate Qt Creator project files are required.

Linux and macOS use the supplied Ninja presets after `VCPKG_ROOT` and the Qt
package path are configured:

```sh
cmake --preset linux-ninja   # or macos-ninja
cmake --build --preset linux-debug
ctest --preset linux-debug
```

A direct Linux configuration can use system dependencies without vcpkg:

```sh
cmake -S . -B build/linux-local -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/gcc_64 -DBUILD_TESTING=ON
cmake --build build/linux-local
dbus-run-session -- ctest --test-dir build/linux-local --output-on-failure
```

If dependencies are outside standard search paths, provide `Catch2_DIR`,
`MINIAUDIO_INCLUDE_DIR`, `FFMPEG_INCLUDE_DIR`, and the individual
`FFMPEG_<COMPONENT>_LIBRARY` cache paths. These paths are local machine settings.

## Windows Release distribution

A Windows Release build recreates a self-contained development distribution at
the project root. Debug builds do not modify it.

```powershell
cmake --build --preset windows-release
& '.\distribution\Yaap.exe'
```

Set the `YAAP_DISTRIBUTION_ROOT` CMake cache variable to choose another Release
destination. Installers, signing, notarization, and clean-machine release
validation remain separate release gates.

## Linux Release distribution

A Linux Release build refreshes `distribution/linux` automatically. Windows
Release builds preserve this directory, and Linux builds do not change the
Windows files in `distribution`. Debug builds do not refresh either layout.

```sh
cmake --preset linux-release
cmake --build --preset linux-release
./distribution/linux/Yaap
```

The `yaap_distribution` target can also refresh the layout explicitly. It stages
CMake's install rules and Qt's QML/runtime deployment before replacing the Linux
directory. The layout includes Qt libraries, plugins, QML imports, sample mods,
documentation, and license notices. The launcher supplies bundled
library paths to the application.

This is a development distribution for compatible Linux systems, not a universal
AppImage: system libraries (including system-installed FFmpeg), and graphics drivers remain host dependencies. Clean-machine validation and
dependency license/source compliance remain release gates. A custom
`YAAP_DISTRIBUTION_ROOT` places Linux output in its `linux` subdirectory.

## Linux shell integration

Yaap exposes `org.mpris.MediaPlayer2.Yaap` on the session bus. DMS/Quickshell and
other MPRIS clients can display track metadata and control play/pause, stop,
volume, and local-file seeking. Library artwork is published when available.
Next/previous, shuffle, and repeat are not advertised because the application
does not yet connect playback to a queue. Authenticated stream URLs are never
included in public metadata.

With a working Linux session bus, closing the window keeps Yaap running by
default. Change this through **Settings → General → Keep playing when the window closes**.
Reopening Yaap activates the existing window. **Settings → General → Quit Now**,
**Ctrl+Q**, or `Yaap --quit` stops the process. Hidden windows stop their animated
backgrounds. Without a session bus, closing the main window exits normally.

While background mode is enabled, Yaap also shows a system tray icon. Left-click
opens the full player; right-click offers **Open**, **Open miniplayer**, and
**Quit**. On DMS, the shell renders this exported D-Bus menu. The icon remains
available while the window is open, disappears when background mode is disabled
or Yaap quits, and re-registers if the tray host restarts. If no tray host is
running, reopening Yaap or using MPRIS can still restore the window.

```sh
./distribution/linux/Yaap --background  # Start hidden
./distribution/linux/Yaap               # Show the existing instance
./distribution/linux/Yaap /path/song.flac # Open and play in that instance
./distribution/linux/Yaap --quit
```

The installed desktop entry is `org.yaap.Yaap.desktop`; ensure the installed
`bin` directory is on PATH when using its launcher. Window activation requests
are subject to the Wayland compositor's focus policy. MPRIS works with the
shell's existing media UI; animated shell popups require a separate extension.

Linux builds require Qt DBus. Run tests inside a session bus, including in CI:

```sh
dbus-run-session -- ctest --preset linux-debug --output-on-failure
```

## Theme documentation

**Settings → Themes** lists Default, loaded theme packages, Custom, and DMS on Linux. Custom
starts from the current theme on first use and restores your saved custom theme
afterwards. Its controls cover seven palette colors, corner radius, spacing,
and main/miniplayer animation choices. Spectrum colors and opacity appear only
when Spectrum is selected. Skin images, control positions, and animation timing
remain inherited from the source theme or existing custom settings. Use **Apply custom theme**
to validate and save edits, or **Revert edits** to discard the draft. The current
miniplayer size remains fixed at 480 × 112. Background images use local PNG/SVG
files; keep those files available for subsequent launches.

On Linux, **DMS (DankMaterialShell)** follows the shell's exported light/dark
palette, corner radius, and popup opacity. Yaap reads the DMS files under the
XDG cache, config, and state directories and checks for changes once per second
while this theme is selected. It never modifies DMS settings. Missing or invalid
data keeps the last valid palette (or Yaap's default until DMS becomes available),
with a status message in Settings. DMS does not supply Yaap background animations
or window shapes. Its separate **Miniplayer background** selector can still use
any built-in animation while retaining DMS colors. Use Custom or a loaded theme
to configure a main-window animation.

The main toolbar uses icons with accessible names and hover tooltips for Library,
Radio, Open file, Open stream, and Settings.

- [Themes, miniplayer, and visualization](docs/modding.md)
- [Theme format compatibility policy](docs/theme-format-policy.md)
- [Player roadmap and remaining release gates](docs/roadmap.md)

Repository contribution and vulnerability-reporting guidance is available in
[CONTRIBUTING.md](CONTRIBUTING.md) and [SECURITY.md](SECURITY.md).
The first-upload and repository-settings checklist is in
[docs/github-publishing.md](docs/github-publishing.md).

## Remaining prototype limitations

- Playback position and completion are coalesced by a 100 ms GUI timer rather
  than published as playback-session snapshots.
- Directory notifications still trigger a debounced recursive traversal. File
  fingerprints avoid rereading metadata for unchanged tracks, but a filesystem
  change journal would scale better for very large collections.
- Opening a station playlist from the basic stream dialog selects its first
  valid entry rather than presenting the complete collection.
- The internal mixer format is fixed at 48 kHz stereo float; explicit device
  negotiation is not implemented.
- Theme installation, updates, and removal are manual.

Planned audio-product work includes ReplayGain, gapless playback, crossfade,
equalization, output-device selection, and media-session controls on Windows
and macOS.

## Licensing

Yaap is licensed under the
[GNU Lesser General Public License version 3 only](LICENSE), identified by the
SPDX expression `LGPL-3.0-only`. Dependency licenses remain separate from the
Yaap project license.

Commercial distribution must use an LGPL-compatible FFmpeg build without GPL
or nonfree components, retain the applicable build configuration and
corresponding source offer, comply with the selected Qt license, and include
the notices in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and
[sbom.spdx.json](sbom.spdx.json). Codec patent obligations require a separate
release review.
