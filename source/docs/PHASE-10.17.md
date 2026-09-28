# Phase 10.17 — Confidence-Aware Scheduling

## Goal
Integrate the Phase 10.16 evidence-confidence model into adaptive peer scheduling without allowing confidence to bypass transport quotas or the global concurrency ceiling.

## Behavior
- Confidence is derived from effective, decayed observations.
- Lower-confidence transports receive a bounded exploration bonus of up to 15 scheduler priority points.
- Higher-confidence transports are allowed to remain in normal exploitation scheduling.
- Ties are resolved toward the lower-confidence transport, improving exploration without starvation.
- `AdaptiveCapacityController::admit()` remains authoritative for learned transport capacity.
- Static per-transport safety quotas remain authoritative.
- The controller's global capacity remains an absolute ceiling.
- No network, consensus, or blockchain behavior is changed.

## API
A new overload was added to `AdaptiveProbeScheduler::select_due()` accepting an `AdaptiveCapacityController` reference.

The existing API remains unchanged for callers that do not want confidence-aware scheduling.

## Safety invariants
1. A quarantined peer is never selected.
2. A peer whose effective due time is in the future is never selected.
3. Unknown networks are never selected.
4. Learned transport capacity cannot be exceeded.
5. Static transport safety quotas cannot be exceeded.
6. Global adaptive capacity cannot be exceeded.
7. Confidence affects ordering only; it cannot grant extra capacity.

## Validation
- `phase1014_integrity_tests` PASS
- `phase1015_decay_tests` PASS
- `phase1016_confidence_tests` PASS
- `phase1017_confidence_scheduler_tests` PASS

The full project rebuild exceeded the execution-time limit during validation; the changed library and affected test targets were incrementally built, and the Phase 10.17 behavior was directly exercised.
