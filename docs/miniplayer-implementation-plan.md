# Miniplayer implementation plan

Status: planning only. This document does not authorize or contain an implementation.

## Objective

Add a compact miniplayer presentation to Yaap's existing frameless Qt Quick window.
The new button beside the close button switches the application from the normal UI
to the miniplayer. In miniplayer mode, the same button uses a restore/maximize icon
and returns to the previous normal window geometry and UI.

This is a presentation change, not an operating-system window-state change:

- Do not call `showMinimized()` when entering the miniplayer.
- Do not call `showMaximized()` when restoring the normal UI.
- Playback, buffering, provider requests, queue state, and dialogs remain owned by
  their existing controllers and must not be restarted by a mode transition.
- The existing close button remains the rightmost control in both modes.

## User-visible behavior

### Normal mode

- Keep the current default size of 900 by 560 logical pixels and minimum size of
  680 by 420.
- Add a compact/minimize `ToolButton` immediately to the left of the close button.
- The button tooltip and accessible name are `Switch to miniplayer`.
- Clicking it records the normal window geometry and changes to miniplayer mode.

### Miniplayer mode

- Use a fixed initial size of 480 by 112 logical pixels. Keep this size fixed for
  the first version so the compact layout has one reliable contract on every OS.
- Show only:
  - a free drag region and the Yaap identity;
  - the current track title, or station title when no track metadata exists;
  - artist/station context on a second, elided line;
  - play/pause and stop controls;
  - a thin seek control when the source has a finite duration;
  - restore/maximize and close buttons at the top right.
- For live radio with no finite duration, omit the seek control rather than showing
  a disabled time range.
- Long titles and station names use middle/right elision and expose the full text in
  a tooltip.
- A left press on any unoccupied chrome/background area calls
  `ApplicationWindow.startSystemMove()`. Interactive controls must not initiate a
  drag.
- The restore button tooltip and accessible name are `Restore full player`.

### Mode and geometry persistence

- Default to normal mode on the first launch.
- Persist the last presentation mode, the last valid normal geometry, and the last
  miniplayer position with `QSettings`.
- On startup and after a monitor configuration change, clamp the selected geometry
  to a `QScreen::availableGeometry()` so at least the complete title bar and both
  window controls are reachable.
- If stored geometry is malformed, implausibly small, or belongs to a disconnected
  screen, fall back to a centered geometry on the primary screen.
- Restore the exact previous normal geometry when leaving miniplayer mode; do not
  always reset to 900 by 560.

## Architecture

### 1. Add a presentation-state controller

Create:

- `src/app/WindowPresentationController.hpp`
- `src/app/WindowPresentationController.cpp`

Expose one small QML-facing object:

```cpp
enum class PresentationMode { Normal, MiniPlayer };

Q_PROPERTY(PresentationMode mode READ mode NOTIFY modeChanged)
Q_PROPERTY(QRect normalGeometry READ normalGeometry NOTIFY normalGeometryChanged)
Q_PROPERTY(QPoint miniPlayerPosition READ miniPlayerPosition
    NOTIFY miniPlayerPositionChanged)
```

Provide invokables or slots to:

- enter miniplayer mode while recording the current normal geometry;
- return to normal mode;
- record the miniplayer position after a successful drag;
- reset invalid persisted state to defaults.

The controller owns settings validation and persistence. It does not own the
`QQuickWindow`, manipulate playback, or contain platform-specific window code. All
methods run on the GUI thread. Register the instance alongside `Player`, `Radio`,
and the other application singletons in `src/app/main.cpp`.

Use versioned settings keys:

- `window/presentation-mode-v1`
- `window/normal-geometry-v1`
- `window/miniplayer-position-v1`

Write only after a completed transition or drag, not on every frame of a move, to
avoid unnecessary settings churn.

### 2. Split the large QML presentation into focused components

Create:

- `qml/Yaap/App/WindowChrome.qml`
- `qml/Yaap/App/FullPlayerView.qml`
- `qml/Yaap/App/MiniPlayerView.qml`

Keep `Main.qml` as the owner of the `ApplicationWindow`, dialogs, themed background,
and mode transition. Move the existing main `ColumnLayout` into `FullPlayerView` and
route its dialog-opening actions back to `Main.qml` with signals.

Instantiate both full and mini views once and switch their `visible` and `enabled`
properties. Do not destroy and recreate `FullPlayerView` with a `Loader`: its
existing `ExtensionHost` children may contain trusted third-party QML state that
must survive a temporary miniplayer transition.

`WindowChrome.qml` receives the host window and presentation mode as explicit
properties and emits `togglePresentationRequested` and `closeRequested`. It owns
the drag-safe free area and the two top-right buttons. The close button remains at
the far right; the compact/restore button sits immediately to its left.

Add all new QML files to the existing `qt_add_qml_module(yaap_app ...)` declaration.

### 3. Add bundled, tintable window-control icons

Create small SVG resources for compact/minimize and restore/maximize instead of
using Unicode glyphs, whose appearance varies by font and operating system. Add
them as QML module resources and tint them through the `ToolButton.icon.color`
property so they follow the active theme.

Use a minimum 44 by 36 logical-pixel hit target for both top-right buttons. Preserve
the current red hover/down treatment for close; use the normal themed hover state
for the presentation toggle.

### 4. Implement the geometry transition in `Main.qml`

Centralize transitions in two functions rather than scattering bindings across the
views:

1. `enterMiniPlayer()` records `x`, `y`, `width`, and `height` through the controller.
2. Apply miniplayer minimum/maximum constraints before assigning the compact size.
3. Clamp and assign the compact position within the active screen's available area.
4. Switch view visibility and move keyboard focus to the miniplayer play/pause
   control.
