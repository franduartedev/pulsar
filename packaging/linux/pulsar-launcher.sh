#!/usr/bin/env bash
# Starts PULSAR from the portable bundle. Works from any directory and from a symlink.
HERE="$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")" && pwd)"
# A host Qt/QML environment must not leak into the bundled runtime.
unset QT_PLUGIN_PATH QML2_IMPORT_PATH QML_IMPORT_PATH QT_QPA_PLATFORMTHEME LD_PRELOAD
exec "$HERE/usr/bin/pulsar-desktop" "$@"
