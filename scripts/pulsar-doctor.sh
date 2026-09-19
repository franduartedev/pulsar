#!/usr/bin/env bash
# PULSAR Development Doctor — read-only environment diagnostics.
#
# Tells a developer whether this Linux machine can build and test PULSAR.
# Read-only: never installs, never uses sudo, never modifies the repo or
# the system. Temporary probe files live in a mktemp directory outside the
# repository and are always removed (cleanup trap).
#
# Exit codes (documented contract):
#   0 = mandatory build environment ready (BUILD READY).
#   1 = a required build dependency is missing.
# Absence of a physical audio device/backend never produces exit 1; it only
# lowers the separate REAL AUDIO SMOKE verdict.
#
# Test-only simulation (never used in normal runs):
#   PULSAR_DOCTOR_TEST_HIDE="name[:name...]" pretends listed checks are
#   missing, to verify exit-code-1 behavior without uninstalling anything.
#   Known names: cmake, cxx, git, pkg-config, qt, libavformat, libavcodec,
#   libavutil, libswscale, libswresample, audio.

set -u

FAIL=0
HIDE="${PULSAR_DOCTOR_TEST_HIDE:-}"

hidden() {
    case ":${HIDE}:" in
        *":$1:"*) return 0 ;;
        *) return 1 ;;
    esac
}

have() {
    command -v "$1" >/dev/null 2>&1
}

first_line() {
    # $1 = command, rest = args; prints first output line or "unknown".
    local out
    if out="$("$@" 2>/dev/null | head -n 1)"; then
        if [ -n "${out}" ]; then
            printf '%s\n' "${out}"
            return 0
        fi
    fi
    printf 'unknown\n'
    return 0
}

# run_bounded <seconds> <cmd...>: run with `timeout` when available so a
# missing audio server can never hang the script.
run_bounded() {
    local secs="$1"
    shift
    if have timeout; then
        timeout "${secs}" "$@"
    else
        "$@"
    fi
}

ok()   { printf '  [OK] %s\n' "$1"; }
miss() { printf '  [MISSING] %s\n' "$1"; FAIL=1; }
info() { printf '  [..] %s\n' "$1"; }
skip() { printf '  [SKIP] %s\n' "$1"; }

echo "PULSAR Development Doctor"
echo "========================="
echo

# ---------------------------------------------------------------- core ---
echo "Core"
if have cmake && ! hidden cmake; then
    ok "CMake $(first_line cmake --version | awk '{print $3}')"
else
    miss "cmake (required)"
fi

CXX_BIN=""
for candidate in c++ g++ clang++; do
    if have "${candidate}" && ! hidden cxx; then
        CXX_BIN="${candidate}"
        break
    fi
done
if [ -n "${CXX_BIN}" ]; then
    ok "C++ compiler ${CXX_BIN} ($(first_line "${CXX_BIN}" --version))"
else
    miss "C++ compiler c++/g++/clang++ (required)"
fi

if have git && ! hidden git; then
    ok "Git $(first_line git --version | awk '{print $3}')"
else
    miss "git (required)"
fi

if have pkg-config && ! hidden pkg-config; then
    ok "pkg-config $(first_line pkg-config --version)"
else
    miss "pkg-config (required)"
fi

if have ninja; then
    info "ninja $(first_line ninja --version) (optional)"
else
    info "ninja not found (optional, Makefiles work)"
fi
echo

# ----------------------------------------------------------------- Qt ---
echo "Qt"
QT_DEV_OK=0
if (pkg-config --exists Qt6Core 2>/dev/null) && ! hidden qt; then
    QT_CORE_PC="Qt6Core $(pkg-config --modversion Qt6Core 2>/dev/null)"
else
    QT_CORE_PC=""
fi
if (pkg-config --exists Qt6Multimedia 2>/dev/null) && ! hidden qt; then
    QT_MM_PC="Qt6Multimedia $(pkg-config --modversion Qt6Multimedia 2>/dev/null)"
else
    QT_MM_PC=""
fi
if [ -n "${QT_CORE_PC}" ]; then
    ok "pkg-config: ${QT_CORE_PC}"
else
    info "pkg-config: Qt6Core metadata not present (normal; Qt ships CMake packages)"
