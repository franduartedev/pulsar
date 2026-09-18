# X1 — Pre-I1 Playback/Edit Performance Baseline

Date: 2026-09-17.
Commit: `ffa429182de9a5b74b2a6172effaa875cfd772fc` (branch `feat/m9-cut-and-save`, tree clean).
Status: BEFORE measurement. No product behavior was changed to obtain these numbers.

## Environment

- CPU: AMD Ryzen 3 3200G with Radeon Vega Graphics, 4 logical CPUs.
- RAM: 16 GB total (15 GiB reported).
- Kernel: 7.0.0-31-generic. Compiler: c++ (Ubuntu 13.3.0).
- FFmpeg CLI: 6.1.1-3ubuntu5 (fixture generation only).
- Linked FFmpeg libs: system libavformat/libavcodec/libavutil + sysroot libswscale (see build-m9.6-desktop cache).
- GPU present: NVIDIA TU117 [GeForce GTX 1650]; not used by the measured paths (CPU decode/preview).
- Build: `build-m9.6-desktop`, `CMAKE_BUILD_TYPE=Debug`.
- Fixtures and raw outputs live outside the repo in `/dev/shm/pulsar-x1` (SSD safety).

These are controlled comparison fixtures, NOT production-representative videos.

## Fixtures

```sh
/usr/bin/ffmpeg -hide_banner -loglevel error -y -f lavfi \
  -i testsrc=duration=12:size=1920x1080:rate=30 \
  -pix_fmt yuv420p -c:v libx264 -preset veryfast fixture_1080p.mp4
/usr/bin/ffmpeg -hide_banner -loglevel error -y -f lavfi \
  -i testsrc=duration=12:size=3840x2160:rate=30 \
  -pix_fmt yuv420p -c:v libx264 -preset veryfast fixture_2160p.mp4
```

Both: H.264, yuv420p, 30 fps, ~12 s. Sizes: 250247 B (1080p), 627414 B (2160p).

## C. Playback baseline — 1080p (RED, PRE-I1 / ERROR-LIMITED)

Command: `pulsar-engine-lab --playback-metrics /dev/shm/pulsar-x1/fixture_1080p.mp4 300`
Exit status: **2**.

```text
media=fixture_1080p.mp4
ticks=300
wall_ms=22
main.requested_ticks=300
main.completed_ticks=6
main.healthy=0
main.error_message=duplicate timestamp arrived after that timestamp was presented
session.stale_event_count=0
session.control_accept_count=2
scheduler.starvation_count=3
scheduler.skipped_presentation_count=0
worker.read_request_count=1
worker.seek_request_count=1
worker.read_completion_count=1
worker.read_latency_p50_us=8
worker.read_latency_p95_us=8
worker.read_latency_max_us=8
worker.descriptor_latency_us=5543
worker.open_latency_us=7355
worker.queue_high_water=1
worker.events_dropped=0
worker.samples_dropped=0
worker.control_wait_count=0
mailbox.published=3
mailbox.evicted=0
mailbox.high_water=2
mailbox.current_size=1
harness.visual_misses=0
seek_epoch.available=1
seek_epoch.completed=1
seek_epoch.read_exercised=1
seek_epoch.healthy=1
seek_epoch.error_message=
seek_epoch.target_num=2
seek_epoch.target_den=1
seek_epoch.starvation_count=2
seek_epoch.skipped_presentation_count=0
seek_epoch.read_request_delta=1
seek_epoch.read_completion_delta=1
seek_epoch.mailbox_published_delta=3
seek_epoch.mailbox_evicted_delta=0
seek_epoch.mailbox_current_size=2
seek_epoch.visual_misses=0
stderr: Playback metrics failed: main playback entered error: duplicate timestamp arrived after that timestamp was presented
```

The known C2 duplicate-frame defect reproduces verbatim in the main phase after a
handful of ticks. Read/starvation/conversion numbers above describe an
error-terminated pump, NOT steady-state playback: marked PRE-I1 / ERROR-LIMITED.
No queue drops and no mailbox evictions/misses occurred before the failure.

## D. Playback baseline — 2160p (RED, PRE-I1 / ERROR-LIMITED)

Command: `pulsar-engine-lab --playback-metrics /dev/shm/pulsar-x1/fixture_2160p.mp4 300`
Exit status: **2**.

```text
media=fixture_2160p.mp4
ticks=300
wall_ms=79
main.requested_ticks=300
main.completed_ticks=4
main.healthy=0
main.error_message=duplicate timestamp arrived after that timestamp was presented
session.stale_event_count=0
session.control_accept_count=2
scheduler.starvation_count=1
scheduler.skipped_presentation_count=0
worker.read_request_count=1
worker.seek_request_count=1
worker.read_completion_count=1
worker.read_latency_p50_us=2
worker.read_latency_p95_us=2
worker.read_latency_max_us=2
worker.descriptor_latency_us=19598
worker.open_latency_us=21532
worker.queue_high_water=1
worker.events_dropped=0
worker.samples_dropped=0
worker.control_wait_count=0
mailbox.published=3
mailbox.evicted=0
mailbox.high_water=2
mailbox.current_size=1
harness.visual_misses=0
seek_epoch.available=1
seek_epoch.completed=0
seek_epoch.read_exercised=0
seek_epoch.healthy=0
seek_epoch.error_message=
seek_epoch.target_num=2
seek_epoch.target_den=1
seek_epoch.starvation_count=0
seek_epoch.skipped_presentation_count=0
seek_epoch.read_request_delta=0
seek_epoch.read_completion_delta=0
seek_epoch.mailbox_published_delta=0
seek_epoch.mailbox_evicted_delta=0
seek_epoch.mailbox_current_size=1
seek_epoch.visual_misses=0
stderr: Playback metrics failed: main playback entered error: duplicate timestamp arrived after that timestamp was presented
```

