#!/usr/bin/env bash
# Builds the PULSAR Linux release candidates from a clean checkout:
#   dist/PULSAR-<version>-linux-x86_64.tar.gz   portable bundle (recommended)
#   dist/PULSAR-<version>-x86_64.AppImage       AppImage (needs FUSE 2, or --appimage-extract-and-run)
#   dist/SHA256SUMS
# Requirements: cmake, ninja, g++, Qt 6 + FFmpeg development packages (see README.md),
# dpkg (Debian/Ubuntu), curl, tar. Run from anywhere. Nothing here publishes anything.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="$ROOT/build-release-desktop"
DIST="${PULSAR_DIST_DIR:-$ROOT/dist}"
TOOLS="${PULSAR_TOOLS_DIR:-$ROOT/build-release-desktop/packaging-tools}"
STAGE="$BUILD/packaging-stage"
QMAKE="${QMAKE:-$(command -v qmake6 || echo /usr/lib/qt6/bin/qmake6)}"

die() { echo "error: $*" >&2; exit 1; }
for tool in cmake ninja curl tar dpkg sha256sum; do command -v "$tool" >/dev/null || die "missing tool: $tool"; done
[ -x "$QMAKE" ] || die "qmake6 not found (set QMAKE=/path/to/qmake6)"

# --- pinned packaging tools (linuxdeploy "continuous" builds; verified by hash) -------------
declare -A TOOL_URL=(
  [linuxdeploy-x86_64.AppImage]=https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
  [linuxdeploy-plugin-qt-x86_64.AppImage]=https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
  [linuxdeploy-plugin-appimage-x86_64.AppImage]=https://github.com/linuxdeploy/linuxdeploy-plugin-appimage/releases/download/continuous/linuxdeploy-plugin-appimage-x86_64.AppImage
)
declare -A TOOL_SHA=(
  [linuxdeploy-x86_64.AppImage]=8aea8da0f7f7039d2a2cecb14657d752a222a5e1d3825caeef186c82f751cdd1
  [linuxdeploy-plugin-qt-x86_64.AppImage]=cfc1055b2b9dbc08412b579f20990b7b41a17b61beaa5847dc9477c96c9e9617
  [linuxdeploy-plugin-appimage-x86_64.AppImage]=49d6a17160675a6bd1781699aae6bdf7692d98552e02a3671d2183d10547842e
)
mkdir -p "$TOOLS"
for name in "${!TOOL_URL[@]}"; do
  [ -f "$TOOLS/$name" ] || curl -fsSL -o "$TOOLS/$name" "${TOOL_URL[$name]}"
  echo "${TOOL_SHA[$name]}  $TOOLS/$name" | sha256sum -c --quiet - \
    || die "$name does not match the pinned hash (update TOOL_SHA after reviewing the new build)"
  chmod +x "$TOOLS/$name"
done
ln -sf "$TOOLS/linuxdeploy-plugin-appimage-x86_64.AppImage" "$TOOLS/linuxdeploy-plugin-appimage"
ln -sf "$TOOLS/linuxdeploy-plugin-qt-x86_64.AppImage" "$TOOLS/linuxdeploy-plugin-qt"
export APPIMAGE_EXTRACT_AND_RUN=1   # the tools themselves run without FUSE
export PATH="$TOOLS:$PATH"

# --- build + install --------------------------------------------------------------------------
cmake -S "$ROOT" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release -DPULSAR_BUILD_TESTS=OFF \
      -DPULSAR_BUILD_DESKTOP=ON -DPULSAR_BUILD_QT_AUDIO=ON -DCMAKE_INSTALL_PREFIX=/usr \
      "-DCMAKE_CXX_FLAGS=-ffile-prefix-map=$ROOT=. -fdebug-prefix-map=$ROOT=."
cmake --build "$BUILD"
VERSION="$("$BUILD/apps/pulsar-desktop/pulsar-desktop" --version | awk '{print $2}')"
[ -n "$VERSION" ] || die "could not read the product version"
NAME="PULSAR-$VERSION"
echo "== packaging $NAME"

rm -rf "$STAGE"; mkdir -p "$STAGE" "$DIST"
APPDIR="$STAGE/AppDir"
DESTDIR="$APPDIR" cmake --install "$BUILD" --prefix /usr

# --- bundle Qt + FFmpeg runtime ---------------------------------------------------------------
export QMAKE QML_SOURCES_PATHS="$ROOT/apps/pulsar-desktop/qml:$ROOT/packaging/linux" DEPLOY_PLATFORM_THEMES=0 \
       EXTRA_PLATFORM_PLUGINS="libqwayland-egl.so;libqwayland-generic.so" VERSION
( cd "$STAGE" && linuxdeploy-x86_64.AppImage --appdir "$APPDIR" \
    --executable "$APPDIR/usr/bin/pulsar-desktop" \
    --desktop-file "$APPDIR/usr/share/applications/pulsar.desktop" \
    --icon-file "$APPDIR/usr/share/icons/hicolor/256x256/apps/pulsar.png" --plugin qt )

