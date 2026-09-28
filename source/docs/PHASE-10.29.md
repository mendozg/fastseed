# Phase 10.29 — Mainnet Canary

## Purpose
Controlled validation of FastSeed against BSTY mainnet protocol constraints without blockchain synchronization, transaction processing, mempool handling, or consensus execution.

## Guardrails
- Mainnet P2P port: 8226
- Minimum peer protocol: 31800
- Probe must complete the BSTY VERSION handshake successfully.
- Service bit 0 is accepted; FastSeed never invents service claims.
- Target count is bounded and duplicate targets are removed.
- Canary evaluation is observational and does not publish DNS results.
- No block, transaction, UTXO, or consensus data is requested.

## Validation
The deterministic canary test covers valid peers, non-mainnet ports, oversized targets, old protocol versions, failed handshakes, duplicate target bounding, and service-zero peers.

A live TCP reachability check was attempted against the four configured BSTY DNS seed hostnames on port 8226. The execution environment did not establish TCP connections within five seconds, so no live-mainnet success is claimed.
