# Protocol research notes

These notes preserve the original investigation findings; they have not been
independently verified as part of repository cleanup. OEM tools and firmware
are not included in this repository.

## Firmware readback research

Static analysis of `KT_BOOT_TOOL_1.0.58.exe` found a hidden `KTSPI flash read`
path. In boot mode it builds 64-byte requests beginning with:

```text
46 0B 00 <address-low> <address-mid> <address-high> 38
```

The remaining request bytes are padding. Successful responses begin with
`46`, carry status zero, and return chunks which the OEM tool concatenates and
writes to `./test.bin`. The tool increments the address by `0x38` (56 bytes)
per request. This has not been sent to the JD1: entering boot mode is deferred
until the exact chip identity and a recovery route are established.

The protocol hypothesis comes from the reverse-engineered KT02H20 protocol.
Compatibility with the JD1 must be established from read-only responses before
any write support is considered.
