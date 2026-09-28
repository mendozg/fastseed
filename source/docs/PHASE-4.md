# Phase 4 — Persistent Peer Database

Phase 4 adds a SQLite-backed persistence layer. It is deliberately separate from the DNS request path: DNS consumers read the Phase 3 in-memory cache only.

## Guarantees

- SQLite WAL mode, `synchronous=NORMAL`, foreign keys and bounded busy timeout.
- Peer records persist health, protocol metadata, source, timestamps, latency history and quarantine state.
- Startup loading produces fresh `PersistentDatabase` crawler candidates in `Queued` state. Persisted HOT/WARM state is never trusted as DNS eligibility.
- Writes support batched transactions and an asynchronous bounded persistence queue.
- Retention defaults to 30 days for successful peers and 14 days for failed-only peers.
- Integrity check, migration, vacuum, cleanup and statistics operations are exposed.
- The database is never consulted by the DNS response path.

## Schema

The `peers` table contains the peer identity and health fields required by the FastSeed architecture, including network, protocol version, services, user agent, lifecycle state, source, observations, latency statistics, quarantine information and error state.

## CLI integration

The database API is library-level in Phase 4. The production CLI surface is reserved for the service/operations integration pass so database commands cannot accidentally put disk I/O into DNS request handling.
