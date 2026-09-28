# Phase 10.12 — Scheduler/Crawler Integration

Phase 10.12 connects the Phase 10.11 adaptive capacity controller directly to the live crawler completion path.

## Scope

- `PeerCrawler` owns an `AdaptiveCapacityController`.
- Probe completions are translated into `ProbeOutcome` values.
- Success, failure, timeout, handshake failure, and transport unsupported results feed back into capacity control.
- Probe latency is derived from `connect_time + handshake_time`.
- Scheduler admission checks the dynamic per-transport capacity before starting a probe.
- The global crawler concurrency and open-connection limits remain authoritative.
- Queue scanning skips temporarily saturated transports while preserving candidates for later scheduling.
- A public `transport_capacity()` accessor exposes current capacity for observability/tests.
- DNS A/AAAA behavior is unchanged.

## Transport mapping

| Candidate network | Transport |
|---|---|
| IPv4 | Direct TCP |
| IPv6 | Direct TCP |
| Tor v3 | Tor SOCKS5 |
| I2P | I2P SAM |

Unsupported ADDRv2 networks are not routed to a transport.

## Feedback mapping

| Probe result | Feedback |
|---|---|
| Success | Success |
| Timeout | Timeout/failure |
| HandshakeFailed | Handshake failure |
| TransportUnsupported | Unsupported, not a failure |
| Other transport/protocol errors | Failure |

## Safety

The dynamic capacity controller cannot exceed its configured per-transport maximums or the crawler's global concurrency/open-connection limits. Tor and I2P remain outside DNS A/AAAA output.

## Validation

Focused Phase 10.12 validation:

- `phase1010_adaptive_scheduler_tests` — PASS
- `phase1011_feedback_tests` — PASS
- `phase1012_crawler_feedback_tests` — PASS
- `phase107_transport_crawler_tests` — PASS
- `phase109_scheduler_tests` — PASS

A complete Phase 1–10.12 build/test/sanitizer run was not performed in this environment.
