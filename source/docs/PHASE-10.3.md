# Phase 10.3 — ADDRv2 Crawler & Peer Model Integration

Phase 10.3 integrates ADDRv2 records into the FastSeed candidate model without treating non-IP networks as ordinary TCP/IP endpoints.

## Supported candidate networks

- IPv4 (0x01): normal TCP probing
- IPv6 (0x02): normal TCP probing
- Tor v3 (0x04): represented as a 32-byte binary endpoint and held in `AwaitingTransport`
- I2P (0x05): represented as a 32-byte binary endpoint and held in `AwaitingTransport`

## Safety boundary

Tor v3 and I2P candidates are **not** passed to the normal TCP probe runner. They are discovery records awaiting a future transport adapter. This prevents a `.onion` or I2P identifier from being accidentally resolved as ordinary DNS/IP data.

## Candidate identity

Candidate keys include the network type, host representation, and port. Tor v3 and I2P addresses are represented internally as `onion:<hex>` and `i2p:<hex>` respectively; the canonical binary bytes are retained in `address_bytes`.

## Persistence

SQLite network values now accommodate IPv4, IPv6, Tor v3, and I2P. Persistent candidates retain their network class and are restored as `Queued` only for IP networks; non-IP records restore as `AwaitingTransport` until a transport adapter exists.

## Scope

This phase does not implement Tor SOCKS or I2P SAM transport. It establishes the correct data model so those transports can be added later without redesigning crawler, health, or database layers.
