# Phase 10.9 — Transport-Aware Crawler Scheduling

Phase 10.9 turns the persisted health information introduced in Phase 10.8 into an active scheduling input.

## Scheduling rules

- Only peers whose `next_probe` is due are selected.
- Quarantined peers are excluded until `quarantine_until` expires.
- Unknown networks are never scheduled for transport probing.
- Hot/warm direct peers receive higher refresh priority than cold/discovered peers.
- Tor v3 and I2P remain transport-isolated and are scheduled only as ADDRv2 candidates.
- Scheduler output is bounded by `max_batch`.
- Persistent peers are converted back into crawler candidates without losing network or raw ADDRv2 bytes.

## API

`PeerCrawler::schedule_from_health()` accepts persisted `PeerHealth` records and schedules only due peers.

## DNS boundary

The scheduler never produces DNS records. Existing DNS code continues to admit only IPv4/IPv6 HOT/WARM peers.
