# Phase 10.11 — Scheduler Feedback Loop & Dynamic Capacity

Adds closed-loop transport capacity control. Probe outcomes feed back into independent Direct TCP, Tor SOCKS5, and I2P SAM concurrency capacities.

- hard minimum/target/maximum per transport
- success/latency based capacity increase
- failure streak, low success rate, timeout, and high latency based decrease
- global capacity ceiling
- admission checks before launching probes
- unsupported outcomes do not falsely count as transport failures
- reset returns a transport to its configured target

The controller does not change DNS behavior: Tor/I2P remain outside A/AAAA output.
