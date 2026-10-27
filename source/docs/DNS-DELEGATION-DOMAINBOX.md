# DNS delegation with Domainbox

This guide delegates `seeder.globalboost.info` to the native authoritative DNS
server included with BSTY-FastSeed. BIND9 and Nginx are not required.

## Prerequisites

- A public static IPv4 address for the FastSeed server (and IPv6 if used).
- FastSeed listening on **UDP and TCP port 53**, accessible from the Internet.
- Control of the **authoritative DNS zone** for `globalboost.info`.
- A FastSeed deployment configured for `seeder.globalboost.info`.
- **Verify FastSeed answers SOA and NS authoritatively before delegation.**
  A/AAAA peer answers alone are not sufficient for a complete delegated zone.

> Domainbox may be the registrar without hosting authoritative DNS.
> Check `dig NS globalboost.info +short` and edit records at the actual
> authoritative DNS provider. Do not change the domain-wide registrar
> nameservers just to delegate this subdomain.

## 1. Configure the parent zone

Replace `203.0.113.10` with the **real** FastSeed public IPv4 address.
The address below is documentation-only and will not work on the Internet.

| Type | Name relative to `globalboost.info` | Value |
|------|-------------------------------------|-------|
| A | `ns1.seeder` | `203.0.113.10` |
| NS | `seeder` | `ns1.seeder.globalboost.info.` |

The `ns1.seeder` A record in the parent zone provides the address
(glue) needed to reach a nameserver *inside* the delegated zone.
Add an AAAA record only if IPv6 is configured and reachable.

For production resilience, use a second independent authoritative FastSeed
instance (`ns2.seeder.globalboost.info`) with its own address and an additional
NS record. Some DNS providers may not support child-zone delegation in their
standard control panel; contact their support if the records cannot be added.

## 2. Configure FastSeed

In `/etc/bsty-fastseed/fastseed.toml`, use the **actual supported keys**
for your installed release, including the authoritative zone:

```toml
[dns]
zone = "seeder.globalboost.info"
listen_address = "0.0.0.0"
port = 53
```

Validate the config and restart the service:

```bash
sudo /usr/local/bin/bsty-fastseed --check-config /etc/bsty-fastseed/fastseed.toml
sudo systemctl restart bsty-fastseed
sudo ss -lntup | grep ':53'
```

Open **both** UDP/53 and TCP/53 in the host firewall and hosting provider
firewall. Ensure `systemd-resolved` does not occupy the intended listener.
Do not disable the server's DNS resolution indiscriminately.

## 3. Test the server directly before delegation

```bash
dig @127.0.0.1 seeder.globalboost.info SOA +norecurse
dig @127.0.0.1 seeder.globalboost.info NS +norecurse
dig @127.0.0.1 seeder.globalboost.info A +norecurse
dig +tcp @127.0.0.1 seeder.globalboost.info A +norecurse
```

Repeat using the server's **public IP from another machine**:

```bash
dig @YOUR_SERVER_IP seeder.globalboost.info SOA +norecurse
dig @YOUR_SERVER_IP seeder.globalboost.info NS +norecurse
dig @YOUR_SERVER_IP seeder.globalboost.info A +norecurse
dig +tcp @YOUR_SERVER_IP seeder.globalboost.info A +norecurse
```

Check for the `aa` (authoritative answer) flag, valid SOA/NS answers,
and working UDP and TCP. Empty A/AAAA peer sets may be normal before
the crawler has validated peers; missing SOA/NS is a deployment blocker.

## 4. Verify delegation after updating Domainbox / the DNS host

```bash
dig NS seeder.globalboost.info +short
dig +trace seeder.globalboost.info A
dig @YOUR_SERVER_IP seeder.globalboost.info A
```

Allow for the parent zone's TTL and caches to expire. Test from multiple
public resolvers. If the parent zone is DNSSEC-signed, verify that its
delegation and DS behavior are consistent with the child zone.

## Troubleshooting

- **Timeout:** verify public address, firewall, port 53 and FastSeed status.
- **SERVFAIL:** inspect delegation/glue, authoritative SOA/NS and DNSSEC.
- **No A/AAAA peers:** check crawler/health state; do not publish unverified peers.
- **Wrong nameserver:** update records at the authoritative DNS host, not
  necessarily the domain registrar.

This guide changes only the `seeder.globalboost.info` delegation; the
website, email, and other `globalboost.info` records should remain untouched.
