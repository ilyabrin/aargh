# Security Policy

## Supported versions

Only the latest release receives fixes.

| Version | Supported |
| ------- | --------- |
| 1.x     | Yes       |
| < 1.0   | No        |

## Reporting a vulnerability

Please **do not open a public issue** for security problems.

Report privately through GitHub instead:
[Report a vulnerability](https://github.com/ilyabrin/aargh/security/advisories/new)

Helpful things to include:

- The argh.h version (or commit)
- Compiler, platform and build flags
- The `argv` input that triggers the problem, ideally as a minimal program

You can expect a first response within 7 days. Once a fix is ready, it will be
released and disclosed in a GitHub security advisory, crediting you unless you
prefer otherwise.

## Scope

argh.h parses `argv`, which often comes from untrusted users or scripts.
Memory-safety bugs (out-of-bounds reads or writes, use of uninitialized
memory, crashes) on any input are treated as security issues.
