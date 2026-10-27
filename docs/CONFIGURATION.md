# FastSeed Configuration

FastSeed uses a typed TOML configuration loaded at startup. The configuration is validated before any network socket or database is opened.

## Commands

```bash
./bsty-fastseed --check-config config/fastseed.production.toml
./bsty-fastseed run --config /etc/bsty-fastseed/fastseed.toml
```

`run --dns-port PORT` remains available as a temporary command-line override for the DNS listener port.

## Main sections

- `[network]` — BSTY mainnet identity and P2P protocol constants.
- `[bootstrap]` — DNS seed sources.
- `[crawler]` and `[crawler.cooldown]` — peer discovery and probe limits.
- `[cache]` and `[health]` — HOT/WARM/COLD peer lifecycle.
- `[database]` and `[database.persistence]` — SQLite persistence.
- `[dns]` and `[dns.rate_limit]` — native authoritative DNS listener and limits.
- `[scheduler]` — adaptive scheduler quotas, refresh periods, confidence and exploration controls.
- `[transport.*]` — optional Tor SOCKS5 and I2P SAM endpoints.
- `[network_intelligence]` — advisory peer scoring configuration.
- `[federation]` — optional seeder-mesh identity and message limits.

## Safety rules

The validator rejects unknown keys and unsafe values. BSTY mainnet configuration requires P2P port `8226` and a protocol version at or above the BSTY minimum peer version. DNS response limits and crawler concurrency are bounded before runtime startup.

Configuration is parsed once at startup. DNS request processing uses the already constructed runtime objects and does not read the TOML file or SQLite database.

## Ubuntu 22.04 production example

```bash
sudo install -d -o fastseed -g fastseed /etc/bsty-fastseed
sudo cp config/fastseed.production.toml /etc/bsty-fastseed/fastseed.toml
sudo chown fastseed:fastseed /etc/bsty-fastseed/fastseed.toml
sudo -u fastseed /usr/local/bin/bsty-fastseed --check-config /etc/bsty-fastseed/fastseed.toml
```

The production systemd unit invokes the same configuration file with:

```text
bsty-fastseed run --config /etc/bsty-fastseed/fastseed.toml
```


## v1.0.0 DNS authority

`dns.nameserver` is the authoritative NS/MNAME DNS name; empty defaults to the
zone apex. `dns.soa_mailbox` is a DNS-format mailbox such as
`hostmaster.example.org` (not an email address); empty uses `hostmaster.<zone>`.
`dns.soa_serial` is an unsigned 32-bit serial (default 1); increment on authority
configuration changes. SOA refresh/retry/expire are 3600/600/86400 seconds;
negative TTL uses `dns.ttl_seconds`. `dns.max_response_size` is a UDP cap in
512..65535; retain 1232 to avoid common fragmentation problems. This cap is
advertised in response OPT; TCP is independently bounded to 65535 bytes.
Old configurations remain valid without these keys. This is an unsigned,
non-recursive authoritative zone: RA/AD are clear, DO is echoed without DNSSEC
records. Only the apex has A/AAAA peer answers and NS/SOA; other in-zone names
return NXDOMAIN. Unsupported apex types return NODATA with SOA.

For the final release deployment, upgrade, nameserver identity and limitations, see [DEPLOYMENT.md](DEPLOYMENT.md). Reboot recovery was not tested.
