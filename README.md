# jd1ctl

A native macOS command-line utility for investigating the vendor-specific HID
interface on the FIIO JD1 Type-C cable (`31b2:2581`). Uses Apple's IOKit and
CoreFoundation frameworks, with no third-party runtime dependencies or driver
installation.

## Build

Requires macOS and Xcode Command Line Tools (`xcode-select --install`).

```sh
make
./build/jd1ctl --help
make test
```

The executable is built locally in `build/`. Tests cover command-line parsing
without opening a device. The full application requires macOS; the parser tests
use standard C and can also run on other platforms with a C compiler.

## Usage

Connect the JD1, then run the desired command:

| Command | Behavior |
| --- | --- |
| `./build/jd1ctl probe` | Send the KTMicro handshake and print the response |
| `./build/jd1ctl read 0x66` | Read a register; addresses accept decimal or hexadecimal |
| `./build/jd1ctl gain` | Read the digital DAC gain |
| `./build/jd1ctl set-gain -20 --yes` | Set both channels to -20 dB and verify by reading back |
| `./build/jd1ctl save --yes` | Request that the device save its current configuration to flash |
| `./build/jd1ctl watch -20` | Stay running and write -20 dB when matching devices attach |

Gain values must be from -60 to 0 dB in exact 0.5 dB increments. `set-gain`
prints the previous raw register value, preserves its upper 16 bits, checks the
write acknowledgement, and verifies the result. `save` persists the device's
current configuration, not just the gain. Both commands require `--yes`.

`watch` provides host-side persistence while the process runs; stop it with
Ctrl-C. It writes automatically, including to matching devices already connected
at startup. The current watcher sends a direct write to register `0x66` with the
upper 16 bits set to zero. It does not read the old value, check an acknowledgement,
or verify the result. Its success message only indicates that macOS accepted the
output report. Use `set-gain` for a verified one-time change.

Exit codes: `0` for success/help, `1` for device or transaction failure, and `2`
for invalid arguments. If no matching interface is found, check the connection
and VID/PID. A successful build or CI run does not establish device compatibility.

## Development

- `src/jd1ctl.c`: macOS HID transport, register operations, and device watcher.
- `src/cli.c` and `src/cli.h`: hardware-independent argument parsing.
- `tests/test_cli.c`: valid commands, confirmation requirements, and invalid numeric inputs.
- [Protocol notes](docs/protocol-notes.md): original firmware readback research.
- [AGENTS.md](AGENTS.md): guidance for AI coding agents and contributors.

Run `make all test` before submitting changes. CI builds on macOS with warnings
as errors and runs only hardware-free checks. `make clean` removes generated
build files. Device-level behavior has not been validated by these tests.

The tool has no generic register-write, boot-mode, or firmware-flashing command.
Protocol research remains experimental; retain the distinction between observed
behavior and protocol hypotheses when contributing.

No license has been selected yet. Add an owner-approved license before presenting
this project as open source.
