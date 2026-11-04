# Alpha3: canonical peers, health and selection (Stage 14.3)

Alpha3 integrates a canonical endpoint model into the existing candidate and
health-cache paths. P2P remains BSTY protocol 70016, port 8226. Mesh v2, signed
seeder identities, session redesign and the database migration are later stages.

## Endpoint contract

An endpoint contains a network, its exact address bytes, and a checked port
(1..65535). Supported networks use the ADDRv2 numbers: IPv4=1 (4 bytes),
IPv6=2 (16 bytes), Tor v3=4 (32 bytes), I2P=5 (32 bytes).

IPv6 compression, case and brackets normalize to one endpoint. IPv4-mapped
IPv6 normalizes to IPv4. IPv4 requires four decimal octets without leading
zeros; abbreviated forms such as `127.1` are rejected consistently across OSes.
Scope IDs, unresolved DNS names, unsupported networks and invalid lengths are
rejected by the endpoint model. Existing bootstrap DNS resolution remains in
the crawler. A hostname must resolve to an IP endpoint before it can enter the
health cache. Identity parsing does not decide public-address admission.

Tor accepts the existing `onion:<64 hex digits>` representation and a 56-character
v3 `.onion` label with checked version/checksum. I2P accepts `i2p:<64 hex digits>`
and ordinary 52-character `.b32.i2p` labels with canonical padding bits.
Extended I2P names are not supported by this model. The overlay binary address
is authoritative when supplied; IP text and binary bytes must agree.

The encodings follow the [Tor v3 address specification](https://spec.torproject.org/rend-spec/encoding-onion-addresses.html)
and [I2P naming documentation](https://i2p.net/en/docs/overview/naming/).
These checks do not demonstrate live Tor/I2P connectivity.

## PeerId format 1

`PeerId` is an endpoint fingerprint, not proof of a remote node's identity or
ownership of a key. Different networks or ports produce distinct preimages.

Canonical bytes, in order:

| Field | Bytes |
| --- | --- |
| Internal endpoint format | `01` |
| BSTY mainnet magic | `a2 b2 e2 f2` |
| Network | 1 |
| Address length | 1 |
| Address | 4, 16 or 32 |
| Port | 2, big endian |

The ID is SHA256 of ASCII `BSTY-FastSeed/PeerId/v1`, a NUL byte, and those
canonical bytes. This is an internal contract, not a new P2P or Mesh message.
A future incompatible identity encoding needs a distinct format/domain.
The five independent Python fixtures cover all four networks and a different
port; the C++ suite compares both preimages and hashes.

## Cache and compatibility

The health cache indexes by PeerId. Legacy textual key lookups remain available,
including both old overlay health keys and candidate keys. IPv6 aliases and
IPv4-mapped aliases share observations. Rediscovery updates discovery metadata
without demoting a validated peer or clearing failures/quarantine. A local
probe result takes precedence over restored records while that cache entry is
retained. Restore picks the newest probe evidence for an alias without adding
duplicate counters; tied failure evidence is preferred.

The SQLite schema remains unchanged. Existing records normalize in memory;
old spelling aliases are not deleted or migrated on disk. Invalid endpoints
are excluded from the health cache. Back up the database before development
testing; do not share a running production database with an alpha process.

`[cache] max_peers = 100000` is optional in existing configuration files.
The parser accepts 1..1000000. It caps every state, including unprobed candidates,
cooldown and quarantine. Existing hot IPv4, hot IPv6, warm and cold limits still
apply; the hot quotas are independent. On overflow, retention favors hot, warm,
validated and cold records, then quarantine/cooldown, then unprobed records.
Fresh discovery cannot evict retained quarantine merely by being newer.
Within a class, recent success/observation wins, then PeerId breaks ties.
This is a count bound, not a claim of Sybil resistance or a measured memory budget.
Probe/stat counters saturate; latency history remains bounded.

## Selection

`SelectionPolicy` provides a result limit, hot/warm eligibility, freshness,
required service bits and explicit network choices. Quarantined, future-dated
and stale successes are excluded. Hot peers precede warm peers; newer success
precedes older success, then canonical host and port provide stable ordering.
An optional per-IP-group limit uses IPv4 /16 and IPv6 /32 prefixes. It is
disabled by default and is an API policy, not a new TOML setting.

The existing DNS snapshot path uses this selector with IPv4/IPv6 enabled and
Tor/I2P disabled. Overlay peers never become A/AAAA answers. This bridge does
not mark Stage 14.5's broader DNS redesign complete. Scheduler/session changes
remain Stage 14.4, and federation continues to use the existing v1 format.

## Verification

Three new CTest suites cover canonical identities, cache behavior and selection;
with servicefilter-r1 there are 58 registered suites. Both Linux gates check independent protocol
and peer fixtures. See `release/V2-VERIFICATION.txt` for actual executed results.
Historical v1/alpha2 logs do not verify alpha3. Full Ubuntu gates, reboot recovery
and live Tor/I2P remain unverified until separately exercised.
