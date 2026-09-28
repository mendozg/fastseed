# Phase 10.25 — Failure Injection & Recovery

## Objective

Validate that BSTY-FastSeed remains bounded and recoverable when transports, protocol handshakes, persistence sinks, or observability sinks fail.

## Scope

- Direct TCP timeout/failure injection
- Tor SOCKS5 failure injection
- I2P SAM failure injection
- P2P handshake/protocol failure injection
- unsupported transport isolation
- scheduler admission after faults
- simulated persistence/metrics sink failure
- recovery through successful probes
- capacity and confidence bounds

## Safety invariants

- Global active probes never exceed `global_max_inflight`.
- Per-transport learned capacity never exceeds configured maximums.
- Per-transport capacity never falls below configured minimums.
- Unsupported transport outcomes do not reduce capacity.
- Confidence remains in `[0,1]`.
- Scheduler remains usable when external persistence/metrics sinks are unavailable.

## Failure model

This phase is deterministic. It does not deliberately take public Internet services, Tor, I2P, or production databases offline. External failures are represented by injected outcomes so CI remains reproducible.

## Recovery model

1. Inject failures.
2. Verify bounded degradation.
3. Inject successful probes.
4. Verify capacity can recover.
5. Verify scheduler admission remains within global and transport limits.
6. Verify confidence remains bounded.

## Test

`phase1025_failure_injection_tests`

## Status

PASS when the deterministic failure-injection and recovery test completes without assertion failures.
