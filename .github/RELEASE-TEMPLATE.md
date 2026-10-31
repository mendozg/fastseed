2.0.0-alpha2 implements Stage 14.2 protocol codecs and regression vectors.
Mesh v2, signed identities, config v2 and persistence migration remain pending.

Source release built from the tagged commit after release and ASan/UBSan gates
passed on Ubuntu 22.04, 24.04 and 26.04. See this tag's Actions run for exact
compiler/library versions and verification logs. Archives contain source only.

Reboot recovery: NOT TESTED.
Live Tor/I2P connections: NOT TESTED.
CI component/mock tests do not establish live overlay connectivity.
Production deployment remains manual.

Before publishing this draft, add the version-specific changes and review the
attached archives and SHA256SUMS.txt. Existing releases are never overwritten.
