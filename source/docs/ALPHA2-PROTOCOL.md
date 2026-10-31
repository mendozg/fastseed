# Stage 14.2: BSTY protocol codec (2.0.0-alpha2)

Alpha2 strengthens the existing BSTY P2P codec. It does not introduce a new
FastSeed mesh wire format. Protocol 70016 and mainnet port 8226 are retained.
The current socket probe benefits from shared header/serializer validation;
integration of the incremental FrameDecoder into connection sessions is Stage 14.4.

## Wire contract

| Field | Encoding / limit |
| --- | --- |
| Header | 24 bytes: 4 magic, 12 command, 4 length, 4 checksum |
| Mainnet magic | A2 B2 E2 F2 |
| Command | 1–12 printable ASCII bytes (20..7E hex), then zero padding |
| Payload length | uint32 little endian; at most 4 MiB |
| Checksum | First four bytes of SHA256(SHA256(payload)); OpenSSL implementation retained |
| Numeric payload fields | Little endian unless explicitly described below |
| Network ports | uint16 big endian |
| CompactSize | Shortest of 1, 3, 5, 9 bytes; nonminimal lengths rejected |
| Version | At most 1024 bytes; user agent at most 256 bytes |
| Address lists | At most 1000 entries; ADDRv2 address at most 512 bytes |

Legacy network addresses use 16-byte IP storage, with IPv4 mapped into IPv6.
ADDRv2 retains binary IPv4 (4 bytes), IPv6 (16), Tor v3 (32) and I2P (32)
representations. Existing Tor v2 rejection and bounded unknown-network handling
are retained. Canonical PeerId derivation and admission policy are Stage 14.3;
this release does not claim full conformance to every later BIP155 extension.

Unknown printable commands remain available to callers for forward compatibility.
Known payload APIs cover version, addr, addrv2, ping/pong and empty control payloads,
including wtxidrelay. Framing does not interpret an arbitrary command's payload.
Version decoding preserves an omitted relay flag, nonzero=true relay values and
bounded trailing extensions. Addr/addrv2 reject trailing data. Application version
advertisement now uses the same alpha2 identity as the CLI instead of `/BSTYFastSeed:0.1/`.

Reference formats were checked against [Bitcoin Core v25 message framing](https://github.com/bitcoin/bitcoin/blob/v25.0/src/protocol.h),
[CompactSize serialization](https://github.com/bitcoin/bitcoin/blob/v25.0/src/serialize.h)
and [BIP155 address fields](https://github.com/bitcoin/bips/blob/master/bip-0155.mediawiki).
BSTY-specific constants come from this repository, not Bitcoin's network constants.

## Bounded readers, writers and frame decoding

Reader supports explicit LE/BE fields, nonallocating byte views, canonical
CompactSize with a caller-provided maximum, and bounded variable strings.
Truncated fixed reads and failed CompactSize/varstring reads preserve their cursor.
Services bitfields may use all 64 bits; address counts and lengths pass smaller caps.
Read calls are sequenced explicitly, including the two bytes of network ports.

Writer defaults to the 4 MiB payload budget and can take a smaller budget.
Input/size validation precedes each composite field write. Self-appending a view
of its own data is supported. Allocation failure is not a transactional guarantee.
Existing `string()` retains its 65535-byte maximum; version encoding applies 256.

FrameDecoder consumes at most one message per call and returns `consumed` plus an
optional complete message. Feed the unconsumed suffix again. It retains one frame,
validates the full header before reserving payload space, and verifies the checksum
before delivering a message. The default cap is 4 MiB, with a smaller per-connection
cap available. It has no queue of decoded messages and is not thread-safe.

`finish()` accepts EOF only at a frame boundary. On a malformed frame or partial
EOF the decoder drops buffered data and latches failure. Further calls fail until
`reset()`; it never scans attacker input to resynchronize. Session deadlines,
connection counts and aggregate memory limits remain the caller's responsibility.

Use ProtocolError::code() for header, size, checksum, truncation, canonical-length,
trailing-data and nonce-payload failures. Some existing address validation APIs
retain their original standard exception types. These codes are local diagnostics,
not serialized error messages.

## Authentication and compatibility boundaries

Checksums detect corruption; they do not authenticate a node or authorize admission.
Peer and seeder ID types remain storage only. Federation v1 remains its existing
`BFS1`, version 1, shared-secret HMAC format. Alpha2 does not alter that codec,
negotiate Mesh v2, implement signed identities or reinterpret v1 federation frames.
Mesh identity, replay and authentication design belongs to Stage 14.6.

Configuration remains v1 and the existing database schema is retained. DNS,
crawler scheduling, transport selection and peer admission are not redesigned here.

## Regression fixtures and verification

`tests/protocol/generate_vectors.py` independently constructs nine fixtures using
Python struct and hashlib. C++ compares complete headers/payloads against those
bytes. Both Linux gates run the generator with `--check` before building.

The new suites cover field boundaries, canonical lengths, transactional read errors,
every two-piece frame split, deterministic chunked/coalesced delivery, partial EOF,
latched errors/reset, malformed commands, bad checksum/magic, size caps and existing
payload formats. This is deterministic regression testing, not a fuzzing campaign.

The complete CMake suite now defines 54 tests (51 existing plus three new codec
targets). Exact runs and platform limitations are recorded in release/V2-VERIFICATION.txt.
Native protocol tests do not establish a Linux application build or live interoperability.
Reboot recovery and live Tor/I2P connections remain **NOT TESTED**.
