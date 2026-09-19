# Development environment

`scripts/pulsar-doctor.sh` is a read-only diagnostic: it tells you whether
your Linux machine can build and test PULSAR. It never installs anything,
never uses sudo, and never modifies the repository or the system.

Run it from the repository root:

```bash
./scripts/pulsar-doctor.sh
```

## What it checks

- **Core**: `cmake`, a C++ compiler (`c++`/`g++`/`clang++`), `git`,
  `pkg-config`, with versions. `ninja` is reported as optional only.
- **Qt**: `Qt6Core` / `Qt6Multimedia` via `pkg-config` (when present),
  location of `Qt6Config.cmake` / `Qt6MultimediaConfig.cmake` across
  standard prefixes, then a temporary out-of-repo CMake project that
  configures, compiles and links `Qt6::Core` + `Qt6::Multimedia`.
  This answers "can PULSAR actually compile against Qt Multimedia?",
  not just "does a library filename exist?". It also runs a tiny
  `QMediaDevices` probe reporting output-device count and default-output
  validity. No playback, no hang: everything is time-bounded.
- **FFmpeg**: development availability per library —
  `libavformat`, `libavcodec`, `libavutil`, `libswscale`,
  `libswresample` — via `pkg-config`. Each is reported independently.
  A bare `ffmpeg` CLI without headers/link metadata does NOT count;
  the CLI version is reported separately for information only.
- **Audio runtime**: `pactl`/`wpctl`/`pipewire` reachability. This is
  informational for builds; it only feeds the smoke verdict below.

## Result levels

- `BUILD READY` (exit 0): every mandatory compile dependency exists.
- `BUILD NOT READY` (exit 1): at least one required build dependency
  is missing. Fix the `[MISSING]` lines.
- `REAL AUDIO SMOKE READY` vs `REAL AUDIO SMOKE NOT AVAILABLE`: whether
  a usable Qt audio output appears available (reachable audio server
  plus a valid default output). A headless CI machine legitimately
  reports `BUILD READY` + `REAL AUDIO SMOKE NOT AVAILABLE`.

Absence of a physical audio device never causes exit 1 on its own.

## Mandatory dependencies (conceptual)

To build the full stack you need, at a conceptual level: a C++20
compiler, CMake, pkg-config, Qt6 base + Multimedia development packages
usable by CMake, and FFmpeg development packages for format, codec,
utility, scaling and resampling libraries. A running PulseAudio/PipeWire
server is a runtime concern for real audio output, not a build
requirement.

## Testing the doctor itself

Without uninstalling anything, simulate a missing dependency:

```bash
PULSAR_DOCTOR_TEST_HIDE="cmake" ./scripts/pulsar-doctor.sh; echo $?
# expect exit 1 with cmake reported MISSING
```

Known names: `cmake`, `cxx`, `git`, `pkg-config`, `qt`,
`libavformat`, `libavcodec`, `libavutil`, `libswscale`,
`libswresample`, `audio`. This mode exists only to verify the
exit-code contract.
