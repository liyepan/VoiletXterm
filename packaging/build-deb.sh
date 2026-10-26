#!/usr/bin/env bash
# Build a .deb from the staged install tree.
#   usage: build-deb.sh <stage-dir> <version> <arch> <out-dir>
set -euo pipefail

stage=${1:?usage: build-deb.sh <stage-dir> <version> <arch> <out-dir>}
version=${2:?version}
arch=${3:?arch}
out=${4:-dist}

pkg=voiletxterm
root=$(mktemp -d)
trap 'rm -rf "$root"' EXIT

cp -a "$stage"/. "$root"/
mkdir -p "$root/DEBIAN"
installed_size=$(du -sk "$root/usr" | cut -f1)

cat > "$root/DEBIAN/control" <<EOF
Package: $pkg
Version: $version
Section: utils
Priority: optional
Architecture: $arch
Maintainer: liyepan <liyepan@linuxacme.com>
Installed-Size: $installed_size
Depends: libc6, libstdc++6, libqt5core5a | libqt5core5t64, libqt5gui5 | libqt5gui5t64, libqt5widgets5 | libqt5widgets5t64, libqt5network5 | libqt5network5t64, libqt5serialport5 | libqt5serialport5t64
Description: VoiletXterm - SSH / Telnet / serial terminal
 A lightweight terminal workspace with its own VT100/xterm engine.
 SSH runs on a real pseudo terminal, Telnet and serial are built in.
EOF

mkdir -p "$out"
dpkg-deb --root-owner-group --build "$root" "$out/${pkg}_${version}_${arch}.deb"
echo "built $out/${pkg}_${version}_${arch}.deb"
