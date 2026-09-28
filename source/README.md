# BSTY FastSeed — Phase 10.1

FastSeed is a standalone high-performance peer discovery and DNS seeder for GlobalBoost-Y (BSTY).

Phase 10.1 adds `sendaddrv2` capability signaling to the BSTY P2P probe path as part of the FastSeed v1.0.0 ADDRv2 release plan.

## Commands

```bash
bsty-fastseed --version
bsty-fastseed --help
bsty-fastseed --self-test
bsty-fastseed --check-config /etc/bsty-fastseed/fastseed.toml
bsty-fastseed probe <host>[:port]
```

## Protocol

- BSTY mainnet P2P: 8226
- BSTY magic: `A2 B2 E2 F2`
- Protocol version: 70016
- `sendaddrv2` is sent after a valid peer `version` and before `verack`.

Full BIP155 ADDRv2 network support is implemented in subsequent Phase 10.x work; Phase 10.1 establishes the negotiation mechanism without adding Tor/I2P transport dependencies.

## Phase 10.7

Phase 10.7 connects the Phase 10.6 transport layer to the peer crawler. The crawler now routes candidates by ADDRv2 network type and hands established transport sockets to the normal BSTY protocol probe. Direct IPv4/IPv6 probing remains enabled by default; Tor v3 and I2P transports are optional. Unsupported optional transports remain `AwaitingTransport` and are never exposed through DNS A/AAAA records.

Phase 10.17: confidence-aware scheduling is implemented; confidence influences ordering only and never bypasses concurrency limits.

## Phase 10.18

Phase 10.18 adds confidence-aware exploration budgeting. Low-confidence transports receive a bounded portion of each scheduling batch, while established transports receive the remaining capacity first. If no established work is available, otherwise-idle capacity may still be used for exploration.
