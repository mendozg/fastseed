# 2.0.0-alpha3 — development milestone, Stage 14.3

## Alpha3 servicefilter-r1 — 2026-11-04

Fix wallet DNS bootstrap queries such as `x9.seeder.globalboost.info`. GlobalBoost
Core 25.x requests peers by service mask; the previous DNS engine returned
NXDOMAIN for these names even when the base name returned healthy peers.

- Accept one `x<1..16 hex digits>` label immediately below the configured zone.
- Require every requested service bit; retain existing health/freshness eligibility.
- Select from all DNS-eligible cached peers before applying the answer limit.
- Keep A/AAAA, EDNS, UDP size negotiation/truncation and TCP behavior consistent.
- Return NOERROR/SOA when a valid filter has no matching address records; reject
  invalid/nested prefixes with NXDOMAIN and outside-zone queries with REFUSED.
- Keep NS/SOA answers at the zone apex; filtered names have address records only.
- Add `dns_service_filter_tests` (58 registered CTest suites in this revision).

This is an alpha3 hotfix artifact revision. The binary version remains
`2.0.0-alpha3`; use the archive/source hashes to identify this exact revision.
No configuration, DNS delegation, database schema or peer-handshake changes.
See [DNS filtering and upgrade checks](../docs/DNS-SERVICE-FILTER.md) and
`release/V2-VERIFICATION.txt` for executed tests and limitations.
Reboot recovery and live Tor/I2P remain **NOT TESTED**.

## Earlier alpha3 work

Documentation revision **docs-r1** adds Ubuntu port-53 diagnosis, existing
FastSeed service handling, systemd-resolved stub configuration, DNS verification
and rollback instructions. Runtime code and prior test evidence are unchanged.

- Canonical endpoint/PeerId model for IPv4, IPv6, Tor v3 and ordinary I2P base32.
- Normalize IP aliases, validate Tor checksum/version and I2P padding bits.
- Key the health cache by endpoint ID; bridge existing textual lookup keys.
- Preserve local health/quarantine on rediscovery and prefer current local probes
  over restored aliases. Keep the existing SQLite schema.
- Bound all cache states with optional `cache.max_peers` (default 100000), retain
  per-state limits and enforce independent IPv4/IPv6 hot quotas.
- Add service/freshness/network selection, optional IP prefix diversity and stable
  ordering; DNS excludes overlay addresses. Saturate counters and bound history.
- Canonicalize crawler insertion/deduplication and count only inserted addresses.
- Add three peer suites (57 total), five independent ID vectors and two overlay
  hostname vectors. Keep the alpha2 interop version repair.
- Fix two test helper naming conflicts and the existing Windows logger time call; use narrow SQLite fixture paths.
- See release/V2-VERIFICATION.txt for executed results and pending Ubuntu gates.
  Reboot recovery and live Tor/I2P remain NOT TESTED.

## 2.0.0-alpha2 interopfix-r1

Correct the interoperability test's stale `/BSTYFastSeed:0.1/` expectation.
It now checks the current shared version. Runtime code is unchanged.
The exact test passed 20 runs each in native Debug, Release and AddressSanitizer.
Full Ubuntu release/sanitizer gates still require confirmation for this revision.

# 2.0.0-alpha2 — unreleased

- Stage 14.2: bounded readers/writers, canonical CompactSize and explicit port byte order.
- Share strict command/header checks across direct header parsing and complete frames.
- Add a bounded incremental frame decoder with partial/coalesced input and latched errors.
- Reuse one CompactSize implementation in version, addr and addrv2; retain existing version extensions.
- Expose wtxidrelay in the payload codec and advertise the current FastSeed version.
- Add nine independently generated wire fixtures and three CTest targets (54 total).
- Preserve existing config/database schemas and v1 federation; Mesh v2 is still pending.

Current evidence: release/V2-VERIFICATION.txt. Full Ubuntu gates are required for this revision.
Reboot recovery and live Tor/I2P: NOT TESTED.

## Earlier milestone notes

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
