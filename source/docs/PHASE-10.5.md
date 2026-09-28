# Phase 10.5 — ADDRv2 Persistence & Health Integration

Phase 10.5 makes ADDRv2 network identity persistent and health-aware.

## Supported persisted networks

- IPv4 (`0x01`)
- IPv6 (`0x02`)
- Tor v3 (`0x04`)
- I2P (`0x05`)

The raw ADDRv2 address bytes are persisted separately from the display/identity host string. Persistent peers are restored as fresh candidates; no persisted state is treated as DNS-eligible without a new local validation cycle.

Tor v3 and I2P records restore as `AwaitingTransport` because Phase 10.5 does not implement Tor SOCKS or I2P SAM transport.

The DNS health snapshot only admits IPv4/IPv6 peers, preventing overlay-network identifiers from appearing in A/AAAA responses.

Schema version is 2. Existing Phase 4 databases are migrated by adding `address_bytes` and retaining prior peer records.
