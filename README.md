# PULSAR

PULSAR is a Linux-first professional video editor foundation. At M8 it contains a headless playback foundation, FFmpeg media adapters, real-media tests, and an opt-in Qt Quick desktop media viewer that can open a local video, show the first frame, play/pause, scrub the source, and display real metadata.

Product direction:

- simple by default;
- powerful by choice;
- fast by design;
- architecture before shortcuts.

## Architecture

The dependency direction is:

```text
domain <- application <- adapters / UI
```

`domain` has no Qt, FFmpeg, Vulkan, SQLite, or Linux-specific dependencies. `application` owns orchestration contracts such as commands and playback timing without depending on FFmpeg or Qt. FFmpeg and Qt live at the outer adapter/UI boundary.

## Build on Linux

Core + FFmpeg adapters and tests:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Core-only build, useful when FFmpeg development files are not installed:

```sh
cmake -S . -B build-core -G Ninja -DCMAKE_BUILD_TYPE=Debug -DPULSAR_BUILD_FFMPEG=OFF
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

Desktop media viewer, when Qt 6 Core/Gui/Qml/Quick and FFmpeg development files are available:

```sh
cmake -S . -B build-desktop -G Ninja -DCMAKE_BUILD_TYPE=Debug -DPULSAR_BUILD_DESKTOP=ON
cmake --build build-desktop
ctest --test-dir build-desktop --output-on-failure
./build-desktop/apps/pulsar-desktop/pulsar-desktop /path/to/video.mp4
```

If Qt or FFmpeg is installed in a non-standard prefix, provide the normal CMake/pkg-config environment externally, for example `CMAKE_PREFIX_PATH` or `PKG_CONFIG_PATH`. The repository does not require project-specific absolute paths.

## Development

Sanitizer build:

```sh
cmake -S . -B build-sanitize -G Ninja -DCMAKE_BUILD_TYPE=Debug -DPULSAR_ENABLE_ASAN=ON -DPULSAR_ENABLE_UBSAN=ON
cmake --build build-sanitize
ctest --test-dir build-sanitize --output-on-failure
```

Warnings as errors:

```sh
cmake -S . -B build-werror -G Ninja -DCMAKE_BUILD_TYPE=Debug -DPULSAR_WARNINGS_AS_ERRORS=ON
cmake --build build-werror
ctest --test-dir build-werror --output-on-failure
```

Benchmarks:

```sh
cmake -S . -B build-bench -G Ninja -DCMAKE_BUILD_TYPE=Release -DPULSAR_BUILD_BENCHMARKS=ON
cmake --build build-bench
./build-bench/benchmarks/pulsar-benchmarks
```

Optional clang-tidy:

```sh
cmake -S . -B build-tidy -G Ninja -DPULSAR_ENABLE_CLANG_TIDY=ON
```