fi
if [ -n "${QT_MM_PC}" ]; then
    ok "pkg-config: ${QT_MM_PC}"
else
    info "pkg-config: Qt6Multimedia metadata not present (normal; Qt ships CMake packages)"
fi

# Locate CMake package files across standard prefixes (do not assume /usr).
QT6_CONFIG=""
QTMM_CONFIG=""
PREFIXES="${CMAKE_PREFIX_PATH:-}:/usr:/usr/local"
OLD_IFS="${IFS}"
IFS=":"
# Intentional word splitting on ':' to walk prefixes.
# shellcheck disable=SC2086
for prefix in ${PREFIXES}; do
    [ -n "${prefix}" ] || continue
    for libdir in "${prefix}/lib/x86_64-linux-gnu/cmake" "${prefix}/lib/cmake"; do
        if [ -z "${QT6_CONFIG}" ] && [ -f "${libdir}/Qt6/Qt6Config.cmake" ]; then
            QT6_CONFIG="${libdir}/Qt6/Qt6Config.cmake"
        fi
        if [ -z "${QTMM_CONFIG}" ] && [ -f "${libdir}/Qt6Multimedia/Qt6MultimediaConfig.cmake" ]; then
            QTMM_CONFIG="${libdir}/Qt6Multimedia/Qt6MultimediaConfig.cmake"
        fi
    done
done
IFS="${OLD_IFS}"
if hidden qt; then
    QT6_CONFIG=""
    QTMM_CONFIG=""
fi
if [ -n "${QT6_CONFIG}" ]; then
    ok "Qt6Config.cmake at ${QT6_CONFIG}"
else
    miss "Qt6Config.cmake (Qt6Core development package usable by CMake)"
fi
if [ -n "${QTMM_CONFIG}" ]; then
    ok "Qt6MultimediaConfig.cmake at ${QTMM_CONFIG}"
else
    miss "Qt6MultimediaConfig.cmake (Qt6 Multimedia development package)"
fi

if have qmake6 && ! hidden qt; then
    info "qmake6 reports Qt $(qmake6 -query QT_VERSION 2>/dev/null || echo unknown)"
fi

# Strongest check: temporary CMake project that configures, compiles and
# links Qt6::Core + Qt6::Multimedia, then probes audio devices at runtime.
QT_PROBE_OK=0
QT_DEVICE_COUNT=""
QT_DEFAULT_VALID=""
if [ -n "${QT6_CONFIG}" ] && [ -n "${QTMM_CONFIG}" ]; then
    if have cmake && have mktemp; then
        PROBE_DIR="$(mktemp -d 2>/dev/null || echo '')"
        if [ -n "${PROBE_DIR}" ] && [ -d "${PROBE_DIR}" ]; then
            cleanup_probe() {
                # shellcheck disable=SC2317
                rm -rf "${PROBE_DIR}"
            }
            trap cleanup_probe EXIT
            cat > "${PROBE_DIR}/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.20)
project(pulsar_doctor_probe LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
find_package(Qt6 REQUIRED COMPONENTS Core Multimedia)
add_executable(qt_link_probe link_probe.cpp)
target_link_libraries(qt_link_probe PRIVATE Qt6::Core Qt6::Multimedia)
add_executable(qt_device_probe device_probe.cpp)
target_link_libraries(qt_device_probe PRIVATE Qt6::Core Qt6::Multimedia)
EOF
            cat > "${PROBE_DIR}/link_probe.cpp" <<'EOF'
#include <QAudioSink>
int main() { return 0; }
EOF
            cat > "${PROBE_DIR}/device_probe.cpp" <<'EOF'
#include <QAudioDevice>
#include <QCoreApplication>
#include <QMediaDevices>
#include <cstdio>
int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  const auto outs = QMediaDevices::audioOutputs();
  const auto def = QMediaDevices::defaultAudioOutput();
  std::printf("count=%d default_valid=%d\n",
              static_cast<int>(outs.size()), static_cast<int>(!def.isNull()));
  return 0;
}
EOF
            PROBE_BUILD="${PROBE_DIR}/build"
            if cmake -S "${PROBE_DIR}" -B "${PROBE_BUILD}" -G "Unix Makefiles" \
                    >/dev/null 2>&1 &&
               cmake --build "${PROBE_BUILD}" >/dev/null 2>&1; then
                QT_PROBE_OK=1
                ok "CMake Qt Multimedia compile+link probe"
                DEVICE_OUT="$(run_bounded 15 "${PROBE_BUILD}/qt_device_probe" 2>/dev/null || echo '')"
                QT_DEVICE_COUNT="$(printf '%s' "${DEVICE_OUT}" |
                    sed -n 's/^count=\([0-9][0-9]*\).*/\1/p' | head -n 1)"
                QT_DEFAULT_VALID="$(printf '%s' "${DEVICE_OUT}" |
                    sed -n 's/^.*default_valid=\([01]\).*/\1/p' | head -n 1)"
            else
                miss "CMake Qt Multimedia compile+link probe (can not build against Qt)"
            fi
            rm -rf "${PROBE_DIR}"
            trap - EXIT
        else
            miss "temporary probe directory (mktemp unavailable)"
        fi
    else
        miss "temporary probe directory (cmake or mktemp unavailable)"
    fi
