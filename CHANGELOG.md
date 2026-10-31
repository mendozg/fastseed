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

# 2.0.0-alpha1 — unreleased

- Stage 14.1 core types, constants, error messages and development version identity.
- Add core contract tests and build/release metadata consistency checks.
- Each test gate creates a private temporary directory to isolate SQLite fixtures.
- Continue Ubuntu 22.04/24.04/26.04 release/sanitizer CI on the v2 feature branch.
- Later protocol, mesh, migration and resilience stages remain pending.

## Earlier changelog

# Changelog

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


## 1.0.0 â€” 2026-10-26

Final release consolidating the actual v1.0.0 RC1 EasyDeploy DNS Docs source and
all subsequent DNS, handshake and release-gate corrections. Intermediate v1.0.1
candidate labels are normalized to v1.0.0 final.

### DNS compatibility

- Accept and validate EDNS(0) OPT/additional records, bounded option TLVs and
  unknown options; reject malformed/duplicate OPT records and trailing data.
- Return response OPT, advertise the server UDP cap, preserve DO and negotiate
  BADVERS using the extended RCODE for unsupported EDNS versions.
- Serve authoritative apex NS/SOA and negative SOA for NODATA, empty cache and
  in-zone NXDOMAIN; refuse unrelated zones and unsupported classes.
- Preserve question case, bound compressed-name traversal and produce consistent
  FORMERR/NOTIMP/REFUSED responses. Drop QR and undersized packets.
- Enforce 512-byte non-EDNS UDP, the client/server EDNS limit, complete-RRset
  truncation and TC=0x0200. Keep TCP independent of UDP limits with correct framing.
- Add configurable nameserver, SOA mailbox and serial with backward-compatible defaults.

### Peer protocol and runtime correctness

- Accept empty sendaddrv2/wtxidrelay before an actual verack in both probe paths;
  verify magic/checksum, retain timeouts and bound pre-verack messages to 16.
- Keep all outgoing handshake/getaddr buffers alive through asynchronous writes.
- Bound version payload allocation, improve received-command errors and cancel
  outstanding connected-probe socket work on completion.
- Complete existing scheduler-state schema setup at version 4 during database open.
- Separate evidence-aging timestamps from capacity-adjustment timestamps; convert
  elapsed durations consistently to hours for half-life and recovery calculations.
- Protect federation replay-map access with a mutex while retaining replay rejection.

### Branding

- Restore the original FastSeed logo from the earlier release and display it in
  the source and archive-root READMEs, without altering the image.

### Compatibility, tests and deployment

- Include utility before Boost.Asio, explicitly link Threads and expose Boost include paths.
- Keep test assertions active in Release builds and supply relative config fixtures.
- Correct stale schema/version/config fixtures, callback expectations and capacity limits.
- Add 72 DNS checks, 26 direct/connected handshake scenarios, concurrent replay and
  evidence half-life regressions, and independent UDP/TCP wire/dig smoke tests.
- Normalize final version strings, documentation and release assertions to 1.0.0.
- Consolidate full release/sanitizer gates, preserve old script entry points, add
  source-integrity verification and document fresh install, safe upgrade and rollback.
- Fresh EasyDeploy accepts a nameserver argument and refuses an existing config.

### Verified evidence and outstanding limits

Operator-provided logs record 50/50 normal and 50/50 sanitizer suites, repeated
handshake/federation checks, public DNS and live peers, and an overnight run.
See release/VERIFIED-RESULTS.txt. Final-label packaging edits have not been rebuilt
locally. Reboot recovery was not tested. No public IPv6 listener or high-load
capacity claim is made. Optional Tor/I2P live-network operation was not established
by the submitted deployment evidence.
