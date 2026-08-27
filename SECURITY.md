# Security policy

## Supported versions

Yaap is pre-release software. Security fixes currently target the latest revision
of the default development branch; no stable release line has been declared.

## Reporting a vulnerability

Report suspected vulnerabilities privately through the repository's GitHub
**Report a vulnerability** form (a private security advisory). If private
reporting is not enabled yet, contact the repository owner privately before
sharing details. Do not post credentials, exploit details, or sensitive media and
server information in a public issue.

Include, where possible:

- the affected commit or version and operating system;
- the affected playback source, provider, extension, or UI mod boundary;
- the expected and observed security impact;
- minimal reproduction steps or a proof of concept;
- sanitized logs, stack traces, or crash dumps; and
- any known mitigations.

Areas of particular interest include credential handling, provider process and IPC
boundaries, mod permissions and validation, media/network parsing, radio playlist
and metadata parsing, and unsafe local-file access.

The maintainer will acknowledge reports on a best-effort basis, investigate them,
and coordinate a reasonable disclosure timeline. Please allow time for a fix and
release before publishing vulnerability details.

