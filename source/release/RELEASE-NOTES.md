# BSTY-FastSeed v1.0.0 final source release

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

## Final-package provenance

The evidence carries the original intermediate 1.0.1 labels. Final preparation
normalizes version strings/assertions/CMake to 1.0.0 and consolidates scripts/docs.
Runtime functional implementation and headers are unchanged. Final source was not
rebuilt locally because Ubuntu execution is blocked; the included release gates
provide reproducible exact-package checks. No prebuilt binary is included.

Old failing candidate logs and interim status reports are not used as current
release results. The original operator evidence is retained unmodified under
evidence/ with hashes. See VERIFIED-RESULTS.txt for exact scope and limitations.

## Original branding restored

The original FastSeed PNG is included without alteration in docs/assets/ and
displayed in both READMEs. This packaging correction introduces no runtime,
configuration or test changes. Reboot recovery remains untested.
