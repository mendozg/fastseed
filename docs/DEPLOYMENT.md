# Deploying BSTY-FastSeed v1.0.0

This source-only package targets Ubuntu 22.04/GCC11. The operator validated its
fixed runtime using Ubuntu/GCC11; see release/VERIFIED-RESULTS.txt for provenance.
Reboot recovery was not tested. Installation and restart do not require a reboot.

## Verify before installation

From source/ install the README dependencies, then:

```bash
python3 scripts/verify-source.py
bash scripts/release-gate.sh
bash scripts/sanitizer-gate.sh
```

Keep the logs. The final version output must be `BSTY-FastSeed 1.0.0 (Production)`.
The package's evidence predates final label/script/document normalization: these
commands verify the exact extracted release. Install only build-release binaries.

## Parent DNS and authority identity

Example stable public server: 142.54.191.210. At the provider hosting globalboost.info:

| Type | Name | Value |
| --- | --- | --- |
| A | nsseed | 142.54.191.210 |
| NS | seeder | nsseed.globalboost.info. |

Keep the parent zone's existing apex nameservers. This delegates only the seeder
subdomain. The FastSeed nameserver must match the parent NS target exactly.
The NS target must have a stable address; peer-answer IPs are not nameserver addresses.
This example places the target outside the delegated child zone to avoid needing
FastSeed to serve an address for a child nameserver. FastSeed serves records only
at its configured apex; it does not synthesize A/AAAA for ns1.<seed-zone>.

Within the existing [dns] section of the service config (not a duplicate section):

```toml
[dns]
zone = "seeder.globalboost.info"
nameserver = "nsseed.globalboost.info"
soa_mailbox = "hostmaster.globalboost.info"
soa_serial = 2026102601
max_response_size = 1232
```

Use a real DNS-format SOA mailbox; it is not an email string containing @.
Increment the serial when authority configuration changes. Do not publish a DS
record for this unsigned child zone. Permit inbound UDP and TCP 53 and outbound
BSTY probing on TCP 8226 plus the DNS resolution required by bootstrap sources.

## Fresh installation

The helper requires an unused DNS port and no existing /etc/bsty-fastseed/fastseed.toml:

```bash
sudo bash deploy-fastseed.sh seeder.globalboost.info nsseed.globalboost.info
```

It builds and runs full CTest, creates the unprivileged service account/data
folders, validates generated config, installs the binary/unit/capability drop-in,
and starts/enables the service. Review the generated bootstrap settings, SOA
mailbox and other operational defaults. Set a custom mailbox/serial if desired,
validate the config and restart after editing. The optional NS argument defaults
to the zone for older invocations; pass a stable explicit NS for production.

If port 53 is occupied, inspect `sudo ss -lntup`. Resolve the specific conflict;
do not blindly disable or replace the host's resolver. The helper does not reboot.

## Existing installation: preserve config and database

Do not run the fresh helper over a live installation. Build/test before downtime:

```bash
./build-release/bsty-fastseed --check-config /etc/bsty-fastseed/fastseed.toml
./build-release/bsty-fastseed probe 69.197.161.154:8226
./build-release/bsty-fastseed probe 207.231.106.156:8226
```

The addresses above are previously tested examples, not permanently guaranteed
live peers. Use currently available known-good BSTY peers where appropriate.
Record the installed version, service unit and configuration. Then back up under
a unique directory and install while stopped (database files must be copied as a
set while quiescent; do not copy only the live SQLite main file):

```bash
backup="/var/backups/bsty-fastseed/$(date -u +%Y%m%dT%H%M%SZ)"
sudo mkdir -p "$backup"
sudo systemctl stop bsty-fastseed
sudo cp -a /usr/local/bin/bsty-fastseed "$backup/bsty-fastseed"
sudo cp -a /etc/bsty-fastseed "$backup/config"
sudo cp -a /var/lib/bsty-fastseed "$backup/data"
sudo systemctl cat bsty-fastseed | sudo tee "$backup/unit.txt" >/dev/null
sudo install -m 0755 build-release/bsty-fastseed /usr/local/bin/bsty-fastseed
sudo systemctl start bsty-fastseed
sudo systemctl --no-pager --full status bsty-fastseed
```

If your database path differs, back up that directory while stopped instead.
Keep existing configuration, ownership and the working capability drop-in. This
release completes the existing schema version 4 initialization; it does not discard
peer records. Old configuration files remain accepted without the new DNS keys.

## Rollback

Stop FastSeed, restore `$backup/bsty-fastseed` to /usr/local/bin/bsty-fastseed,
then start it. If reverting config or database as well, restore from the matching
backup while stopped, including the SQLite sidecars and original ownership.
Keep a log of which backup you used. No rollback was executed as part of packaging.

## DNS and discovery checks

```bash
sudo systemctl is-enabled bsty-fastseed
sudo journalctl -u bsty-fastseed -n 50 --no-pager
dig +norecurse @127.0.0.1 seeder.globalboost.info A
dig +noedns +norecurse @127.0.0.1 seeder.globalboost.info A
dig +tcp +norecurse @127.0.0.1 seeder.globalboost.info A
dig +norecurse @127.0.0.1 seeder.globalboost.info AAAA
dig +norecurse @127.0.0.1 seeder.globalboost.info NS
dig +norecurse @127.0.0.1 seeder.globalboost.info SOA
dig +edns=1 +noednsnegotiation @127.0.0.1 seeder.globalboost.info A
```

Normal apex replies should have AA/NOERROR. SOA and NS work before peer discovery.
EDNS1 should return BADVERS. Empty A/AAAA with SOA means no eligible peers of that
family are in the published cache. A diagnostic probe does not fill the running
service's cache. The warning when RD is set is expected: this is not a recursive resolver.

From another host:

```bash
dig +norecurse @142.54.191.210 seeder.globalboost.info A
dig +tcp +norecurse @142.54.191.210 seeder.globalboost.info A
dig -4 +trace seeder.globalboost.info A
dig -4 @8.8.8.8 seeder.globalboost.info A
```

Replace IP/zone with your deployment. Success against one resolver does not establish
reachability of another; the operator observed a local Windows/WSL path failure to
1.1.1.1 while direct FastSeed and 8.8.8.8 worked. That is not evidence of a server defect.

## Stability and recovery scope

The supplied overnight run has 144 samples at five-minute intervals. It demonstrates
sampled DNS/service availability and a bounded MemoryCurrent range for that period,
not uninterrupted availability or fresh health validation between every sample.
A service restart was observed and startup is enabled. Full-server reboot recovery
was not tested, at the operator's request; leave it documented until a maintenance
window permits a real reboot check.
