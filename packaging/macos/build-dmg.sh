#!/usr/bin/env bash
# Create a compressed dmg containing the app and an /Applications symlink.
#   usage: build-dmg.sh <path-to-.app> <version> <arch> <out-dir>
set -euo pipefail

app=${1:?usage: build-dmg.sh <app> <version> <arch> <out-dir>}
version=${2:?version}
arch=${3:?arch}
out=${4:-dist}

mkdir -p "$out"
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT

cp -R "$app" "$stage/"
ln -s /Applications "$stage/Applications"

target="$out/VoiletXterm-${version}-${arch}.dmg"
rm -f "$target"
hdiutil create -volname "VoiletXterm $version" -srcfolder "$stage" -ov -format UDZO -quiet "$target"
echo "built $target"
