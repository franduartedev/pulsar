#!/usr/bin/env bash
# Builds the FFmpeg that PULSAR ships: FFmpeg 6.1.1 (unmodified upstream release) + x264, both from
# source, so that the distributed binaries are exactly what the shipped source archives produce.
#
#   FFmpeg: --enable-gpl --enable-version3   (GPL-3.0-or-later; NOT --enable-nonfree)
#   x264:   linked statically into libavcodec for H.264 export (GPL-2.0-or-later)
#   dav1d:  AV1 decoding, from the Ubuntu 24.04 libdav1d-dev package (BSD-2-Clause)
#   no libzvbi, x265, xvid or any other external codec library; no network protocols; no
#   avfilter/avdevice/postproc; no system library auto-detection.
#
# Usage: build-ffmpeg.sh [PREFIX]   (default: <repo>/build-release-desktop/ffmpeg-prefix)
# Output: PREFIX/{lib,include,lib/pkgconfig} and PREFIX/BUILDINFO.txt. Source archives are kept
# in $PULSAR_THIRD_PARTY_DIR (default build-release-desktop/third-party) so they can be shipped.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
WORK="$ROOT/build-release-desktop"
PREFIX="$(realpath -m "${1:-$WORK/ffmpeg-prefix}")"
SRC="$(realpath -m "${PULSAR_THIRD_PARTY_DIR:-$WORK/third-party}")"
JOBS="${JOBS:-$(nproc)}"
POOL=http://archive.ubuntu.com/ubuntu/pool/universe

# Pinned inputs (name | url | sha256). The FFmpeg archive is the upstream 6.1.1 release tarball
# (the Ubuntu "orig" tarball is byte-identical to it).
FFMPEG_TAR=ffmpeg_6.1.1.orig.tar.xz
FFMPEG_URL=$POOL/f/ffmpeg/$FFMPEG_TAR
FFMPEG_SHA=8684f4b00f94b85461884c3719382f1261f0d9eb3d59640a1f4ac0873616f968
X264_TAR=x264_0.164.3108+git31e19f9.orig.tar.gz
X264_URL=$POOL/x/x264/x264_0.164.3108%2bgit31e19f9.orig.tar.gz
X264_SHA=41606cb8e788a7f8c4514290646d4ba5c7bc68d9e1ccd1a73f446a90546913eb

DAV1D_TAR=dav1d_1.4.1.orig.tar.xz    # source only: the binary is the Ubuntu libdav1d7 package (BSD-2-Clause)
DAV1D_URL=http://archive.ubuntu.com/ubuntu/pool/universe/d/dav1d/$DAV1D_TAR
DAV1D_SHA=8d407dd5fe7986413c937b14e67f36aebd06e1fa5cfec679d10e548476f2d5f8

die() { echo "error: $*" >&2; exit 1; }
for tool in curl tar xz make nasm pkg-config sha256sum gcc; do command -v "$tool" >/dev/null || die "missing tool: $tool"; done
pkg-config --exists dav1d || die "libdav1d-dev is required (apt install libdav1d-dev)"
mkdir -p "$SRC"

fetch() { # name url sha
  local file="$SRC/$1"
  [ -f "$file" ] || curl -sSfL -o "$file" "$2"
  echo "$3  $file" | sha256sum -c --quiet - || die "checksum mismatch for $1"
}
fetch "$FFMPEG_TAR" "$FFMPEG_URL" "$FFMPEG_SHA"
fetch "$X264_TAR" "$X264_URL" "$X264_SHA"
fetch "$DAV1D_TAR" "$DAV1D_URL" "$DAV1D_SHA"

BUILD="$WORK/ffmpeg-build"
rm -rf "$BUILD" "$PREFIX"; mkdir -p "$BUILD" "$PREFIX"
tar -C "$BUILD" -xf "$SRC/$X264_TAR"; tar -C "$BUILD" -xf "$SRC/$FFMPEG_TAR"
X264_DIR="$(find "$BUILD" -maxdepth 1 -type d -name 'x264-*' | head -1)"
FFMPEG_DIR="$BUILD/ffmpeg-6.1.1"
[ -d "$X264_DIR" ] && [ -d "$FFMPEG_DIR" ] || die "unexpected source layout"

X264_FLAGS=(--prefix="$PREFIX" --enable-static --enable-pic --bit-depth=8 --disable-cli --disable-opencl
            --disable-avs --disable-swscale --disable-lavf --disable-ffms --disable-gpac --disable-lsmash)
FFMPEG_FLAGS=(--prefix="$PREFIX" --enable-shared --disable-static --enable-pic
              --enable-gpl --enable-version3
              --disable-autodetect --disable-programs --disable-doc --disable-debug
              --disable-avdevice --disable-avfilter --disable-postproc --disable-network
              --enable-zlib --enable-libdav1d --enable-libx264
              "--extra-ldflags=-Wl,--exclude-libs,libx264.a" --pkg-config-flags=--static)

echo "== x264"; ( cd "$X264_DIR" && ./configure "${X264_FLAGS[@]}" >/dev/null && make -j"$JOBS" >/dev/null && make install >/dev/null )
echo "== FFmpeg"; ( cd "$FFMPEG_DIR" && PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig" ./configure "${FFMPEG_FLAGS[@]}" >/dev/null \
  && make -j"$JOBS" >/dev/null && make install >/dev/null )

{
  echo "FFmpeg 6.1.1 ($FFMPEG_TAR sha256 $FFMPEG_SHA)"
  echo "x264 0.164.3108+git31e19f9 ($X264_TAR sha256 $X264_SHA)"
  echo "dav1d $(pkg-config --modversion dav1d) (system package libdav1d-dev, linked dynamically)"
  echo "compiler: $(gcc --version | head -1)"
  echo "x264 configure: ${X264_FLAGS[*]}"
  echo "ffmpeg configure: ${FFMPEG_FLAGS[*]}"
} > "$PREFIX/BUILDINFO.txt"
echo "== done: $PREFIX"; cat "$PREFIX/BUILDINFO.txt"
