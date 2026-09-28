#!/bin/sh
set -eu
PREFIX=${PREFIX:-/usr/local}
install -Dm755 build/bsty-fastseed "$PREFIX/bin/bsty-fastseed"
install -Dm644 deploy/systemd/bsty-fastseed.service /etc/systemd/system/bsty-fastseed.service
install -Dm644 deploy/systemd/bsty-fastseed-tmpfiles.conf /etc/tmpfiles.d/bsty-fastseed.conf
install -Dm640 config/fastseed.phase9.example.toml /etc/bsty-fastseed/fastseed.toml
printf '%s\n' 'Installed BSTY-FastSeed systemd assets. Review /etc/bsty-fastseed/fastseed.toml before enabling.'
