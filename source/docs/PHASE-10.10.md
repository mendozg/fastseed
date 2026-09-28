# Phase 10.10 — Adaptive Probe Scheduling & Network Quotas

Phase 10.10 makes crawler scheduling transport-aware at the concurrency level.

## Features

- independent Direct TCP, Tor SOCKS5, and I2P SAM quotas;
- active-probe accounting prevents one transport from consuming the entire crawler budget;
- per-network batch limits;
- adaptive refresh intervals based on peer state, availability, and observed latency;
- healthy/low-latency peers may refresh less frequently;
- unreliable or high-latency peers refresh sooner, bounded by minimum/maximum intervals;
- failure cooldown remains authoritative after a recent failure;
- quarantined peers remain excluded;
- unknown networks remain unscheduled;
- Tor v3 and I2P never enter DNS A/AAAA output.

## Default quotas

| Transport | Max inflight | Batch |
|---|---:|---:|
| Direct TCP | 192 | 192 |
| Tor SOCKS5 | 32 | 32 |
| I2P SAM | 32 | 32 |

Total default transport concurrency is therefore 256.

## Adaptive policy

Base refresh comes from the peer state. With at least three observations:

- availability >= 90% and average latency <= 250 ms: refresh interval doubles;
- availability <= 50% or average latency >= 1500 ms: refresh interval halves;
- all intervals remain bounded by the configured minimum and maximum.

A recent failure uses its persisted `next_probe` cooldown instead of the success-based adaptive interval.
