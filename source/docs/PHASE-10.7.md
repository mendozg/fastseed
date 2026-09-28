# Phase 10.7 — Transport-Aware Peer Crawler Integration

Phase 10.7 integrates the Phase 10.6 transport adapters with the peer crawler.

## Scope

- Candidate-aware transport routing.
- IPv4/IPv6 candidates use direct TCP.
- Tor v3 candidates use the optional SOCKS5 adapter when enabled.
- I2P candidates use the optional SAM adapter when enabled.
- Unsupported Tor/I2P transports remain `AwaitingTransport` rather than entering DNS or being counted as failed peers.
- Connected transport sockets are handed to the normal BSTY `PeerProbe` handshake.
- ADDRv2 discovery remains part of successful probes.
- Existing cooldown, health and persistence semantics are preserved.

## Transport configuration

`CrawlerConfig` adds:

- `enable_tor_transport`
- `enable_i2p_transport`
- `tor_transport`
- `i2p_transport`
- `i2p_destination_resolver`

Direct IPv4/IPv6 probing is enabled by default. Tor and I2P are opt-in.

## Important boundary

Transport adapters establish the network path only. BSTY protocol validation remains in `PeerProbe`. FastSeed still contains no consensus, blockchain synchronization, transaction validation, or mining logic.

## Validation

Phase 10.7 adds:

- a local direct-TCP crawler integration test using the real transport router and connected-socket BSTY handshake;
- a transport-router test proving Tor v3 and I2P candidates are routed to their dedicated adapters and unsupported transport is surfaced as `TransportUnsupported`.
