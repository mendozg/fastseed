# Phase 1.2 Protocol Conformance Vectors

Frozen BSTY mainnet wire facts used by FastSeed:

- Magic: `A2 B2 E2 F2`
- P2P port: `8226`
- Protocol: `70016`
- Minimum peer protocol: `31800`
- Header: 24 bytes
- Checksum: first four bytes of SHA256d(payload)
- ADDR port: big-endian uint16
- ADDRv2 port: big-endian uint16
- IPv4 ADDRv2 network id: `1`
- IPv6 ADDRv2 network id: `2`
- CompactSize vectors are bounded before allocation

Phase 1.2 adds deterministic round-trip coverage for VERSION/VERACK, ADDR,
ADDRv2, GETADDR, PING and PONG. Real `globalboostd` interoperability remains
an environment-dependent integration gate and is never represented as passed
unless a live node is actually contacted.
