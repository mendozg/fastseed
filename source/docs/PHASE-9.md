# Phase 9 — Release Engineering & Production Deployment

Phase 9 adds release-time configuration validation, explicit version/help/self-test CLI contracts, systemd deployment assets, hardened service defaults, and a production configuration baseline.

## Safety boundaries

- DNS request path remains memory-only.
- SQLite remains persistence-only.
- Federation remains disabled by default.
- FastSeed does not link against `globalboostd`.
- Service runs as an unprivileged `fastseed` account.
- Systemd uses `NoNewPrivileges`, `ProtectSystem=strict`, `ProtectHome`, and bounded writable paths.

## Deployment

Build and test first, then review `config/fastseed.phase9.example.toml` and copy it to `/etc/bsty-fastseed/fastseed.toml`.

The Phase 9 service is deliberately not enabled automatically by the install script.
