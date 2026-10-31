# BSTY-FastSeed 2.0.0-alpha1 — development milestone

Stage 14.1 starts the 2.0 roadmap from v1.0.0 commit
7b027ae6acb120532c5342a17d7b8a52c9d993a7, retaining the existing runtime.

- Checked endpoint ports, distinct sequence/timestamp values, peer/seeder ID storage.
- BSTY protocol constants and internal error messages for subsequent modules.
- Shared development version identity; metadata checks before test gates/packaging.
- Core contract test added to CTest (51 registered tests total).
- Private temporary directories for each test gate to isolate SQLite fixtures.
- CI covers the v2 feature branch; alpha tags produce draft prereleases.

Mesh v2, signed identities, config v2 and database migration remain pending.
No production upgrade is recommended for this alpha. Read docs/V2-ROADMAP.md
and release/V2-VERIFICATION.txt for exact scope and verification limitations.
Old operator logs are v1.0.0 evidence and do not establish alpha compatibility.
Reboot recovery and live Tor/I2P: **NOT TESTED**.
