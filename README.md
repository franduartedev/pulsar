# PULSAR

PULSAR is a Linux-first video editor currently in development.

Initial principles:

- fast by design;
- simple by default;
- power when needed;
- modular architecture;
- efficiency as a design requirement.

## Build on Linux

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

## Development

Sanitizer build:

```sh
cmake -S . -B build-sanitize -G Ninja -DCMAKE_BUILD_TYPE=Debug -DPULSAR_ENABLE_ASAN=ON -DPULSAR_ENABLE_UBSAN=ON
cmake --build build-sanitize
ctest --test-dir build-sanitize --output-on-failure
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
