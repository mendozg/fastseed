# Phase 10.15 — Scheduler State Lifecycle & Decay

Phase 10.15 prevents persistent adaptive scheduler learning from remaining equally authoritative forever.

## Goals

- Decay historical evidence using a configurable half-life.
- Clear consecutive success/failure streaks when evidence becomes old.
- Gradually move learned transport capacity toward its configured neutral target.
- Preserve all existing minimum/target/maximum bounds.
- Keep unsupported outcomes excluded from capacity failure calculations.
- Make decay deterministic for tests by accepting an explicit time point.

## Configuration

`FeedbackConfig` now provides:

- `learning_half_life` — default 6 hours.
- `capacity_recovery_period` — default 24 hours.

## Behavior

For state older than one learning half-life, statistical counters are exponentially decayed. Capacity is independently relaxed toward the transport target according to the recovery period. Consecutive streaks are reset because they represent short-term conditions and should not survive indefinitely.

Decay never increases capacity beyond configured limits and never drops it below the configured minimum.

## Restart model

Phase 10.14 remains authoritative for persistence integrity. Phase 10.15 adds lifecycle management on top: restored state can be decayed before it becomes influential, preventing old network conditions from permanently shaping scheduler behavior.
