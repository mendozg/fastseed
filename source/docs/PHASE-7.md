# BSTY-FastSeed Phase 7 — Distributed Seeder Mesh

Phase 7 establishes a small, authenticated federation protocol between independent FastSeed instances. It is deliberately separate from BSTY P2P protocol 70016 and from blockchain/consensus code.

## Scope
- Seeder identity via configured `seeder_id`.
- Authenticated federation frames using HMAC-SHA256 over a shared federation secret.
- HELLO, PEER_SYNC, PEER_WITHDRAW and ACK-compatible message types (ACK reserved for the next implementation stage).
- Monotonic per-seeder sequence numbers for replay protection.
- Timestamp freshness window.
- Bounded peer/withdrawal counts and frame size.
- Duplicate/stale/replayed messages rejected.
- Self-originated messages rejected.
- Peer records retain provenance (`source_seeder`) and observation time.

## Trust boundary
Federated records are observations, not proof that a BSTY node is currently reachable. Imported records must still enter the local crawler and Phase 3 health pipeline before becoming DNS eligible.

## Security boundary
The shared secret authenticates the federation transport but does not provide public identity or non-repudiation. Phase 7 intentionally leaves certificate/public-key identity for a later federation security phase.

## Wire format
Magic `BFS1`, version, type, flags/reserved, sequence, Unix timestamp, sender id, bounded peer records, bounded withdrawals, and a 32-byte HMAC-SHA256 tag.

## Failure isolation
Each seeder keeps its own crawler, health cache, database, and DNS snapshot. A federation failure cannot directly block the DNS request path.
