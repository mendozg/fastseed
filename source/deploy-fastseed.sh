#!/usr/bin/env bash
set -Eeuo pipefail
trap 'echo "Deployment failed at line $LINENO. Existing service was not deliberately disabled." >&2' ERR
die(){ echo "ERROR: $*" >&2; exit 1; }
[[ $# -ge 1 && $# -le 2 ]] || die "Usage: sudo bash deploy-fastseed.sh seeder.example.org [nameserver.example.org]"
DOMAIN="${1%.}"
NAMESERVER="${2:-$DOMAIN}"
NAMESERVER="${NAMESERVER%.}"
[[ "$DOMAIN" =~ ^[a-zA-Z0-9-]+(\.[a-zA-Z0-9-]+)+$ && ${#DOMAIN} -le 253 ]] || die "Invalid DNS domain"
[[ "$NAMESERVER" =~ ^[a-zA-Z0-9-]+(\.[a-zA-Z0-9-]+)+$ && ${#NAMESERVER} -le 253 ]] || die "Invalid nameserver domain"
[[ $EUID -eq 0 ]] || die "Run with sudo"
[[ ! -e /etc/bsty-fastseed/fastseed.toml ]] || die "Existing config found; use the upgrade procedure in docs/DEPLOYMENT.md"
[[ -f CMakeLists.txt && -f config/fastseed.production.toml ]] || die "Run from the source/ directory"
command -v cmake >/dev/null || die "Install cmake and build dependencies first"
command -v g++ >/dev/null || die "Install build-essential first"
if ss -H -lntu '( sport = :53 )' | grep -q .; then
  echo "Port 53 is occupied. Inspect: sudo ss -lntup | grep ':53'"
  echo "If systemd-resolved uses it, disable only DNSStubListener and check /etc/resolv.conf."
  die "Resolve port 53 conflict before deployment"
fi
echo "Building FastSeed for this host..."
cmake -S . -B build-deploy -DCMAKE_BUILD_TYPE=Release
cmake --build build-deploy -j 2
ctest --test-dir build-deploy --output-on-failure
BIN=build-deploy/bsty-fastseed
[[ -x "$BIN" ]] || die "Binary missing"
"$BIN" --self-test
install -d -m 0755 /etc/bsty-fastseed /var/lib/bsty-fastseed /var/log/bsty-fastseed
if ! id fastseed &>/dev/null; then useradd --system --home /var/lib/bsty-fastseed --shell /usr/sbin/nologin fastseed; fi
chown fastseed:fastseed /var/lib/bsty-fastseed /var/log/bsty-fastseed
CFG=/etc/bsty-fastseed/fastseed.toml
if [[ -e "$CFG" ]]; then
  cp -a "$CFG" "$CFG.bak.$(date +%Y%m%d%H%M%S)"
fi
sed -E -e "s|^zone = \".*\"|zone = \"$DOMAIN\"|" -e "s|^nameserver = \".*\"|nameserver = \"$NAMESERVER\"|" config/fastseed.production.toml > "$CFG"
chmod 0644 "$CFG"
"$BIN" --check-config "$CFG"
install -m 0755 "$BIN" /usr/local/bin/bsty-fastseed.new
install -m 0644 deploy/systemd/bsty-fastseed.service /etc/systemd/system/bsty-fastseed.service
# The service runs unprivileged and receives only the ability to bind privileged ports.
mkdir -p /etc/systemd/system/bsty-fastseed.service.d
cat > /etc/systemd/system/bsty-fastseed.service.d/dns-capability.conf <<'EOF'
[Service]
AmbientCapabilities=CAP_NET_BIND_SERVICE
CapabilityBoundingSet=CAP_NET_BIND_SERVICE
EOF
if [[ -e /usr/local/bin/bsty-fastseed ]]; then
 cp -a /usr/local/bin/bsty-fastseed /usr/local/bin/bsty-fastseed.previous
fi
mv -f /usr/local/bin/bsty-fastseed.new /usr/local/bin/bsty-fastseed
systemctl daemon-reload
systemctl enable --now bsty-fastseed
systemctl restart bsty-fastseed
systemctl --no-pager --full status bsty-fastseed
echo "Configured zone: $DOMAIN; nameserver: $NAMESERVER"
echo "Test DNS: dig @127.0.0.1 $DOMAIN A"
echo "Note: Point the domain's NS delegation to this host; an A record alone is not delegation."
