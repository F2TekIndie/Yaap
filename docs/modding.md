# Yaap extension API 1.1

Yaap discovers extension packages in the distribution `mods` directory and the
user application-data `mods` directory. Each direct child is one package and
must contain `manifest.json`.

## Trust model

- Themes contain validated JSON tokens and are data-only.
- QML extensions run inside Yaap. They are trusted code, even though the host
  exposes only versioned singleton APIs and declared extension slots.
- Provider executables run in a separate process. This isolates crashes and
  bounds IPC, but it is not an operating-system security sandbox. Install only
  provider executables you trust until a WASM or native OS sandbox is added.

Yaap requires the user to grant every declared permission before enabling a
package. Revoking grants disables the package and stops its provider process.
These declarations support informed consent and host feature gating; they do
not turn in-process QML or a native executable into a security sandbox. Package
digest binding, signing, and OS-level containment are tracked in the
[post-1.0 roadmap](roadmap.md).

Permission grants are stored with the package version and a deterministic
SHA-256 of every package file. Any content change invalidates the grant. The
optional publisher fields are displayed as declared identity only; they are
not a cryptographic signature. Yaap has no remote catalog or automatic mod
updates while publisher-signature verification remains unavailable.

## Manifest

The schema version is `1`. Extension API compatibility uses an inclusive
minimum and exclusive maximum:

```json
{
  "schemaVersion": 1,
  "id": "org.example.my-mod",
  "name": "My Mod",
  "version": "1.0.0",
  "api": {"minimum": "1.0", "maximumExclusive": "2.0"},
  "kind": ["theme", "ui-extension", "provider"],
  "permissions": []
}
```

IDs are lowercase reverse-domain identifiers with at least three segments.
Package paths must be relative, must exist, and may not resolve outside the
package through `..` or a symbolic link.

## Themes

Theme mods point to a JSON document through `theme.data`. Schema 1 requires the
seven palette values used by `Theme` and bounded `cornerRadius` and `spacing`
metrics. The optional data-only `background.effect` token accepts `none` or
`waves`; the trusted host renders the effect, so a theme cannot execute code.
Theme changes are validated completely before being applied. API 1.1 adds the
host-owned `spectrum` background effect. A theme may configure 8–48 columns,
mirroring, opacity, and bounded attack/release durations through the optional
`background.parameters` object. It may also provide validated `gradientStart`,
`gradientMiddle`, and `gradientEnd` colors. A spectrum theme can opt into the
host-owned hue slider with `hueShiftAdjustable: true`; the selected hue is
stored per theme and only shifts the effect gradient. Themes receive no PCM and
still execute no code.

## UI extensions

API 1.x exposes these slots:

- `navigation.primary`
- `nowPlaying.aboveTransport`
- `nowPlaying.toolbar.after`

Each component requires the matching `ui.extend:<slot>` permission. Components
can import `Yaap.ModApi 1.0` for `Theme` and read-only API metadata. API 1.1 also
provides the read-only `AudioVisualization` singleton with 48 logarithmic
frequency bands, peak levels, center frequencies, RMS, and overall peak. It
publishes bounded normalized analysis data rather than raw PCM. Extensions that
consume it remain trusted QML code; ordinary theme packages can only select and
configure host-owned renderers. Extensions cannot receive the private application
controllers through a root context.

An API 1.1 QML extension can use the model directly:

```qml
import QtQuick
import Yaap.ModApi 1.1

Row {
    Repeater {
        model: AudioVisualization
        Rectangle {
            required property real level
            required property real frequencyHz
            width: 4
            height: level * 100
            color: Theme.accent
        }
    }
}
```

Levels and peaks are normalized to `0.0`–`1.0`; frequencies are in hertz. The
model updates at most about 30 times per second. Consumers must not assume access
to PCM samples or the audio-device thread.

## Provider extensions

Provider packages declare an executable for each supported platform. Yaap
launches enabled providers with a random local-socket name, a one-time nonce,
and protocol version. See [provider-protocol.md](provider-protocol.md).

The sample packages under `samples/mods` demonstrate all three package kinds.
Bundled theme examples include Ocean, the warm light Paper theme, neon
Synthwave, and a deliberately square High Contrast theme. They are copied into
the runnable development distribution and can be enabled from the Mods dialog.
