# Phase 10.26 — Security Hardening & Adversarial Testing

## Objective
Harden FastSeed against malformed peer traffic, resource exhaustion, candidate flooding, and DNS rate-limit abuse while preserving bounded behavior.

## Changes
- bounded protocol host/source lengths
- parser oversized-message rejection before payload allocation
- canonical command-padding enforcement retained and tested
- ADDRv2 strict size/type validation retained and tested
- candidate admission rejects pathological host strings
- candidate capacity checked before ADDRv2 conversion/stat updates
- DNS rate limiter rejects overlong source identifiers
- DNS per-source rejection no longer consumes global quota
- adversarial regression test suite

## Security invariants
- no unbounded peer-controlled allocation in parser path
- candidate count never exceeds configured maximum
- ADDRv2 addresses remain <= 512 bytes
- Tor v2 is rejected
- DNS source identifiers are bounded
- per-source rate-limit failures do not exhaust global capacity

## Test
`phase1026_security_tests`

## Status
PASS when the deterministic adversarial security suite completes without assertion failures.
