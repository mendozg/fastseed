# Phase 3 — Peer Health & Cache

Phase 3 turns successful Phase 2 probes into a bounded in-memory peer health layer. The cache is a consumer of probe results; it does not perform network I/O, DNS resolution, blockchain synchronization, or database I/O.

## Lifecycle

```text
DISCOVERED -> CANDIDATE -> PROBING -> VALIDATED -> WARM -> HOT
                                      \-> COLD -> COOLDOWN / QUARANTINED
```

- First successful validation enters `WARM`.
- A peer becomes `HOT` after the configured minimum number of successful observations with a consecutive success streak.
- Failures reset the success streak.
- A previously healthy peer moves to `COLD` after a failure.
- Repeated failures can move a peer to `QUARANTINED` for a bounded interval.

## Health data

Each peer tracks:

- protocol version, services, user agent
- first/last seen
- last success/failure
- successful and failed probes
- consecutive success/failure streaks
- last/average/p95 latency
- bounded latency history (default 32 samples)
- availability ratio
- last error and quarantine deadline

## Freshness

DNS-ready snapshots contain only fresh `HOT` peers, optionally including fresh `WARM` peers. The snapshot is produced entirely from memory. No DNS or database operation is performed by the cache.

Baseline freshness is 30 minutes. Phase 5 will use this cache as the memory-only source for DNS responses.

## Configuration

```toml
[cache]
hot_ipv4 = 1000
hot_ipv6 = 500
warm_max = 10000
cold_max = 50000

[health]
minimum_observations_for_hot = 3
freshness_seconds = 1800
quarantine_seconds = 3600
quarantine_after_failures = 3

[health.latency]
history_size = 32
```

## Safety properties

- No peer is trusted merely because it was advertised by ADDR/ADDRv2.
- Only completed Phase 2 probe results update health state.
- DNS selection never performs network or database I/O.
- Latency history is bounded.
- Cache capacity is bounded by HOT/WARM/COLD limits.
- Quarantined peers are excluded from DNS snapshots.
