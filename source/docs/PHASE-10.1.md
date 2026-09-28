# Phase 10.1 — SENDADDRV2 Capability Signaling

Phase 10.1 promotes ADDRv2 negotiation into the FastSeed v1.0.0 release path.

## Scope

- Add the BSTY P2P `sendaddrv2` command to the protocol command registry.
- Encode the command as an empty-payload message.
- Have `PeerProbe` send `sendaddrv2` after receiving a valid peer `version` and before sending `verack`.
- Keep the existing ADDRv2 record codec unchanged in this phase; full BIP155 network IDs beyond IPv4/IPv6 are Phase 10.2+.
- Add deterministic wire-vector coverage and a local TCP interoperability test of the actual probe.

## Wire format

`sendaddrv2` has an empty payload. Its BSTY message therefore consists of the normal 24-byte message header with command `sendaddrv2`, payload length zero, and the SHA256d empty-payload checksum.

## Handshake

```text
VERSION -> VERSION
             |
             v
        SENDADDRV2
             |
             v
          VERACK
             |
             v
        VERACK
             |
             v
        ESTABLISHED
```

## Compatibility

FastSeed continues to use BSTY protocol version 70016 and P2P port 8226. No consensus, blockchain synchronization, or transport-specific Tor/I2P code is introduced here.
