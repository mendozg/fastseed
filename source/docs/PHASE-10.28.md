# Phase 10.28 — Multi-Seeder Federation Test

Adds deterministic multi-seeder federation state handling and validation.

## Guarantees
- authenticated federation messages
- replay protection remains authoritative
- duplicate messages do not mutate state
- deterministic newest-observation conflict resolution
- deterministic source-seeder tie break
- bounded federation snapshots
- simple rebroadcast-loop suppression through source exclusion
- withdrawals are explicit and scoped to valid federation messages
- malformed/truncated/authentication-failed messages are rejected
- each seeder keeps an independent local store

## Scope
This phase validates federation intelligence exchange. It does not make federation data authoritative for consensus or bypass peer health validation.
