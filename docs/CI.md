# Continuous integration and release packaging

Pushes to main, pull requests and manual Actions runs build/test the canonical
source tree on Ubuntu 22.04, 24.04 and 26.04. Each OS runs the full release gate
and a separate ASan/UBSan gate. Every job uses a private temporary directory to
avoid fixed-name SQLite fixtures left by another user or run. Compiler/library
versions, gate logs and CTest diagnostics are saved as Actions artifacts.

Actions are pinned to exact commits. Test jobs have read-only repository access;
only the draft-release job can write release assets. No server credentials,
production installation or automatic service restart is used.

## Enable and use

Push this workflow to main. In GitHub, open Actions, enable workflows if prompted,
and select **Build, test and package → Run workflow** to run it immediately.
The first run must pass before claiming CI verification. Consider requiring the
six OS/gate checks in branch protection after their names appear on a run.

Manual runs also provide downloadable source archives and hashes. Packaging uses
tracked files from the tested commit only, excluding untracked build files. Both
archive formats are compared byte-for-byte against Git contents before upload.

## Release a new version

Update runtime/CMake/version assertions, source/release/VERSION, docs and integrity
manifests together. Commit those changes, then create and push the matching tag
(for example v1.0.1 when the source version is 1.0.1). All six jobs must pass.
The workflow packages source ZIP/tar.gz plus SHA256SUMS and creates a **draft**
release. Review the draft, replace its template with version-specific changes,
and publish manually. Existing releases are not overwritten: recreating an
existing release tag fails at draft creation. Do not delete/move v1.0.0 to trigger
this workflow; use the manual run to package the current version instead.

Reboot recovery and live Tor/I2P connections remain **NOT TESTED**. Component/mock
tests cannot verify live overlay connectivity. Production deployment is manual.
