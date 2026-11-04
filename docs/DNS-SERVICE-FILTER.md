# Wallet DNS service filtering (alpha3 servicefilter-r1)

GlobalBoost Core 25.x logs the base DNS seed name but, without a name proxy,
looks up `x9.<seed-zone>`. The mask is hexadecimal: 0x9 requests both NODE_NETWORK
(0x1) and NODE_WITNESS (0x8). Previously FastSeed returned NXDOMAIN for that
subdomain, even when an ordinary A query returned peers. The wallet can attempt
a peer-address-fetch fallback; its zero DNS-address count is not a count of all
available P2P connections. A name proxy uses a different bootstrap path.

Sources: [wallet DNS lookup](https://github.com/GlobalBoost/GlobalBoost-Y/blob/25.x/src/net.cpp#L1385-L1413),
[service selection](https://github.com/GlobalBoost/GlobalBoost-Y/blob/25.x/src/protocol.cpp#L121-L126),
[service flags](https://github.com/GlobalBoost/GlobalBoost-Y/blob/25.x/src/protocol.h#L264-L285).

## Response contract

`x<mask>.<zone>` accepts exactly one label containing x followed by 1–16 hex
digits. Matching is case insensitive and leading zeros are allowed within that
length. Zero selects the ordinary eligible peers; all 64 service bits are
supported. A peer qualifies when `(services & mask) == mask`. These are observed
peer-advertised capabilities, not an independent proof of every service.

Filtering also preserves health/freshness/quarantine and include_warm policy.
The snapshot holds all eligible IP peers from the bounded cache; service and
address-family selection precede max_answers. This removes the previous
max_answers-times-four snapshot cutoff that could hide qualifying peers.
Snapshots can therefore be larger than before, bounded by cache capacity and
existing health quotas. No per-query unbounded cache of service masks is created.

- A/AAAA: only matching IPv4/IPv6 peers; response owner is the queried name.
- No eligible matches: authoritative NOERROR, no answers, apex SOA in authority.
- Other types at a valid filtered name: NOERROR/SOA, including NS and SOA.
- Apex NS/SOA and ordinary apex A/AAAA: existing behavior retained.
- Malformed, overflowing or nested filter: NXDOMAIN/SOA.
- Outside the configured zone: REFUSED.
- UDP/EDNS limits, whole-RRset truncation and full TCP responses remain in effect.

No wildcard or static x9 record is needed at the parent DNS provider. The
existing delegation sends queries beneath the seed zone to FastSeed. A wildcard
copy of the apex would not enforce the service mask.

## Apply and verify an existing alpha3 installation

Apply ONE patch matching the source baseline (original alpha3 or alpha3 docs-r1),
or unpack the full source archive into a new directory. The patches include the
same final source and the earlier port-53 documentation. Do not apply both.

From the repository/distribution root, after applying the matching patch:

```bash
python3 source/scripts/verify-source.py
bash scripts/release-gate.sh && bash scripts/sanitizer-gate.sh
```

Run gates as the ordinary build user. Expect 58 CTest suites per gate; a test
count is not a pass claim. These gates do not install or restart the service.
After both pass, use the existing-installation upgrade and rollback procedure
in [DEPLOYMENT.md](DEPLOYMENT.md), starting in `source/`. Keep the existing config,
database, service unit and capability drop-in. Do not run the fresh-install
helper over a working installation. Replacing the executable requires a service
restart; no host reboot is required. Reboot recovery remains untested.

## Check the installed server

Use this server's configured zone (change it to seeder2/seeder3 if appropriate):

```bash
zone=seeder.globalboost.info
dig +norecurse @127.0.0.1 "$zone" A
dig +norecurse @127.0.0.1 "x9.$zone" A
dig +tcp +norecurse @127.0.0.1 "x9.$zone" A
dig +noedns +norecurse @127.0.0.1 "x9.$zone" A
dig +norecurse @127.0.0.1 "x9.$zone" AAAA
dig @8.8.8.8 "x9.$zone" A
```

With eligible peers advertising service bits 0x9, expect NOERROR and matching A
answers. Empty AAAA answers with NOERROR/SOA are valid if no eligible IPv6 peers
match. A valid filter must not return NXDOMAIN. Matching A answers need not equal
the apex count: some peers may not advertise all requested capabilities.

Public resolvers may retain the earlier NXDOMAIN until its negative-cache TTL
expires (60 seconds in the supplied local response). Verify directly against the
authoritative server first; the resolver's own cache can take longer to refresh.
Flushing the Windows cache does not flush an ISP/public resolver cache.

On Windows:

```powershell
nslookup -type=A x9.seeder.globalboost.info
nslookup -type=A x9.seeder.globalboost.info 8.8.8.8
```

After public filtered lookups work, restart the wallet normally if you want a new
startup attempt. An established wallet may skip DNS seeding because it already
has peers; do not delete peers.dat or wallet data just to change the log count.
For the supplied wallet trace, test each configured seed separately.

See `release/V2-VERIFICATION.txt`: native Windows component results are distinct
from full Ubuntu builds, wallet end-to-end discovery and public DNS deployment.
Reboot recovery and live Tor/I2P remain NOT TESTED.
