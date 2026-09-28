# Phase 6 — Production Hardening & Distributed Seeder Foundations

Phase 6 hardens the Phase 5 DNS path for production operation without adding blockchain or consensus dependencies.

## Goals

- Bound DNS response size to avoid oversized UDP responses and fragmentation.
- Add per-source and global DNS query rate limiting with bounded memory.
- Keep DNS request handling memory-only and non-blocking.
- Preserve independent FastSeed instances so multiple seeders can operate without sharing mutable runtime state.
- Make failure of one seeder independent of other seeders.

## DNS safeguards

The response engine defaults to a 1232-byte response budget. If an answer set would exceed the budget, the response is truncated with the DNS TC bit instead of emitting an oversized UDP datagram.

The UDP server applies a bounded token-window limiter:

```toml
[dns.rate_limit]
per_source_per_second = 100
global_per_second = 5000
max_sources = 4096
```

Rate limiting is applied before response generation. Rejected requests are dropped without DNS, database, crawler, or network I/O.

## Distributed deployment model

FastSeed is designed to run as independent instances:

```text
                    BSTY network
                         |
          +--------------+--------------+
          |              |              |
      Seeder A        Seeder B       Seeder C
       crawler         crawler         crawler
       health          health          health
       SQLite          SQLite          SQLite
          |              |              |
          +------ independent DNS -------+
```

Instances do not require a shared SQLite database. This prevents a single persistence failure from becoming a network-wide DNS failure.

## Operational requirements

- Run each seeder under a dedicated service account.
- Keep the SQLite file private to that seeder (`0600`).
- Expose DNS only on intended interfaces.
- Use separate IPv4/IPv6 service instances where required by deployment.
- Monitor process liveness, DNS response rate, query drops, crawler success rate, and HOT/WARM peer counts.
- Do not put database or peer-probe operations in the DNS request callback.
- Use multiple independent public seeders before replacing existing BSTY DNS seeds.

## Phase 6 acceptance

- Existing Phase 1–5 tests remain green.
- Rate limiter is bounded and deterministic.
- DNS responses remain within the configured UDP budget.
- Oversized answer sets are truncated safely.
- DNS remains independent of SQLite and crawler operations.
- Sanitizer builds remain clean.
