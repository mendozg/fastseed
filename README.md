<p align="center">
  <img src="docs/assets/fastseed-logo.png" alt="BSTY FastSeed — Faster Peers, Stronger Network" width="520">
</p>

# BSTY-FastSeed v1.0.0

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
