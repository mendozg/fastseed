# Phase 1.1 Protocol Test Vectors

Frozen invariants:

- Mainnet magic: `A2 B2 E2 F2`
- P2P port: `8226`
- Protocol version: `70016`
- Minimum peer protocol: `31800`
- Header: 24 bytes
- Payload checksum: first four bytes of SHA256(SHA256(payload))
- IPv4 network address: IPv4-mapped IPv6 (`::ffff:a.b.c.d`)
- Network port: 16-bit big-endian
- Maximum message size: 4 MiB
- Maximum user-agent size: 256 bytes

The test suite intentionally uses deterministic addresses and heights. Dynamic VERSION timestamp and nonce are not used as fixed-byte golden values yet; later protocol-vector work will add explicit deterministic constructors.
