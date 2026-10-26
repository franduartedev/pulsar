#!/usr/bin/env bash
# Builds a portable PULSAR folder for Windows from an MSYS2 UCRT64 build tree.
#   usage: packaging/windows/bundle.sh <build-dir> <output-dir>
# Run inside the MSYS2 UCRT64 shell. The result runs by double-clicking
# pulsar-desktop.exe; no Qt/FFmpeg installation is needed on the target PC.
set -euo pipefail

build_dir=${1:?build dir}
out_dir=${2:?output dir}
root=$(cd "$(dirname "$0")/../.." && pwd)
exe="$build_dir/apps/pulsar-desktop/pulsar-desktop.exe"

rm -rf "$out_dir"
mkdir -p "$out_dir"
cp "$exe" "$out_dir/"
cp -r "$root/assets" "$out_dir/assets"
cp "$root/LICENSE" "$root/THIRD_PARTY_NOTICES.md" "$out_dir/"

# Qt runtime: platform plugins, QML modules, multimedia backend...
windeployqt6 --release --no-translations --qmldir "$root/apps/pulsar-desktop/qml" \
  "$out_dir/pulsar-desktop.exe"

# Everything else the exe (and the deployed plugins) link against: FFmpeg, x264, libstdc++...
copy_deps() {
  ldd "$1" | awk '/=> \/ucrt64\//{print $3}' | while read -r dll; do
    [ -f "$out_dir/$(basename "$dll")" ] || cp "$dll" "$out_dir/"
  done
}
copy_deps "$out_dir/pulsar-desktop.exe"
# Iterate so plugin / DLL dependencies of the copied DLLs are picked up as well.
for _ in 1 2 3; do
  while IFS= read -r file; do copy_deps "$file"; done < <(find "$out_dir" -name '*.dll')
done

echo "Bundle ready in $out_dir ($(du -sh "$out_dir" | cut -f1))"
