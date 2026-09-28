# Phase 5 — DNS Response Engine

Phase 5 adds the authoritative, memory-only DNS response layer for BSTY FastSeed.

## Design

The DNS path consumes only an immutable `DnsSnapshot` published from the Phase 3 health cache. It never reads SQLite, performs peer probes, resolves names, or touches the crawler.

Supported query types are `A` and `AAAA` for the configured seed zone. Responses use a configurable TTL and bounded answer count. Queries for other classes/types are refused or return no data as appropriate.

The UDP server is asynchronous and uses Boost.Asio. It does not perform blocking work in the DNS request handler.

## Safety properties

- Only Phase 3 DNS-eligible HOT/WARM peers can enter a snapshot.
- Persistent database records cannot directly become DNS answers.
- Snapshot publication is atomic through `std::shared_ptr`.
- DNS requests are bounded to 4096 bytes.
- DNS labels are bounded to the DNS maximum of 63 bytes.
- No recursive DNS resolution is performed.
- No blockchain/consensus code is linked into the DNS engine.

## Configuration

```toml
[dns]
zone = "seed.globalboost.info"
listen_address = "0.0.0.0"
listen_port = 53
ttl_seconds = 60
max_answers = 32
include_warm = true
refresh_interval_seconds = 60
```

## Acceptance

- A/AAAA answer generation
- IPv4/IPv6 separation
- authoritative response flags
- bounded answer count
- immutable snapshot publication
- no database access on DNS path
- malformed query handling
- unsupported query handling
- deterministic unit tests
