# Phase 10.2 — Full ADDRv2 / BIP155 network types

Phase 10.2 makes ADDRv2 a first-class FastSeed v1.0.0 protocol feature.

## Supported network IDs

| ID | Network | Status |
|---:|---|---|
| 0x01 | IPv4 | supported |
| 0x02 | IPv6 | supported |
| 0x03 | Tor v2 | rejected/obsolete |
| 0x04 | Tor v3 | supported |
| 0x05 | I2P | supported |

Tor v3 and I2P addresses are represented internally by their 32-byte BIP155 wire values. This phase does not implement Tor SOCKS or I2P SAM transport; it implements address representation and validation.

## Security/conformance

- ADDRv2 entry count is bounded to 1000.
- Address size is bounded to 512 bytes.
- IPv4 is exactly 4 bytes.
- IPv6 is exactly 16 bytes.
- Tor v3 is exactly 32 bytes.
- I2P is exactly 32 bytes.
- Tor v2 is rejected.
- Unknown future network IDs are preserved up to the 512-byte protocol limit.
- CompactSize values are required to use their canonical/minimal encoding.
- Port remains big-endian.

## Scope boundary

FastSeed v1.0.0 can discover, validate, persist, and federate ADDRv2 address records. Actual outbound Tor/I2P transport remains a later feature.
