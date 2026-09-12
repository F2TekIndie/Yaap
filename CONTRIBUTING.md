# Contributing to Yaap

Yaap is currently pre-release software. Small fixes and focused improvements are
welcome. Please discuss substantial behavior, architecture, theme format, or UI
changes before investing in an implementation.

Yaap is licensed under `LGPL-3.0-only`. Unless separately agreed in writing,
contributions are submitted under that same project license. Dependency licenses
remain separate from Yaap's license.

## Development setup

Follow the platform setup and CMake preset instructions in [README.md](README.md).
The project requires C++20, CMake, Qt 6, FFmpeg, miniaudio, and Catch2. Dependencies
should be installed outside the repository; do not commit a vcpkg installed tree,
Qt binaries, generated build files, user databases, credentials, or media files.

## Making a change

1. Create a focused branch from the repository's default branch.
2. Keep the change scoped and follow the established C++ and QML style.
3. Add or update tests at stable public boundaries when behavior changes.
4. Update user, theme, or release documentation when applicable.
5. Update `THIRD_PARTY_NOTICES.md` and `sbom.spdx.json` if dependencies or packaged
   components change.
6. Run the relevant configure, build, and test presets before opening a pull
   request.

For example, on Windows:

```powershell
cmake --preset windows-vs2022
cmake --build --preset windows-vs2022-debug
ctest --preset windows-vs2022-debug --output-on-failure
git diff --check
```

Run the release gate before proposing packaging or dependency changes:

```powershell
cmake --build build/windows-vs2022 --target yaap_release_gate --config Debug
```

For a configured local Linux build:

```sh
cmake --build build/linux-local
dbus-run-session -- ctest --test-dir build/linux-local --output-on-failure
cmake --build build/linux-local --target yaap_release_gate
git diff --check
```

The settings UI smoke test exercises General, theme selection, conditional Custom
controls, applying edits, and tray actions. Linux shell changes also have manual
integration smoke tests that require an active niri/Wayland session, Python,
`dbus-run-session`, `gdbus` (MPRIS), and Python `dbus-next` (tray):

```sh
python3 tests/smoke/MprisSessionSmoke.py distribution/linux/Yaap --niri
python3 tests/smoke/TraySessionSmoke.py distribution/linux/Yaap
YAAP_TRAY_TEST_THEME=org.yaap.synthwave-theme \
  python3 tests/smoke/TraySessionSmoke.py distribution/linux/Yaap
```

These scripts launch isolated test instances with temporary settings and a
private session bus. The tray script covers full/miniplayer activation, tray-host
restart, and Quit; its optional theme variable exercises a bundled animated skin.
Release builds refresh `distribution/linux`; a documentation-only change can be
staged with `cmake --build build/linux-local --target yaap_distribution`.

## Pull requests

Pull requests should explain the problem, the chosen solution, user-visible
effects, and how the change was verified. Include screenshots for visible UI or
theme changes. Keep generated files, local settings, test databases, credentials,
and unrelated formatting changes out of the patch.

By submitting a pull request, you confirm that you have the right to contribute
its contents and license them under Yaap's `LGPL-3.0-only` project license.

## Security issues

Do not open a public issue for a suspected vulnerability or include credentials in
logs. Follow [SECURITY.md](SECURITY.md) instead.
