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

## E2.1 transform baseline (diagnostic, same machine)

Synthetic reusable RGBA, 60 frames at 1920×1080: default 1 layer 0.332 s
(180.5 fps); transformed 1 layer 3.836 s (15.6 fps); transformed 3 layers
3.661 s (16.4 fps). Real MPEG-4/FFmpeg: default 1 layer 0.610 s (98.4 fps);
transformed 1 layer 3.825 s (15.7 fps); transformed 3 layers 4.337 s
(13.8 fps). Peak decoders 1/1/3; retained source frames 2/2/6 (FFmpeg).
The transformed CPU path is a deterministic bilinear source-over reference;
it is ~6× slower than the default opaque path and is not optimized in E2.1.
Default single-track behavior is unchanged from E1.
