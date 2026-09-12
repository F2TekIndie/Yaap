# Yaap theme packages

Yaap discovers theme packages in the distribution `mods` directory and the
user application-data `mods` directory. Each direct child is one package and
must contain `manifest.json`.

## Theme validation

Themes contain validated JSON settings and are selected from Settings → Themes.
There are no permission grants or trust dialogs. Package paths and theme data
are validated before a theme can be used. Legacy manifest permission fields
are ignored. Package hashes remain metadata, not an approval requirement.

## Manifest

The [theme manifest schema](schemas/theme-manifest-v1.schema.json) uses version `1`.
The retained `api` compatibility field uses an inclusive
minimum and exclusive maximum:

```json
{
  "schemaVersion": 1,
  "id": "org.example.my-mod",
  "name": "My Mod",
  "version": "1.0.0",
  "api": {"minimum": "1.0", "maximumExclusive": "2.0"},
  "kind": ["theme"],
  "theme": {"data": "theme.json"}
}
```

IDs are lowercase reverse-domain identifiers with at least three segments.
Package paths must be relative, must exist, and may not resolve outside the
package through `..` or a symbolic link.

## Themes

Theme mods point to a JSON document through `theme.data`. Schema 1 requires the
seven palette values used by `Theme` and bounded `cornerRadius` and `spacing`
metrics. The optional data-only `background.effect` token accepts `none`,
`waves`, `spectrum`, or `paperPlanes`; the trusted host renders the effect, so a
theme cannot execute code.
Theme changes are validated completely before being applied. A theme may add one
optional package-local PNG or SVG `background.image` underneath its single
host-owned effect. The image object accepts an `asset`, `fit`
(`preserveAspectFit`, `preserveAspectCrop`, or `stretch`), nine-point
`alignment`, and opacity from 0 to 1. Image paths cannot escape the package;
files and decoded PNG dimensions are bounded. Setting `windowShape` to `true`
also uses the image alpha as the native window's visible and input-region hint;
themes should keep all essential controls inside the opaque silhouette.

The optional `layout` object lets a shaped theme declare the rectangular safe
area used by every normal player control and place the close button independently.
`controlArea` accepts bounded `leftInset`, `topInset`, `rightInset`, and
`bottomInset` values. `closeButton` accepts bounded `rightInset`, `topInset`,
`width`, and `height` values. Missing values use the host defaults. Themes cannot
name, replace, or execute individual controls; they only position the trusted
host-owned interface. Application dialogs use separate frameless, resizable
windows so the main skin's native mask does not clip them. Each dialog has a
host-defined initial size and persists its resized width and height through the
application settings.

The optional `miniPlayer` object styles the host-owned compact presentation. Its
`size` is currently fixed to 480 by 112 logical pixels. `layout.controlArea` uses
the same four inset names as the normal layout; `layout.windowControls` accepts
`rightInset`, `topInset`, `buttonWidth`, `buttonHeight`, and `spacing` for the
restore and close pair. `background.image` accepts the same image fields as the
normal background and can independently set `windowShape`. All compact content
and both window controls must remain inside an opaque shaped-image region.

`miniPlayer.background.effect` accepts `followTheme` (the default), `none`,
`waves`, `spectrum`, or `paperPlanes`. Settings lets users override this choice
per theme. Compact effects render at reduced opacity, with at most 24 spectrum
columns or five smaller paper planes. Hidden windows unload the renderers.
Static PNG or SVG silhouettes remain supported alongside the effects.

API 1.1 adds the host-owned `spectrum` background effect. A theme may configure
8–48 columns,
mirroring, opacity, and bounded attack/release durations through the optional
`background.parameters` object. It may also provide validated `gradientStart`,
`gradientMiddle`, and `gradientEnd` colors. A spectrum theme can opt into the
host-owned hue slider with `hueShiftAdjustable: true`; the selected hue is
stored per theme and only shifts the effect gradient. Themes receive no PCM and
still execute no code.

```json
"background": {
  "image": {
    "asset": "assets/background.svg",
    "fit": "preserveAspectFit",
    "alignment": "center",
    "opacity": 0.8
  },
  "effect": "spectrum",
  "parameters": {
    "columns": 48,
    "mirror": true
  }
},
"layout": {
  "controlArea": {
    "leftInset": 64,
    "topInset": 84,
    "rightInset": 216,
    "bottomInset": 84
  },
  "closeButton": {
    "rightInset": 170,
    "topInset": 48,
    "width": 44,
    "height": 36
  }
},
"miniPlayer": {
  "size": {"width": 480, "height": 112},
  "layout": {
    "controlArea": {
      "leftInset": 16,
      "topInset": 8,
      "rightInset": 96,
      "bottomInset": 8
    },
    "windowControls": {
      "rightInset": 8,
      "topInset": 8,
      "buttonWidth": 36,
      "buttonHeight": 32,
      "spacing": 4
    }
  },
  "background": {
    "image": {
      "asset": "assets/miniplayer.svg",
      "fit": "stretch",
      "alignment": "center",
      "opacity": 1.0,
      "windowShape": true
    }
  }
}
```

## Custom appearance in Settings

Custom copies the active theme on first use and subsequently restores its saved
values. The editor exposes 11 common appearance settings: seven palette colors,
corner radius, spacing, main background effect, and miniplayer background effect.
When either window explicitly selects Spectrum, four additional controls expose
the three gradient colors and spectrum opacity. A miniplayer following a main
Spectrum background also uses these settings.

Skin images, safe-area insets, button geometry, fixed miniplayer dimensions,
spectrum column count, and animation timing stay in the theme data. They are
preserved when editing an existing custom theme but are not shown in the form.
Apply custom theme validates and saves the full draft; Revert edits discards
unapplied changes. Theme packages can still configure the full format above.

Ocean and High Contrast configure the palette and metrics; Ocean adds Waves.
Paper adds shaped images, matching control geometry, and Paper planes. Synthwave
adds shaped images, geometry, and Spectrum parameters. Those skin-specific
details are the reason the package format is larger than the Custom editor.

## Supported package kinds

Only data-only theme packages are supported. UI extension packages are rejected,
including previously installed packages. Animated backgrounds and spectrum
visualization remain built-in theme features.

## Sample packages

Account services and custom provider extensions are no longer supported.
Provider manifests are rejected, including previously installed packages.

The sample packages under `samples/mods` demonstrate themes.
Bundled theme examples include Ocean, the warm light Paper theme, neon
Synthwave, and a deliberately square High Contrast theme. They are copied into
the runnable development distribution and can be selected from Settings → Themes.
The [README theme gallery](../README.md#sample-themes) shows captures from the
actual Release application rather than design mockups.
