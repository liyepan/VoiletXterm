#!/usr/bin/env bash
# Build a .rpm from the staged install tree.
#   usage: build-rpm.sh <stage-dir> <version> <arch> <out-dir>
set -euo pipefail

stage=${1:?usage: build-rpm.sh <stage-dir> <version> <arch> <out-dir>}
version=${2:?version}
arch=${3:?arch}
out=${4:-dist}

stage=$(cd "$stage" && pwd)
mkdir -p "$out"
out=$(cd "$out" && pwd)

top=$(mktemp -d)
trap 'rm -rf "$top"' EXIT
mkdir -p "$top"/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

cat > "$top/SPECS/voiletxterm.spec" <<EOF
Name:           voiletxterm
Version:        $version
Release:        1
Summary:        VoiletXterm - SSH / Telnet / serial terminal
License:        MIT
URL:            https://git.linuxacme.com/liyepan/VoiletXterm
BuildArch:      $arch
AutoReqProv:    yes

%description
A lightweight terminal workspace with its own VT100/xterm engine.
SSH runs on a real pseudo terminal, Telnet and serial are built in.

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}
cp -a "$stage"/usr %{buildroot}/usr

%files
/usr/bin/voiletxterm
/usr/share/applications/voiletxterm.desktop
/usr/share/icons/hicolor/256x256/apps/voiletxterm.png

%changelog
EOF

rpmbuild -bb --define "_topdir $top" "$top/SPECS/voiletxterm.spec"

find "$top/RPMS" -name '*.rpm' -exec cp -v {} "$out/" \;