5. `restoreFullPlayer()` clears compact maximum constraints first, restores the
   normal minimum size, applies the validated saved geometry, and restores focus to
   the normal transport.

Guard transitions against re-entry. A double click or repeated keyboard activation
must produce one completed mode change. Modal dialogs naturally block the chrome;
the transition functions should additionally refuse a programmatic mode change
while a modal dialog is visible.

Listen for screen/available-geometry changes and re-clamp only when needed. Avoid
continuous bindings between stored geometry and live window geometry because they
can fight the native window manager during a drag.

### 5. Build the miniplayer content from existing controller properties

`MiniPlayerView.qml` reads the existing `Player` singleton only:

- `Player.nowPlayingTitle`, `Player.nowPlayingText`, and `Player.title` for the main
  label;
- `Player.nowPlayingArtist`, `Player.stationTitle`, and
  `Player.nowPlayingMetadataStale` for context;
- `Player.hasAudio`, `Player.isPlaying`, `Player.isLoading`, and
  `Player.isBuffering` for control state;
- `Player.positionMilliseconds` and `Player.durationMilliseconds` for seeking.

Call only the existing `Player.play()`, `pause()`, `stop()`, and `seek()` methods.
Do not add previous/next buttons until the playback queue is exposed through a
stable controller API.

Make the layout robust for:

- no selected track;
- local files with a duration;
- radio streams with changing ICY metadata and no duration;
- buffering and reconnecting states;
- very long or non-Latin metadata;
- theme spacing and corner-radius extremes allowed by the theme schema.

### 6. Preserve the mod boundary

Continue applying data-only theme colors, spacing, corners, and the host-owned
background effect in miniplayer mode. Do not render the current full-size extension
slots inside the compact view; those extensions have no compact sizing contract.

Do not add a miniplayer extension slot in this change. If third-party miniplayer UI
is wanted later, introduce a versioned slot such as `miniplayer.toolbar.after` with
an explicit maximum height, permission name, documentation, and conformance tests.
That is an Extension API change and should not be smuggled into this presentation
refactor.

### 7. CMake and source integration

Update `CMakeLists.txt` to:

- compile the new controller into `yaap_app` and `yaap_tests`;
- include the three QML components and SVG resources in `Yaap.App`;
- retain C++20 and the current Qt 6.5 minimum;
- avoid adding a new third-party dependency.

No deployment-script change should be necessary because Qt's QML resource module
embeds the new QML and icons. Confirm this from the generated distribution rather
than assuming it.

## Testing plan

### Catch2 controller tests

Add `tests/WindowPresentationControllerTests.cpp` covering:

- first launch defaults to normal mode and default geometry;
- entering miniplayer records normal geometry exactly once;
- restoring returns the stored normal geometry;
- mode and both geometries survive controller reconstruction through `QSettings`;
- malformed, undersized, and off-screen persisted geometry is rejected or clamped;
- repeated enter/restore requests are idempotent;
- settings from an unknown future schema version fall back safely.

Use the test suite's redirected temporary `QSettings` location so tests never touch
the user's application settings.

### QML and application tests

- Ensure QML cache generation and type registration succeed in the normal build.
- Add a focused Qt Quick test or a small application smoke mode that can assert:
  - the initial full view is visible;
  - clicking compact changes the root size and visible view;
  - clicking restore reinstates the full view and previous geometry;
  - close is present and reachable in both modes;
  - interactive controls do not trigger the window drag handler;
  - long metadata is elided and has a tooltip;
  - radio streams omit the finite-duration seek control.
- Run a playback continuity test that starts a stream, toggles modes twice, and
  verifies the playback generation/source was not reopened.

### Platform and packaging checks

For Windows, Linux, and macOS, manually verify:

- drag behavior of the frameless window;
- multi-monitor restoration, including removing the monitor used on the prior run;
- 100%, 150%, and 200% display scaling;
- keyboard focus and screen-reader names;
- task switcher/dock behavior remains that of one normal application window;
- the Release distribution starts with all miniplayer resources available.

On Windows, build the Release preset and launch
`build/windows-vs2026/distribution/Release/Yaap.exe` as the packaging smoke test.

## Recommended implementation sequence

1. Add and unit-test `WindowPresentationController` with versioned settings.
2. Extract `WindowChrome` without changing current behavior; build and smoke-test.
3. Extract `FullPlayerView` and verify all dialogs and extension slots still work.
4. Add `MiniPlayerView` using only existing `Player` properties and methods.
5. Add the mode transition and validated geometry restoration in `Main.qml`.
6. Add SVG resources, accessibility metadata, focus order, elision, and tooltips.
7. Add QML/playback-continuity tests and exercise multi-screen edge cases.
8. Build the full Release target, run Catch2, and launch the generated distribution.
9. Update user-facing documentation and screenshots after behavior is stable.

## Acceptance criteria

- The normal UI has a presentation-toggle button immediately left of close.
- The button switches to a usable 480 by 112 miniplayer without interrupting audio.
- The miniplayer has restore and close controls at fixed top-right positions.
- Restore returns to the previous normal geometry and full UI.
- Free window areas drag correctly in both modes; buttons and seek controls do not.
- Mode and valid geometry persist safely across launches and monitor changes.
- Existing theme mods still style the host-owned compact UI.
- Existing UI extensions retain their state while the full view is hidden.
- All regular tests, QML generation, Release packaging, and the GUI smoke launch pass.