fi
echo

# ------------------------------------------------------------- FFmpeg ---
echo "FFmpeg"
for lib in libavformat libavcodec libavutil libswscale libswresample; do
    if (pkg-config --exists "${lib}" 2>/dev/null) && ! hidden "${lib}"; then
        ok "${lib} $(pkg-config --modversion "${lib}" 2>/dev/null)"
    else
        miss "${lib} development package (headers + pkg-config metadata)"
    fi
done
if have ffmpeg; then
    info "ffmpeg CLI $(first_line ffmpeg -version | awk '{print $3}') (runtime tool only)"
else
    info "ffmpeg CLI not installed (optional; not a substitute for dev packages)"
fi
echo

# ------------------------------------------------------ Audio runtime ---
echo "Audio runtime"
AUDIO_REACHABLE=0
if hidden audio; then
    info "audio checks hidden by PULSAR_DOCTOR_TEST_HIDE"
else
    if have pactl; then
        if run_bounded 5 pactl info >/dev/null 2>&1; then
            ok "PulseAudio-compatible server reachable (pactl)"
            AUDIO_REACHABLE=1
        else
            info "pactl present but no server reachable"
        fi
    else
        info "pactl not installed"
    fi
    if have wpctl; then
        if run_bounded 5 wpctl status >/dev/null 2>&1; then
            ok "PipeWire status readable (wpctl)"
        else
            info "wpctl present but PipeWire status unavailable"
        fi
    else
        info "wpctl not installed"
    fi
    if have pipewire; then
        PIPEWIRE_VERSION="$(pipewire --version 2>/dev/null | grep -m 1 -E '[0-9]+\.[0-9]+' || echo '')"
        if [ -n "${PIPEWIRE_VERSION}" ]; then
            info "pipewire ${PIPEWIRE_VERSION}"
        else
            info "pipewire installed (version line unavailable)"
        fi
    else
        info "pipewire not installed"
    fi
fi
if [ -n "${QT_DEVICE_COUNT}" ]; then
    if [ "${QT_DEVICE_COUNT}" -gt 0 ] 2>/dev/null; then
        ok "${QT_DEVICE_COUNT} Qt audio output device(s)"
    else
        info "Qt reports 0 audio output devices"
    fi
    if [ "${QT_DEFAULT_VALID}" = "1" ]; then
        ok "default Qt audio output valid"
    elif [ -n "${QT_DEFAULT_VALID}" ]; then
        info "default Qt audio output not valid"
    fi
elif [ "${QT_PROBE_OK}" = "1" ]; then
    info "Qt audio device probe produced no result (no backend/display)"
fi
echo

# --------------------------------------------------------------- Result
echo "Result"
if [ "${FAIL}" -eq 0 ]; then
    echo "  BUILD READY"
else
    echo "  BUILD NOT READY (missing mandatory dependencies above)"
fi
if [ "${AUDIO_REACHABLE}" -eq 1 ] && [ "${QT_DEFAULT_VALID}" = "1" ]; then
    echo "  REAL AUDIO SMOKE READY"
else
    echo "  REAL AUDIO SMOKE NOT AVAILABLE"
fi

exit "${FAIL}"