# The GTK platform theme drags GTK + GSettings into the bundle and crashes on hosts without
# matching schemas; Qt falls back to its own theme and Qt Quick file dialogs.
rm -rf "$APPDIR/usr/plugins/platformthemes"

# Fail the build (instead of shipping a crashing file dialog) if a required QML module is missing.
for module in Qt/labs/folderlistmodel QtQuick/Dialogs QtQuick/Controls QtQuick/Window; do
  [ -f "$APPDIR/usr/qml/$module/qmldir" ] || die "bundle is missing the QML module $module"
done

# --- prune libraries nothing refers to --------------------------------------------------------
prune_unused_libs() {
  local libdir="$APPDIR/usr/lib" used="$STAGE/used-libs.txt"
  : > "$used"
  while IFS= read -r elf; do
    LD_LIBRARY_PATH="$libdir" ldd "$elf" 2>/dev/null | awk -v d="$libdir" '$3 ~ d {print $3}' >> "$used" || true
  done < <(find "$APPDIR/usr/bin" "$APPDIR/usr/plugins" "$APPDIR/usr/qml" "$libdir" -type f \( -name '*.so*' -o -name 'pulsar-desktop' \))
  sort -u "$used" -o "$used"
  find "$libdir" -maxdepth 1 \( -type f -o -type l \) | while read -r lib; do
    real="$(readlink -f "$lib")"
    grep -qxF "$lib" "$used" || grep -qxF "$real" "$used" || rm -f "$lib"
  done
}
prune_unused_libs; prune_unused_libs   # second pass catches libraries only the first pass's removals orphaned
find "$APPDIR/usr/lib" -xtype l -delete

# --- licenses of everything that ships --------------------------------------------------------
LIC="$APPDIR/usr/share/doc/pulsar-third-party"; mkdir -p "$LIC"
: > "$LIC/PACKAGES.txt"
{ find "$APPDIR/usr/lib" "$APPDIR/usr/plugins" "$APPDIR/usr/qml" -type f -name '*.so*' -printf '%f\n' | sort -u; } | while read -r so; do
  for dir in /usr/lib/x86_64-linux-gnu /lib/x86_64-linux-gnu /usr/lib/x86_64-linux-gnu/qt6/plugins/* /usr/lib/x86_64-linux-gnu/qt6/qml/*; do
    if pkg="$(dpkg -S "$dir/$so" 2>/dev/null | head -1 | cut -d: -f1)"; [ -n "$pkg" ]; then echo "$pkg"; break; fi
  done
done | sort -u > "$STAGE/pkgs.txt"
while read -r pkg; do
  ver="$(dpkg-query -W -f='${Version}' "$pkg" 2>/dev/null || true)"
  echo "$pkg $ver" >> "$LIC/PACKAGES.txt"
  [ -f "/usr/share/doc/$pkg/copyright" ] && cp "/usr/share/doc/$pkg/copyright" "$LIC/$pkg.copyright" || true
done < "$STAGE/pkgs.txt"


# --- portable tar.gz ----------------------------------------------------------------------------
BUNDLE="$STAGE/$NAME"
rm -rf "$BUNDLE"; mkdir -p "$BUNDLE"
cp -a "$APPDIR/usr" "$BUNDLE/usr"
cp "$ROOT/packaging/linux/pulsar-launcher.sh" "$BUNDLE/pulsar"; chmod +x "$BUNDLE/pulsar"
cp "$ROOT/docs/release/0.1.0-beta.md" "$BUNDLE/README-BETA.md"
cp "$ROOT/CHANGELOG.md" "$BUNDLE/CHANGELOG.md"
cp -a "$ROOT/packaging/linux/pulsar.desktop" "$ROOT/packaging/linux/pulsar.svg" "$BUNDLE/"
EPOCH="$(git -C "$ROOT" log -1 --format=%ct 2>/dev/null || date +%s)"
tar --sort=name --owner=0 --group=0 --numeric-owner --mtime="@$EPOCH" -C "$STAGE" -czf "$DIST/$NAME-linux-x86_64.tar.gz" "$NAME"

# --- AppImage ---------------------------------------------------------------------------------
( cd "$DIST" && ARCH=x86_64 linuxdeploy-plugin-appimage --appdir "$APPDIR" )
APPIMAGE="$(ls -t "$DIST"/*.AppImage | head -1)"
[ "$(basename "$APPIMAGE")" = "$NAME-x86_64.AppImage" ] || mv "$APPIMAGE" "$DIST/$NAME-x86_64.AppImage"

( cd "$DIST" && sha256sum "$NAME-linux-x86_64.tar.gz" "$NAME-x86_64.AppImage" > SHA256SUMS && cat SHA256SUMS && ls -l "$NAME"* )
