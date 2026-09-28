# Phase 2 — High-Concurrency Peer Crawler

Phase 2 builds the scheduler layer above the Phase 1 protocol engine. It does not add blockchain synchronization, consensus, mempool, mining, or trust in unvalidated peer advertisements.

## Scope

- bounded candidate queue
- candidate identity by host + port
- candidate lifecycle: QUEUED → PROBING → VALIDATED or COOLDOWN
- bootstrap/manual/ADDR/ADDRv2/persistent-database/peer-exchange provenance
- asynchronous Boost.Asio probe scheduling
- configurable concurrency and connection limits
- connection, handshake and timeout metrics
- IPv4/IPv6 candidate accounting
- exponential cooldown after failures
- validated-peer callback for Phase 3 cache integration

## Configuration baseline

```toml
[crawler]
max_concurrent_probes=256
connect_timeout_ms=1500
handshake_timeout_ms=2500
[crawler.cooldown]
initial_seconds=30
maximum_seconds=3600
backoff_multiplier=2.0
[limits]
max_open_connections=2048
max_candidates=100000
```

## Safety properties

The crawler never treats ADDR/ADDRV2 entries as trusted. Every candidate is passed through the Phase 1 P2P probe. DNS/database work is not performed by the crawler scheduler itself; those sources are adapters for later phases.

## Bootstrap DNS resolution

`add_bootstrap_seed()` asynchronously resolves a configured DNS seed and converts each returned endpoint into an IP:port candidate. The resolved addresses are then independently probed through the Phase 1 protocol engine.
