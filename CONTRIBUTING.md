# Contributing to Yaap

Yaap is currently pre-release software. Small fixes and focused improvements are
welcome. Please discuss substantial behavior, architecture, provider API, or UI
modding changes before investing in an implementation.

The repository does not yet have a project license. Contact the repository owner
before contributing code, and do not assume that dependency licenses grant rights
to Yaap itself.

## Development setup

Follow the platform setup and CMake preset instructions in [README.md](README.md).
The project requires C++20, CMake, Qt 6, FFmpeg, miniaudio, and Catch2. Dependencies
should be installed outside the repository; do not commit a vcpkg installed tree,
Qt binaries, generated build files, user databases, credentials, or media files.

## Making a change

1. Create a focused branch from the repository's default branch.
2. Keep the change scoped and follow the established C++ and QML style.
3. Add or update tests at stable public boundaries when behavior changes.
4. Update user, provider, modding, or release documentation when applicable.
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

## Pull requests

Pull requests should explain the problem, the chosen solution, user-visible
effects, and how the change was verified. Include screenshots for visible UI or
theme changes. Keep generated files, local settings, test databases, credentials,
and unrelated formatting changes out of the patch.

By submitting a pull request, you confirm that you have the right to contribute
its contents. A project contribution license will need to be documented when the
repository owner selects Yaap's license.

## Security issues

Do not open a public issue for a suspected vulnerability or include credentials in
logs. Follow [SECURITY.md](SECURITY.md) instead.

