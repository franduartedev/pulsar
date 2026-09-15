# Benchmarks

PULSAR includes a minimal local benchmark harness for existing domain operations.

Build it with:

```sh
cmake -S . -B build-bench -G Ninja -DCMAKE_BUILD_TYPE=Release -DPULSAR_BUILD_BENCHMARKS=ON
cmake --build build-bench
./build-bench/benchmarks/pulsar-benchmarks
```

The harness uses `std::chrono::steady_clock` and a small volatile sink to consume results so the optimizer cannot remove the measured work entirely. These are trend indicators for local development, not scientific measurements or strict CI thresholds.
