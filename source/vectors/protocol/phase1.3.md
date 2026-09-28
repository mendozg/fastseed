# BSTY FastSeed Phase 1.3 Protocol Vectors

## Canonical empty VERACK frame

```text
a2 b2 e2 f2
76 65 72 61 63 6b 00 00 00 00 00 00
00 00 00 00
5d f6 e0 e2
```

- Magic: `a2b2e2f2`
- Command: `verack`
- Payload length: `0`
- Checksum: `5df6e0e2`

## Canonical PING payload

Nonce:

```text
1122334455667788
```

The payload is serialized little-endian as:

```text
88 77 66 55 44 33 22 11
```
