Historical v1.0.0 record — not alpha1 verification.
Current milestone: release/V2-VERIFICATION.txt and docs/V2-ROADMAP.md.

# Final release checklist

- [x] Consolidate RC1 and all DNS, handshake and release-gate fixes.
- [x] Review operator log: 50/50 normal CTest suites pass.
- [x] Review operator log: federation and handshake suites each repeat 20 times.
- [x] Review operator log: independent DNS smoke checks pass.
- [x] Review operator log: 50/50 ASan/UBSan suites pass.
- [x] Review live peer/public DNS/delegation/recursive-resolution evidence.
- [x] Service restart observed and automatic startup configured.
- [x] Review 144 overnight samples; all DNS replies succeed, memory bounded.
- [x] Normalize source version/help/startup strings and release assertions to 1.0.0.
- [x] Preserve operator evidence and record its provenance/limits.
- [x] Static parsing of final Bash/Python scripts.
- [x] ZIP/tar.gz extraction, byte checks and source-integrity checks (see package report).
- [ ] Recompile/retest exact final-labeled package on the target host.
- [ ] Test final packaging helper install/upgrade/rollback on a staging host.
- [ ] Full-server reboot recovery — NOT TESTED (operator cannot reboot).

Checked operator items describe supplied evidence, not executions on the packaging
host. Current runtime logic matches the tested corrective source; only version
strings change. No reboot or exact-final-binary test result is claimed.
