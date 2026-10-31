# BSTY-FastSeed 2.0.0-alpha1 — core foundation

Development milestone Stage 14.1, based on the tested v1.0.0 runtime and CI.
Adds strongly typed endpoint ports, sequence/timestamp values, peer/seeder ID
storage, shared BSTY constants, an internal error vocabulary and central version
identity. The existing DNS/P2P runtime is retained. Mesh v2 negotiation, signed
identities, the canonical peer codec and configuration v2 are future milestones.

Use a development branch and an isolated test deployment. Production seeders
should remain on the released v1.0.0 build. Current configuration stays v1;
do not add a config version 2 setting yet. BSTY P2P remains protocol 70016, port 8226.
The legacy --self-test command is only an acknowledgement; run the test gates.

Reboot recovery and live Tor/I2P connections: **NOT TESTED**.

See docs/V2-ROADMAP.md for milestone scope and verification.

## Historical v1.0.0 documentation

## Boost compatibility update — 2026-10-30

Four timer cancellation calls in probe, crawler and transport now use the
supported zero-argument cancel() API with system_error handling during cleanup.
Socket cancellation calls are unchanged. This addresses the Boost 1.90 build failure.

Operator-provided Ubuntu 26.04.1 LTS results (GCC 15.2.0 / Boost 1.90.0):
- Release CTest: 50/50 passed, 1.94 seconds.
- Federation/handshake repeated regression checks: passed, 24.56 seconds.
- Application smoke and full release gate: PASS.
- ASan/UBSan CTest: 50/50 passed, 13.14 seconds; full sanitizer gate: PASS.
- Nonfatal shared_ptr atomic deprecation and federation indentation warnings remain.

Earlier Ubuntu 22.04/GCC11 results cover the preceding runtime revision.
The timer-adjusted revision has not been retested on Ubuntu 22.04 here.
Ubuntu 24.04 full-suite validation remains pending.
Reboot recovery: NOT TESTED. Live Tor/I2P connections: NOT TESTED.
Transport component/mock tests do not establish live overlay connectivity.

Tests were run by the operator, not the packaging agent. The logs report version
1.0.0. The operator supplied no source/binary hash, so these are evidence for the
described changes, not a cryptographic attestation of this archive. Packaging
checks verify archive integrity; no new C++ build or deployment ran locally.

HISTORICAL PRE-BOOST OVERVIEW (superseded where noted above)

<p align="center">
  <img src="docs/assets/fastseed-logo.png" alt="BSTY FastSeed — Faster Peers, Stronger Network" width="520">
</p>

# BSTY-FastSeed v1.0.0

GitHub Actions configuration is included for release and sanitizer gates on
Ubuntu 22.04, 24.04 and 26.04, plus source packaging and draft releases on tags.
See [CI and release instructions](docs/CI.md). Workflow execution is pending its
first GitHub run; adding configuration does not establish passed CI results.

Standalone GlobalBoost-Y peer discovery and authoritative DNS seeder. The final
release consolidates DNS/EDNS, modern peer-handshake, persistence, feedback-aging
and federation fixes. The original logo is retained unchanged.

## Build and verify

The canonical source and release evidence live in **source/**. No current prebuilt
binary is shipped: build the final source for the destination Ubuntu host.

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libboost-dev libssl-dev libsqlite3-dev python3 dnsutils
python3 source/scripts/verify-source.py
bash scripts/release-gate.sh
bash scripts/sanitizer-gate.sh
```

Release binary: `source/build-release/bsty-fastseed`.
Expected version: `BSTY-FastSeed 1.0.0 (Production)`.
The gates do not install or restart a service. Test assertions stay active in Release.

## Deploy and upgrade

Read [deployment instructions](docs/DEPLOYMENT.md). Fresh-install example:

```bash
sudo bash deploy-fastseed.sh seeder.globalboost.info nsseed.globalboost.info
```

The fresh helper refuses an existing configuration or occupied DNS port. For a
working installation, use the documented manual upgrade/rollback procedure to
preserve config, database and service capabilities. There is no need to reboot
the server to install this release.

Parent DNS must delegate the configured seed zone to a stable nameserver whose
address points to the DNS host. Do not use dynamic peer-answer IPs as nameserver
identity. See the deployment document for the `nsseed.globalboost.info` example.

## Verification status

Operator evidence records 50/50 normal suites, 50/50 ASan/UBSan suites, 20 repeats
each of federation/handshake tests, independent DNS checks, successful public
UDP/TCP/recursive DNS and live BSTY handshakes. The overnight log has 144 successful
samples with bounded MemoryCurrent (2.06–2.23 MiB) and no recorded automatic restarts.

**Reboot recovery was not tested.** Final version-label, script and documentation
normalization occurred after the supplied runtime test runs; an exact-final-label
C++ rebuild was not run on the packaging host. Run the included gates on the
host that will build your release binary. The source-only package does not claim
that the removed older prebuilt binary contains these fixes.

- [Detailed README](source/README.md)
- [Changelog](CHANGELOG.md)
- [Configuration](docs/CONFIGURATION.md)
- [Verified results and limitations](source/release/VERIFIED-RESULTS.txt)
- [Original evidence](source/release/evidence/)
- [Release notes](source/release/RELEASE-NOTES.md)

Root config/doc copies are distribution conveniences. `source/` is the canonical
build tree; its own source hash manifest verifies the packaged release bytes.
`RELEASE-CHECKSUMS.txt` covers repository distribution files (excluding itself).
