# Benchmarks

PULSAR includes a minimal local benchmark harness for existing domain operations.

Build it with:

```sh
cmake -S . -B build-bench -G Ninja -DCMAKE_BUILD_TYPE=Release -DPULSAR_BUILD_BENCHMARKS=ON
cmake --build build-bench
./build-bench/benchmarks/pulsar-benchmarks
```

The harness uses `std::chrono::steady_clock` and a small volatile sink to consume results so the optimizer cannot remove the measured work entirely. These are trend indicators for local development, not scientific measurements or strict CI thresholds.

## E1 visual stack gate

Build the focused 1080p composition benchmark:

```sh
cmake -S . -B build-e1-bench -G Ninja -DCMAKE_BUILD_TYPE=Release -DPULSAR_BUILD_TESTS=OFF -DPULSAR_BUILD_BENCHMARKS=ON
cmake --build build-e1-bench --target pulsar_visual_stack_benchmark
./build-e1-bench/benchmarks/pulsar_visual_stack_benchmark
ffmpeg -hide_banner -loglevel error -y -f lavfi -i testsrc2=size=1920x1080:rate=30 -t 2 -c:v mpeg4 -q:v 5 /tmp/opencode/pulsar-e1-1080.mp4
./build-e1-bench/benchmarks/pulsar_visual_stack_benchmark /tmp/opencode/pulsar-e1-1080.mp4
```

On this development machine, 60 frames at 1920×1080: reusable synthetic RGBA,
1 layer 0.321 s (187 fps), 3 layers 0.650 s (92 fps); real MPEG-4/FFmpeg
decoding, 1 layer 0.662 s (90.7 fps), 3 layers 1.552 s (38.7 fps). Peak active
decoders: 1/3; retained source frames: 2/6 for FFmpeg. The benchmark asserts
these state bounds and output size; it excludes audio and encoding. Ordinary
single-track operation is measured alongside the layered case. These are
observations, not hard thresholds across different machines.
