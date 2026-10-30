# BSTY-FastSeed v1.0.0 final source release

## Boost compatibility update â€” 2026-10-30

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


This release combines the original RC1 EasyDeploy code with the validated DNS,
peer-handshake, database, feedback-aging and federation concurrency fixes.
See CHANGELOG.md for the full list and docs/DEPLOYMENT.md for installation,
configuration preservation, delegation and rollback.

## Verified results

Operator-provided evidence shows 50/50 normal and 50/50 sanitizer CTest suites,
20 repeats each of the federation/handshake tests, independent DNS smoke checks,
live BSTY handshakes and public UDP/TCP/recursive DNS serving. The 144-sample
observation recorded no DNS failures or automatic restarts, with MemoryCurrent
between 2.06 and 2.23 MiB over 11h55m05s. These are sampled observations, not
proof of uninterrupted availability or absence of all memory defects.

Reboot recovery was not tested. The service restart passed and startup is enabled;
that does not substitute for an actual host reboot test.

## Historical pre-Boost package provenance (2026-10-26)

The evidence carries the original intermediate 1.0.1 labels. Final preparation
normalizes version strings/assertions/CMake to 1.0.0 and consolidates scripts/docs.
Runtime functional implementation and headers are unchanged. Final source was not
rebuilt locally because Ubuntu execution is blocked; the included release gates
provide reproducible exact-package checks. No prebuilt binary is included.

Old failing candidate logs and interim status reports are not used as current
release results. The original operator evidence is retained unmodified under
evidence/ with hashes. See VERIFIED-RESULTS.txt for exact scope and limitations.

## Historical branding restoration (2026-10-26)

The original FastSeed PNG is included without alteration in docs/assets/ and
displayed in both READMEs. This packaging correction introduces no runtime,
configuration or test changes. Reboot recovery remains untested.
