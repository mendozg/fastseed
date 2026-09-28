# Phase 10.18 — Exploration/Exploitation Balancing

Phase 10.18 adds a bounded exploration budget to the confidence-aware scheduler.

## Goals

- Give low-confidence transports regular opportunities to recover.
- Prevent low-confidence candidates from crowding out established transports.
- Preserve adaptive per-transport capacity and the global concurrency ceiling.
- Keep the policy deterministic and configurable.

## Configuration

`AdaptiveSchedulerConfig` adds:

- `exploration_fraction` — fraction of available batch capacity reserved for exploration; default `0.20`.
- `minimum_exploration` — minimum exploration slots when exploration is enabled; default `1`.
- `maximum_exploration` — hard exploration-slot cap; default `32`.
- `low_confidence_threshold` — confidence below which a candidate is considered exploratory; default `0.50`.

Values are clamped by the scheduler. Exploration can never exceed available global capacity.

## Scheduling policy

1. Build the normal confidence-ranked candidate set.
2. Compute the bounded exploration budget.
3. Fill exploration slots from candidates below the confidence threshold.
4. Fill remaining capacity from established/high-confidence candidates.
5. If established work is exhausted, use otherwise-idle capacity for additional exploration.
6. Every admission remains subject to transport capacity and the global ceiling.

This produces controlled exploration without intentionally leaving the crawler idle.

## Persistence

No database schema change is required. Exploration policy is runtime configuration; confidence remains derived from the persisted scheduler evidence introduced by Phase 10.16.

## Validation

- `phase1018_exploration_tests` — PASS
- `phase1017_confidence_scheduler_tests` — PASS

The full project test suite is not claimed by this phase artifact.
