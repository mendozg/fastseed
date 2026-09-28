# Phase 10.14 — Scheduler State Integrity & Multi-Instance Coordination

## Scope

Phase 10.14 hardens persistent adaptive scheduler state introduced by the 10.x scheduler work.

### Added

- Scheduler state schema version 1.
- SQLite schema migration to version 4.
- Atomic UPSERT persistence for per-transport scheduler state.
- Strict transport/state bounds at restore time.
- Seven-day maximum restore age.
- Future-dated state rejection.
- Explicit scheduler state freshness checks.
- Process-level database lock using an adjacent `.lock` file and `flock(LOCK_EX|LOCK_NB)`.
- Lock cleanup on normal close and failed startup.
- Restart/recovery and multi-instance collision tests.

## Safety model

Persisted state is learning data, not live runtime state. FastSeed never restores active probes, sockets, queue ownership, or in-flight capacity slots.

A persisted capacity is accepted only when:

1. the state version is supported;
2. the capacity is inside the transport's configured minimum/maximum;
3. the timestamp is not in the future;
4. the state is no older than seven days.

Invalid or stale scheduler state is ignored and the controller returns to its configured target.

## Database coordination

Only one FastSeed process may open a given peer database at a time. The lock is held for the lifetime of the database connection. SQLite WAL remains enabled for database durability, while the process lock prevents two independent FastSeed schedulers from racing over the same persistent state.

The lock file is mode `0600`; the operating-system advisory lock is released automatically when the process exits.

## Validation

Targeted regression suite:

- `phase108_db_transport_tests` — PASS
- `phase109_scheduler_tests` — PASS
- `phase1010_adaptive_scheduler_tests` — PASS
- `phase1011_feedback_tests` — PASS
- `phase1012_crawler_feedback_tests` — PASS
- `phase1014_integrity_tests` — PASS

Full project build was attempted but exceeded the available build time; the affected library and all six targeted test executables were subsequently built successfully.
