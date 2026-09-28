# Phase 10.8 — Transport-Aware Health & Persistence

Phase 10.8 makes peer health explicitly aware of the transport used to reach a peer and persists that transport lifecycle across restarts.

## Scope

- Direct TCP for IPv4/IPv6
- Tor v3 through the Tor SOCKS5 adapter
- I2P through the I2P SAM adapter
- Transport-specific attempt/success/failure counters
- Last transport error
- Persisted `next_probe`
- Persisted quarantine deadline
- Exponential transport-aware cooldown scheduling
- SQLite schema version 3
- Automatic migration from schema v2
- DNS remains restricted to IPv4/IPv6

## Health model

Each `PeerHealth` record now contains:

- `transport`
- `transport_attempts`
- `transport_successes`
- `transport_failures`
- `transport_last_error`
- `next_probe`
- existing `quarantine_until`

The transport is derived from the ADDRv2 network:

| Network | Transport |
|---|---|
| IPv4 | Direct TCP |
| IPv6 | Direct TCP |
| Tor v3 | Tor SOCKS5 |
| I2P | I2P SAM |

Unknown/legacy candidates continue to use Direct TCP compatibility behavior.

## Cooldown

Failures schedule the next probe using bounded exponential backoff:

```text
initial = 30 seconds
multiplier = 2.0
maximum = 3600 seconds
```

These are configurable through `HealthConfig`. Quarantine remains independently controlled by the existing failure threshold and quarantine duration.

A successful probe clears the transport error and schedules the next normal refresh using the health freshness interval.

## Persistence

SQLite schema version 3 adds:

- `transport_kind`
- `transport_attempts`
- `transport_successes`
- `transport_failures`
- `transport_last_error`

The existing `next_probe` column is now populated and restored as part of health state. Existing v2 databases migrate automatically when opened.

## DNS isolation

Tor v3 and I2P records continue to be excluded from `PeerHealthCache::dns_snapshot()`. They can be healthy, warm, hot, quarantined, and persisted without ever becoming DNS A/AAAA records.

## Tests

Phase 10.8 adds:

- transport-aware health lifecycle test
- transport-aware SQLite persistence test
- schema v3 validation

Regression coverage exercised during Phase 10.8 validation:

- Phase 3 health cache
- Phase 4 persistence
- Phase 10.5 ADDRv2 persistence
- Phase 10.6 transport router
- Phase 10.7 transport-aware crawler/router
- Phase 10.8 health/persistence
