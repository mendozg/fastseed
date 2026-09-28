# Phase 10.4 — ADDRv2 Peer Exchange

Phase 10.4 makes ADDRv2 an active peer-discovery mechanism. After a successful BSTY VERSION/VERACK handshake, FastSeed sends `getaddr`, accepts a bounded post-handshake discovery window, decodes `addrv2`, and exposes discovered records through `ProbeResult`. The crawler automatically converts those records into network-aware candidates.

## Behavior

```text
VERSION -> SENDADDRV2 -> VERACK
                         │
                         ▼
                       GETADDR
                         │
                         ▼
                      ADDRv2
                         │
          ┌──────────────┼──────────────┐
          ▼              ▼              ▼
        IPv4           Tor v3          I2P
          │              │              │
       TCP queue    awaiting transport  awaiting transport
```

The discovery window is bounded. EOF or connection reset after a successful handshake is not treated as a peer failure. Malformed ADDRv2 frames are protocol failures.

`ProbeResult::discovered_addrv2` contains decoded records. The crawler feeds them back as `PeerExchange` candidates; IPv4/IPv6 are queued for TCP probing, while Tor v3/I2P remain `AwaitingTransport` until a transport adapter is implemented.
