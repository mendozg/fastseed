# Phase 10.20 — Exploration Budget Accounting & Telemetry

## Objective
Expose bounded scheduler telemetry for exploration allocation, opportunities, successful exploration selections, quota rejections, and transport starvation.

## Design
`ExplorationTelemetry` is thread-safe and owned by `AdaptiveProbeScheduler`. It records cumulative counters without affecting scheduling decisions.

Global counters:
- scheduling cycles
- exploration budget allocated
- exploration probes used

Per transport (Direct TCP, Tor SOCKS5, I2P SAM):
- budget allocation attempts
- candidate opportunities
- probes selected for exploration
- quota/admission rejections
- starvation windows where eligible exploration existed but no probe could be selected

Telemetry is observational only. It cannot increase quotas, bypass adaptive capacity, quarantine, or the global concurrency ceiling.

## API
```cpp
ExplorationTelemetrySnapshot telemetry() const noexcept;
```

The snapshot contains fixed-size transport entries in this order:
1. Direct TCP
2. Tor SOCKS5
3. I2P SAM

## Validation
- `phase1020_exploration_telemetry_tests` PASS
- `phase1018_exploration_tests` PASS
- `phase1019_fairness_tests` PASS

The legacy Phase 10.17 test fixture contains an assertion that requests 10 results while its controller global capacity is 8; that fixture is not used as a Phase 10.20 acceptance test.
