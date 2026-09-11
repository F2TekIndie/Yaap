# Theme format compatibility

Theme manifests use schema version 1 and the existing `api` range fields for
compatibility with the host's theme format version (currently 1.1). A package
specifies an inclusive `minimum` and exclusive `maximumExclusive`. Yaap rejects
incompatible packages before reading their theme data.

Only the `theme` package kind is supported. Themes contain validated appearance
data and package-local images, with no executable code, permission grants, or
external development-kit dependency. Legacy permission fields are ignored.

Optional fields may be added without invalidating existing packages. Changes
that require new fields or reinterpret existing theme data should introduce a
new format version and document a migration path.

The [manifest schema](schemas/theme-manifest-v1.schema.json) describes structural
validation. The application also checks package-contained paths, colors, numeric
bounds, image formats and sizes, and usable window geometry. A package hash is
metadata, not a signature or an approval requirement.

See [theme documentation](modding.md) for supported appearance settings.
