Alpha3 servicefilter-r1 also fixes x<hex> wallet DNS bootstrap queries.
See docs/DNS-SERVICE-FILTER.md for the response contract and upgrade checks.

2.0.0-alpha3 implements Stage 14.3 canonical endpoints, PeerIds, health/cache
and policy-based selection. It includes the alpha2 protocol codec and interop fix.
Mesh v2, signed seeder identities, config v2 and database migration remain pending.

Required tag gates: Release and ASan/UBSan on Ubuntu 22.04, 24.04 and 26.04.
See the corresponding Actions run and release/V2-VERIFICATION.txt for actual
results; historical logs and test counts do not verify this revision.
Archives contain source only. Production deployment remains manual.

Reboot recovery: NOT TESTED. Live Tor/I2P connections: NOT TESTED.
Component/mock tests do not establish live overlay connectivity.

Before publishing this draft, review its Actions results, changes, attached
archives and SHA256SUMS.txt. Existing releases are never overwritten.
