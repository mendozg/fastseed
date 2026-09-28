# Phase 10.16 — Scheduler Confidence Model

## Objective
Distinguish the **quantity and freshness of scheduler evidence** from the **quality/outcome of that evidence**. Confidence is therefore an evidence-strength metric, not a success score.

## Model
For a transport, effective observations are the successful plus failed observations after exponential freshness decay using the configured learning half-life.

Confidence is bounded to `[0,1]`:

`confidence = 1 - exp(-effective_observations / 8)`

The model is deliberately outcome-neutral: eight successes and four successes/four failures produce approximately the same confidence when equally fresh. The success rate remains available separately through existing statistics and adaptive-capacity logic.

## Properties
- New transport: confidence `0`.
- More fresh observations increase confidence with diminishing returns.
- Old observations contribute less according to the existing half-life.
- Confidence never changes configured capacity bounds.
- Unsupported observations do not count as positive/negative probe evidence.
- Persisted state needs no new database column because confidence is derived from persisted counters and timestamp.

## API
`AdaptiveCapacityController::effective_observations()` returns the freshness-weighted evidence quantity.

`AdaptiveCapacityController::confidence()` returns normalized evidence strength.

## Validation
`phase1016_confidence_tests` verifies empty state, growth with observations, freshness decay, and outcome-neutral evidence strength.