Same C2 reproduction. The seek epoch did not settle here (no reads issued
post-seek within budget on the heavier fixture); its zeros are INCOMPLETE data,
not a product verdict. Notable scale signal even pre-failure: descriptor
~19.6 ms and open ~21.5 ms at 2160p vs ~5.5/~7.4 ms at 1080p.

## E/F. D3 edit workload (independent of the C2 playback failure)

Temporary harness `/dev/shm/pulsar-x1/edit_metrics_probe.cpp` (NOT committed):
real `EditorSession` + real `FFmpegPlaybackWorker` factory + real probe on the
1080p fixture. Deterministic exact edits, each followed by
`synchronize_document_state()`: import, 2× add_clip, 3× move, 6× inward trim,
1× split, 1× delete, 4× undo, 2× redo, plus 2 settled no-ops (20
generation-changing ops).

Run 1:
```text
sync_performed_count=19
sync_skipped_count=2
sync_p50_us=14776
sync_p95_us=22281
sync_max_us=22281
worker_construction_count=15
worker_ctor_p50_us=6938
worker_ctor_p95_us=10635
worker_ctor_max_us=10635
descriptor_p50_us=6816
descriptor_p95_us=10533
descriptor_max_us=10533
edit_sequence_wall_ms=245
```

Run 2:
```text
sync_performed_count=19
sync_skipped_count=2
sync_p50_us=14905
sync_p95_us=24946
sync_max_us=24946
worker_construction_count=15
worker_ctor_p50_us=6804
worker_ctor_p95_us=9966
worker_ctor_max_us=9966
descriptor_p50_us=6699
descriptor_p95_us=9847
descriptor_max_us=9847
edit_sequence_wall_ms=256
```

Classification: **POTENTIALLY HARMFUL (borderline)**. Synchronize p95
(~22–25 ms) exceeds one 16 ms frame budget, and worker construction /
descriptor latency (~7–10 ms, ~70% of the sync cost) is the dominant portion —
i.e. synchronous `describe_source` on the GUI thread during edits. Stable
across runs. Skipped-path stays cheap (guard works). Verdict is evidence for
later optimization, not action now.

## G. Desktop diagnostics smoke

Command: `pulsar-desktop fixture_1080p.mp4 --smoke-exit-ms 1000
--dump-performance-on-exit` → QML fails to load in this environment
(`QtQuick.Window not installed`; pre-existing, unrelated to M1A/M1B). The flag
itself works end to end (exit 0, snapshot printed):
`sync_performed_count=1` with all worker/conversion/mailbox fields zero, since
no media ever played. This verifies the reporting path, not performance.

## H. Conversion latency

`conversion baseline: BLOCKED / INSUFFICIENT PRE-I1`. No successful desktop
preview conversions occurred (QML never loaded; engine-lab has no converter).
X2 repeats this under healthy playback. Nothing was manufactured to obtain a p95.

## I. X1 answers

1. Duplicate defect reproduces: **yes, verbatim, both resolutions, main phase**.
2. Phase: main playback within ~4–6 ticks; seek epoch healthy once on 1080p, unsettled on 2160p.
3. Queue drops before failure: **none** (events 0, samples 0, waits 0, high-water 1).
4. Mailbox evictions/misses before failure: **none** (high-water 2/8).
5. Latencies pre-failure: descriptor ~5.5 ms / open ~7.4 ms (1080p); ~19.6/~21.5 ms (2160p); single read ~2–8 µs on tiny fixture (observational, n=1).
6. Sync over edit workload: p50 ~14.8 ms, p95 ~22–25 ms, max ~22–25 ms.
7. Real workers constructed during edits: **15 of 19 syncs**.
8. Descriptor/constructor dominant: **yes** (~7–10 ms of ~15–25 ms sync).
9. D3: **POTENTIALLY HARMFUL (borderline)** — over a frame budget, worker-bound.
10. Blocked until I1: steady-state pump throughput, conversion p95, post-seek health on heavy media.

## Limitations

- Debug build, single machine, tiny synthetic fixtures; two D3 runs only.
- Fake-clock harness: counts are deterministic in control flow, wall/latency values observational.
- 2160p seek-epoch zeros are a harness-budget artifact, not signal.
- Desktop path unverified beyond flag plumbing (Qt environment).
- No conclusions about "fast" or "slow" — narrow evidence only.

## Deferred to X2

Repeat C+D+G+H on the same fixtures/commands after I1 and diff field by field;
expect `main.healthy=1`, sustained reads, and a real conversion p95.
