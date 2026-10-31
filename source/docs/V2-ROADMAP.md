# FastSeed 2.0 roadmap and Stage 14.1

Recovered from the original planning discussion:
https://chatgpt.com/share/6ae56179-19d0-83e8-b005-35c87254142b

The original design called 2.0 architecture Phase 13 and implementation Phase 14.
The later shipped v1.0 release reused Phase 13. Milestone labels below refer to
the 2.0 plan; they are not evidence that its proposed components already exist.

## Stage 14.1: implemented foundation

- Port is an explicitly constructed, checked endpoint port (1..65535). It is not
  used for listener configuration, where zero can legitimately request an ephemeral port.
- Sequence and UnixSeconds are distinct uint64 value types with comparison and
  explicit access. No unchecked increment or implicit arithmetic is provided.
  Timeouts continue to use monotonic clocks, not UnixSeconds.
- PeerId and SeederId are distinct 32-byte storage types. A stored ID is not a
  verified identity. Hash derivation/canonical address encoding is Stage 14.3;
  seeder key management and signatures are Stage 14.6.
- Core constants reference the existing BSTY constants. The coin protocol remains
  70016 with mainnet port 8226. Planned Mesh v2 is marked unavailable.
- A bounded error-code/message vocabulary is available to later modules. Its
  numeric values are internal and do not define a wire format.
- Version identity is 2.0.0-alpha1 (Development); existing CLI help/startup/version
  use the same identity. CMake uses numeric 2.0.0, with the prerelease suffix in
  release/VERSION. verify-version.py checks release metadata consistency.

These types are additive foundations. Existing peer storage and transport types
are integrated incrementally in later stages. Configuration and database schemas
remain at their existing versions. The legacy --self-test acknowledgement has
not become the comprehensive planned v2 self-test.

## Remaining implementation sequence

| Stage | Scope | Status |
| --- | --- | --- |
| 14.2 | Framing, reader/writer, codec, messages, known-byte vectors | Pending |
| 14.3 | Canonical peers and IDs, health/cache, selection | Pending |
| 14.4 | P2P session and scheduler integration | Pending |
| 14.5 | DNS integration with immutable memory snapshots | Pending |
| 14.6 | Seeder identities, advertisements, Mesh v2 and replay protection | Pending |
| 14.7 | Persistence v2, migration and bounded event queues | Pending |
| 14.8 | Isolated v1 compatibility adapter and config v2 | Pending |
| 14.9 | Recovery, watchdogs and metrics | Pending |
| 14.10 | Fuzzing, benchmarks, load tests and sanitizers | Pending |
| 14.11 | Security review, integration and RC1 | Pending |

Before 14.2, specify wire endianness, message sizes, canonical address encoding,
authentication boundaries and supported compatibility behavior against the actual
v1 federation code. Do not invent compatibility with messages v1 never implemented.
Retain existing IPv4/IPv6 and optional Tor/I2P discovery components. Overlay IDs
and address representation must be covered when the canonical model is designed.

DNS remains memory-only at request time. Remote advertisements are candidates
for local validation, not permission to bypass admission. Keep the independent
BSTY P2P protocol distinct from the FastSeed mesh protocol. Use a C++20 baseline
so Ubuntu 22.04/GCC11 remains in CI. Performance targets require measurements.

## Verification and release status

The new core contract test is registered with CTest; the complete suite now has
51 test entries. Test count is not a claim of a completed run. Both test gates
create a fresh private temporary directory per invocation, including sudo runs,
and clean it on exit. This avoids cross-user fixed-name SQLite fixtures.

Current milestone results are in release/V2-VERIFICATION.txt. Earlier operator
logs describe v1.0.0; they are not a test report for this alpha. GitHub must run
the six Ubuntu release/sanitizer jobs on this revision before it is called verified.

Use feature/v2-core-foundation for review. Push this branch for CI or open a PR;
keep production seeders on v1.0.0. New alpha tags create draft prereleases after
the gates pass. Reboot recovery and live Tor/I2P: **NOT TESTED**.
