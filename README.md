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
