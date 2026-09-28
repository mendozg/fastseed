# Phase 13 — BSTY-FastSeed v1.0.0 Production Release

Phase 13 is the production release gate for the complete FastSeed v1.0.0 line after Native DNS (Phase 11) and Network Intelligence (Phase 12).

## Release properties

- Standalone C++20 service.
- Native authoritative DNS; BIND9 and Nginx are not required.
- BSTY mainnet P2P validation on port 8226.
- IPv4/IPv6 A/AAAA DNS responses from an atomic in-memory snapshot.
- SQLite is persistence only and is not accessed by the DNS request path.
- Network intelligence is advisory; protocol safety, quotas, cooldowns, quarantine, and hard resource limits remain authoritative.
- No blockchain synchronization, consensus, transaction, UTXO, mempool, or mining logic.

## Runtime

`bsty-fastseed run` starts the native DNS server, restores persisted peer state, seeds the crawler with the configured BSTY DNS seed set, and refreshes scheduling periodically.

For an unprivileged smoke test, use `bsty-fastseed run --dns-port 8053`. Production DNS normally uses port 53 and therefore requires the appropriate Linux capability/privilege configuration.

## Release gate

The release gate verifies:

1. v1.0.0 version identity.
2. BSTY mainnet protocol constants.
3. Native DNS baseline limits.
4. Mainnet-canary port policy.
5. Phase 12 Network Intelligence tests.
6. Phase 13 protocol and interoperability tests.
7. Release package consistency.

Live public-peer connectivity is intentionally not part of the deterministic release gate; it must be performed as an operator-controlled deployment smoke test.
