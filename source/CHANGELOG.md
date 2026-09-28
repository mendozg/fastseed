## 0.10.12 — Phase 10.12

- Integrated adaptive capacity feedback into live crawler probe completion.
- Added dynamic transport admission and `transport_capacity()`.
- Added Phase 10.12 crawler feedback integration test.
- Corrected Phase 10.10 adaptive scheduler fixture timestamps so peers are genuinely due.

## 0.10.4 — Phase 10.4

- Added active ADDRv2 peer exchange after the BSTY handshake.
- Added bounded GETADDR/discovery window.
- Added decoded ADDRv2 records to `ProbeResult`.
- Integrated discovered ADDRv2 records into the crawler as `PeerExchange` candidates.
- Preserved Tor v3/I2P `AwaitingTransport` behavior.
- Added deterministic local interoperability and crawler integration tests.

# Changelog
## Phase 10.14 — Scheduler State Integrity & Multi-Instance Coordination

- Added versioned persistent adaptive scheduler state.
- Added bounded, age-checked scheduler-state restoration.
- Added SQLite schema version 4 and scheduler-state UPSERT persistence.
- Added single-process database locking with a `.lock` file.
- Added restart, stale-state, invalid-state, and multi-instance collision tests.



## 0.10.1 — Phase 10.1

- Added `sendaddrv2` to the BSTY P2P command registry.
- Added empty-payload `encode_sendaddrv2()`.
- Updated `PeerProbe` to advertise ADDRv2 support before `verack`.
- Added golden wire-vector coverage.
- Added local TCP interoperability coverage for the actual probe handshake.
- Preserved the existing Phase 1–9 architecture and validation boundaries.

## 0.10.2
- Add full ADDRv2 network ID handling for IPv4, IPv6, Tor v3, and I2P.
- Reject obsolete Tor v2 addresses.
- Add strict address-length validation and canonical CompactSize parsing.
- Preserve unknown future network IDs within protocol bounds.

## 0.10.3 — Phase 10.3
- Integrated ADDRv2 network types into crawler candidates.
- Added Tor v3 and I2P candidate representations.
- Prevented non-IP ADDRv2 endpoints from entering the TCP probe runner.
- Extended SQLite network classification for Tor v3 and I2P.

## 0.10.8 — Phase 10.8

- Added explicit transport state to peer health records.
- Added transport-specific attempt, success, and failure counters.
- Added persisted last transport error.
- Added persisted `next_probe` scheduling.
- Added bounded exponential cooldown scheduling.
- Persisted transport and cooldown/quarantine state in SQLite schema v3.
- Added automatic v2 → v3 database migration.
- Preserved strict DNS A/AAAA isolation for Tor v3 and I2P.
- Added Phase 10.8 health and persistence tests.

## 0.10.7 — Phase 10.7

- Integrated ADDRv2-aware transport routing into the peer crawler.
- Added candidate-aware probe execution through `TransportProbeRunner`.
- IPv4/IPv6 candidates use direct TCP transport.
- Tor v3 and I2P candidates can use the Phase 10.6 adapters when enabled.
- Unsupported optional transports remain `AwaitingTransport` instead of being treated as failed peers.
- Connected transport sockets are handed directly to the normal BSTY `PeerProbe` handshake.
- Preserved ADDRv2 discovery, cooldown, health, persistence, and DNS isolation boundaries.
- Added Phase 10.7 crawler and transport-router tests.

## Phase 10.10 — Adaptive Probe Scheduling & Network Quotas

- Added independent Direct TCP, Tor SOCKS5, and I2P SAM scheduler quotas.
- Added active-probe accounting and per-network batch limits.
- Added adaptive refresh intervals using availability and latency signals.
- Preserved failure cooldown and quarantine semantics.
- Added Phase 10.10 adaptive scheduler regression tests.

## Phase 10.15
- Added adaptive scheduler evidence decay.
- Added configurable learning half-life and capacity recovery period.
- Reset stale consecutive success/failure streaks.
- Gradually recover learned capacity toward configured transport targets.
- Added deterministic decay tests.

## Phase 10.16
- Added scheduler evidence confidence model.
- Added freshness-weighted effective observation count.
- Added outcome-neutral confidence metric derived from persisted state.
- Added Phase 10.16 confidence tests and documentation.

## Phase 10.17
- Added confidence-aware adaptive scheduling.
- Added bounded exploration priority for low-confidence transports.
- Added controller-aware global and per-transport admission checks.
- Preserved existing scheduler API for non-confidence-aware callers.
- Added Phase 10.17 scheduler tests.

## Phase 10.18

- Added bounded exploration/exploitation scheduling.
- Added configurable exploration fraction and hard exploration bounds.
- Added low-confidence threshold for exploration candidates.
- Preserved adaptive transport quotas and global concurrency ceiling.
- Added Phase 10.18 exploration tests.

## Phase 10.20
- Added thread-safe exploration budget accounting.
- Added per-transport exploration opportunities, selections, quota rejections, and starvation telemetry.
- Added scheduler telemetry snapshot API.
- Added Phase 10.20 exploration telemetry test.
- No database schema migration required.

## Phase 10.22
- Added Prometheus exploration metrics exporter (Phase 10.21 integration).
- Added recording rules for exploration selection, starvation, rejection, and transport share.
- Added alerts for exploration starvation, imbalance, budget underuse, and admission pressure.
- Added minimal Grafana exploration dashboard.

## Phase 10.25

- Added deterministic failure-injection and recovery test suite.
- Added Direct TCP, Tor SOCKS5, and I2P SAM outage simulations.
- Added handshake/protocol failure simulations.
- Verified unsupported transport outcomes do not reduce learned capacity.
- Verified scheduler admission remains bounded after faults.
- Verified capacity recovery and confidence bounds.
- Added Phase 10.25 documentation.

## 0.10.26 — Security Hardening
- Added bounded host/source limits.
- Hardened parser size accounting.
- Hardened candidate admission against oversized hosts and candidate-limit abuse.
- Hardened DNS rate limiting so rejected per-source traffic does not consume global quota.
- Added deterministic adversarial security regression tests.

## Phase 10.28
- Added FederatedPeerStore for bounded multi-seeder peer intelligence.
- Added deterministic conflict resolution, source exclusion, and withdrawal handling.
- Added multi-seeder federation integration test.

## Phase 10.29
- Added controlled BSTY mainnet-canary policy and evaluation.
- Added target bounds and duplicate suppression.
- Added explicit protocol/port guardrails.
- Added deterministic canary integration test.
- No blockchain synchronization or consensus dependency introduced.
