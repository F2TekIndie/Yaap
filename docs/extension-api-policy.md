# Extension API 1.x compatibility policy

Extension API `1.x` follows these rules:

- Manifest schema 1, provider protocol 1, slot identifiers, permission names,
  error codes, and required DTO fields are stable for the lifetime of API 1.x.
- Minor host releases may add optional object fields, methods, slots, and
  permissions. Receivers must ignore unknown optional fields.
- Existing behavior is deprecated in documentation for at least one minor
  release before removal. Removal or a required-field semantic change requires
  Extension API 2.0.
- A package declares an inclusive minimum and exclusive maximum API version.
  Yaap refuses packages outside that range before loading code.
- Permission grants are bound to package version and SHA-256 content digest.
  Any changed package requires a new explicit grant.
- Protocol request IDs are decimal strings, payloads are at most 8 MiB, and
  implementations may have no more than 128 outstanding requests.

Frozen protocol error codes are `invalid_request`, `incompatible_version`,
`permission_denied`, `unavailable`, `timeout`, `cancelled`, and
`provider_error`.

Third-party packages should run `yaap-provider-conformance --package <path>`.
Provider executables should additionally run:

```text
yaap-provider-conformance --executable <path> --provider-id <reverse-domain-id>
```

