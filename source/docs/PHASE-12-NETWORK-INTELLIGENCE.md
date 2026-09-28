# Phase 12 — Network Intelligence

FastSeed now contains an advisory Network Intelligence layer.

## Signals
- peer reliability
- latency quality
- observation freshness with half-life decay
- IPv4 /24 diversity
- IPv6 /48 diversity
- transport/network diversity

## Safety
Network Intelligence never overrides:
- protocol validation
- quarantine/cooldown
- global capacity
- transport quotas
- candidate bounds
- DNS snapshot isolation

## Validation
The Phase 12 focused test was added and integrated into CMake. The environment's build window expired before the executable completed, so this archive does not claim a Phase 12 test PASS.
