# Phase 10.6 — ADDRv2 Transport Adapters

Phase 10.6 adds an explicit transport layer for non-IP ADDRv2 candidates.

## Scope

- `DirectTcpTransport` for IPv4/IPv6.
- `TorSocks5Transport` using a local SOCKS5 proxy, no authentication.
- `I2pSamTransport` using the SAM STREAM protocol.
- Async Boost.Asio operations with bounded timeouts.
- Transport results return an established TCP socket for the next protocol layer.
- Tor v3 ADDRv2 32-byte public keys are converted to canonical v3 `.onion` hostnames.
- I2P ADDRv2 values remain opaque 32-byte identifiers. A destination resolver is required because the ADDRv2 value is not itself a SAM destination.

## Safety boundaries

- No Tor cryptography is implemented by FastSeed.
- No I2P cryptography is implemented by FastSeed.
- Tor v2 remains unsupported.
- Tor/I2P candidates are never sent to the normal TCP resolver.
- Tor/I2P candidates are never emitted as DNS A/AAAA records.
- Adapters are optional; normal IPv4/IPv6 operation has no Tor/I2P runtime dependency.

## Architecture

```text
PeerCandidate
   |
   +-- IPv4/IPv6 ----> DirectTcpTransport ----> TCP socket
   |
   +-- Tor v3 -------> TorSocks5Transport -----> SOCKS5 ----> TCP socket
   |
   +-- I2P ----------> I2pSamTransport --------> SAM STREAM -> TCP socket
```

The returned socket is deliberately separated from BSTY protocol logic. The existing protocol engine remains responsible for VERSION/SENDADDRV2/VERACK/ADDRv2 and consensus-independent peer validation.

## I2P destination resolution

BIP155 I2P records contain a 32-byte address identifier. SAM `STREAM CONNECT` requires an I2P destination string. FastSeed therefore requires a resolver/mapping layer before it attempts an I2P connection. This avoids inventing a destination from a hash that cannot be reversed.

## Tests

- Tor v3 hostname construction.
- Invalid/non-IP routing behavior.
- Mock SOCKS5 CONNECT handshake.
- Mock I2P SAM HELLO/SESSION/STREAM handshake.
- No public Tor/I2P network is required for tests.
