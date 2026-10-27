# Build the final executable

The obsolete prebuilt binary from the earlier release was removed: it does not
contain the verified source fixes. No replacement prebuilt binary is claimed.

From the repository root run `bash scripts/release-gate.sh`. The current executable
will be `source/build-release/bsty-fastseed`. Do not install sanitizer binaries.
See [deployment instructions](../docs/DEPLOYMENT.md).
