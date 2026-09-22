# Coding agent guidance

## Scope and layout

This repository contains `jd1ctl`, a small native macOS C utility for the FIIO
JD1 HID interface (VID `0x31B2`, PID `0x2581`). It depends only on IOKit and
CoreFoundation. Keep the project small; add dependencies or abstractions only
when a concrete change needs them.

- `src/jd1ctl.c`: device discovery, HID transactions, gain operations, watcher.
- `src/cli.[ch]`: argument parsing; must stay independent of macOS and hardware.
- `tests/test_cli.c`: hardware-free CLI regression tests.
- `docs/protocol-notes.md`: research notes, not a verified device specification.
- `Makefile`: local build and test entry points.
- `.github/workflows/ci.yml`: macOS build and hardware-free checks.

## Build and validation

From the repository root on macOS with Xcode Command Line Tools:

```sh
make all test CFLAGS='-O2 -Wall -Wextra -Wpedantic -Werror -std=c11'
./build/jd1ctl --help
```

On other platforms, `make test` can validate the portable parser; report that the
macOS application was not built. Build outputs belong in `build/` and must not
be committed. Do not use the legacy ignored root executable for validation.
Use `make clean` when changing compiler flags so that targets are rebuilt.

Tests and help do not access hardware. Do not run device commands as routine
validation. Hardware writes (`set-gain`, `save`, `watch`) require explicit user
authorization for device testing. Never run `watch` casually: it writes to matching
devices on attachment and may keep running indefinitely.

## Implementation rules

Use C11, four-space indentation, `static` for file-local helpers, and existing
naming conventions. Check allocations and IOKit return values. Balance Core
Foundation retain/release calls and device open/close operations. Callback
buffers and contexts must outlive their registration and scheduled use.

Validate all CLI inputs before opening the HID manager. Reject empty numeric
strings, overflow, trailing junk, non-finite gain values, out-of-range attenuation,
and values outside 0.5 dB steps. Never cast floating-point input to an integer
before checking its finite range. Keep invalid input at exit code 2 and help at 0.
Add regression cases when changing parsing behavior.

Preserve VID/PID matching, the -60..0 dB range, `--yes` requirements for
`set-gain`/`save`, and the restricted write scope at register `0x66`. Do not add
generic register writes, boot-mode transitions, or flashing without a specifically
requested scope change. For one-time gain writes, preserve upper register bits,
print the old value, check acknowledgement, and verify readback.

The watcher currently uses a separate direct-write path that zeros upper register
bits and does not verify acknowledgement/readback. Do not describe it as verified
or assume it has the same guarantees as `set-gain`. Changes to this path need
focused protocol review and separately reported hardware validation.

## Documentation and review

Update README usage and tests with CLI changes. Preserve research uncertainty;
do not invent hardware results or claim that parser tests validate USB behavior.
Do not add proprietary OEM tools, firmware dumps, credentials, personal paths,
or generated binaries. Do not select a license on the owner's behalf.

Before finishing, review `git diff --check` and `git status --short`, summarize
changes and checks, and state any untested device behavior. Do not commit, push,
or publish unless the user requests it.
