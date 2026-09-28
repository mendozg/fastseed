# Phase 11 — Native DNS

FastSeed can serve its own authoritative DNS responses directly. BIND9 and Nginx are not required.

## Services

- UDP DNS: port 53 by default
- TCP DNS: port 53 by default
- A records: IPv4 HOT/WARM peers
- AAAA records: IPv6 HOT/WARM peers
- authoritative zone configured by `DnsConfig::zone`
- atomic in-memory snapshot reads
- SQLite and crawler are never accessed from the DNS request path
- per-source and global rate limiting

## Deployment

For `seeder.globalboost.info`, point the domain's authoritative DNS delegation/records at the host running FastSeed. FastSeed itself answers DNS queries; an HTTP reverse proxy is unnecessary.

Binding to port 53 normally requires root or an appropriate Linux capability such as `CAP_NET_BIND_SERVICE`. The FastSeed process should otherwise run as the dedicated `fastseed` user.

## Architecture

Crawler/health/database -> validated DNS snapshot -> NativeDnsServer -> UDP/TCP clients

The DNS request path is deliberately read-only and memory-only.
