# Phase 1.3 — Protocol Conformance and Interoperability

Phase 1.3 adds canonical message framing, an explicit VERSION/VERACK handshake
state machine, deterministic wire vectors, malformed-frame tests, and a local
TCP interoperability harness.

The production probe remains compatible with real BSTY `globalboostd` peers;
this test suite does not require a public node and therefore remains suitable
for deterministic CI execution.

## Acceptance

- Canonical BSTY message encoding/decoding
- VERSION/VERACK state transitions
- PING/PONG nonce validation
- Golden VERACK vector
- TCP request/response interoperability
- Existing Phase 1, 1.1 and 1.2 suites preserved
