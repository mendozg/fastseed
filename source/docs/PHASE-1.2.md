# FastSeed Phase 1.2

Phase 1.2 adds the BSTY wire-message conformance layer required before the
high-concurrency crawler in Phase 2.

## Implemented

- Message command registry.
- VERACK and GETADDR payload helpers.
- ADDR encode/decode with bounded CompactSize count.
- ADDRv2 encode/decode with bounded count/address size and IPv4/IPv6 validation.
- PING/PONG nonce encode/decode.
- Deterministic protocol round-trip tests.
- Frozen Phase 1.2 protocol vector notes.

## Interoperability gate

A real `globalboostd` endpoint must be supplied separately for live handshake
validation. The source tree does not claim live-mainnet interoperability merely
because unit tests pass.
