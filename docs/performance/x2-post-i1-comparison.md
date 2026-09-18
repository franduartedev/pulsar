# X2 — Post-I1 Controlled Comparison

Date: 2026-09-18.
Measured commit: `7760f1f5ded676164d4ec588c0ef8a62616da375` (branch
`feat/m9-cut-and-save`, tree clean; includes I1 + identity-seek test hardening).
Reference: `docs/performance/x1-pre-i1-baseline.md` (commit `ffa4291`).

## Environment

Same machine and toolchain as X1: AMD Ryzen 3 3200G (4 logical), 16 GB RAM
(12 GiB free at measure time vs 7 in X1 — irrelevant), kernel 7.0.0-31,
GCC 13.3, FFmpeg CLI 6.1.1, GTX 1650 present but unused, Debug build.
No material difference; nothing normalized.

Build note (environment only, no repo changes): the system lacks
`libswscale` dev headers and Qt6, and the old sysroot is gone. For measurement
only, `/dev/shm/pulsar-x2/sysroot` provides a `libswscale` header shim with
accurate FFmpeg 6.1 declarations plus a `.pc` file, linked against the real
system `libswscale.so.7`. Product code, CMake files and flags are untouched;
`CMAKE_BUILD_TYPE=Debug` as in X1.

## Fixtures (same generation commands and file sizes as X1)

```sh
/usr/bin/ffmpeg -hide_banner -loglevel error -y -f lavfi \
  -i testsrc=duration=12:size=1920x1080:rate=30 \
  -pix_fmt yuv420p -c:v libx264 -preset veryfast \
  /dev/shm/pulsar-x2/fixture_1080p.mp4
/usr/bin/ffmpeg -hide_banner -loglevel error -y -f lavfi \
  -i testsrc=duration=12:size=3840x2160:rate=30 \
  -pix_fmt yuv420p -c:v libx264 -preset veryfast \
  /dev/shm/pulsar-x2/fixture_2160p.mp4
```

Sizes: 250247 B (1080p), 627414 B (2160p) — identical to X1.

## 1080p playback

Command: `pulsar-engine-lab --playback-metrics
/dev/shm/pulsar-x2/fixture_1080p.mp4 300`. Exit status: **0**.

```text
media=fixture_1080p.mp4
ticks=300
wall_ms=448
main.requested_ticks=300
main.completed_ticks=300
main.healthy=1
main.error_message=
session.stale_event_count=0
session.control_accept_count=2
scheduler.starvation_count=150
scheduler.skipped_presentation_count=1
worker.read_request_count=150
worker.seek_request_count=1
worker.read_completion_count=150
worker.read_latency_p50_us=2394
worker.read_latency_p95_us=2552
worker.read_latency_max_us=3543
worker.descriptor_latency_us=6538
worker.open_latency_us=8703
worker.queue_high_water=1
worker.events_dropped=0
worker.samples_dropped=0
worker.control_wait_count=0
mailbox.published=151
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
seek_epoch.read_request_delta=2
seek_epoch.read_completion_delta=1
seek_epoch.mailbox_published_delta=2
seek_epoch.mailbox_evicted_delta=0
seek_epoch.mailbox_current_size=1
seek_epoch.visual_misses=0
```

## 2160p playback

Command: same with `fixture_2160p.mp4 300`. Exit status: **2**
(seek epoch did not settle; see below).

```text
media=fixture_2160p.mp4
ticks=300
wall_ms=1368
main.requested_ticks=300
main.completed_ticks=300
main.healthy=1
main.error_message=
session.stale_event_count=0
session.control_accept_count=2
scheduler.starvation_count=152
scheduler.skipped_presentation_count=3
worker.read_request_count=150
worker.seek_request_count=1
worker.read_completion_count=150
worker.read_latency_p50_us=8776
worker.read_latency_p95_us=10704
worker.read_latency_max_us=12432
worker.descriptor_latency_us=19314
worker.open_latency_us=18617
worker.queue_high_water=1
worker.events_dropped=0
worker.samples_dropped=0
worker.control_wait_count=0
mailbox.published=151
mailbox.evicted=0
mailbox.high_water=4
mailbox.current_size=3
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
seek_epoch.mailbox_current_size=3
seek_epoch.visual_misses=0
stderr: Playback metrics failed: seek epoch not healthy
```

A repeat run showed the identical signature (main healthy 150/150, seek
epoch all-zero), so this is stable, not flaky: on 2160p no post-seek read is
issued within the harness budget and no error surfaces. Same incomplete-data
signature as X1's 2160p seek — not a new I1 regression, not a verdict.

## D3 edit workload (two runs, 1080p fixture)

Throwaway harness rebuilt outside the repo with the identical deterministic
sequence (import, 2 add, 3 move, 4 trim, 1 split, 1 delete, 4 undo, 2 redo;
19 performed + 2 skipped).

Run 1:
```text
sync_performed_count=19
sync_skipped_count=2
sync_p50_us=13001
sync_p95_us=18827
sync_max_us=18827
edit_sequence_wall_ms=224
worker_construction_count=15
worker_ctor_p50_us=6603
worker_ctor_p95_us=13741
worker_ctor_max_us=13741
descriptor_p50_us=6454
descriptor_p95_us=13280
descriptor_max_us=13280
```

Run 2:
```text
sync_performed_count=19
sync_skipped_count=2
sync_p50_us=13325
sync_p95_us=24935
sync_max_us=24935
edit_sequence_wall_ms=234
worker_construction_count=15
worker_ctor_p50_us=6494
worker_ctor_p95_us=14051
worker_ctor_max_us=14051
descriptor_p50_us=6358
descriptor_p95_us=13940
descriptor_max_us=13940
```

## Desktop / conversion

BLOCKED. No Qt6 in this environment, so `pulsar-desktop` cannot build and the
smoke + `--dump-performance-on-exit` path cannot run (X1 at least executed a
prebuilt binary far enough to print the zero-filled snapshot). Conversion
latency stays BLOCKED / INSUFFICIENT. Nothing was installed or fixed.

## X1 → X2 comparison (1080p)

| field | X1 | X2 | reading |
|---|---|---|---|
| main.healthy | 0 (duplicate-timestamp error ~tick 6) | 1, 300/300 ticks, no error | **C2 gone on 1080p** |
| exit status | 2 | 0 | correctness state |
| completed_ticks | 6, error-limited | 300 | X1 error-limited, not a baseline |
| worker.read_request_count | 1 (error-limited) | 150, all completed | sustained pump reached; no speedup % vs X1 |
| read_latency p50/p95/max | 8/8/8 µs (n=1) | 2394/2552/3543 µs | X1 n=1; X2 is first real distribution |
| descriptor/open latency | 5543/7355 µs | 6538/8703 µs | same order, noise range |
| queue drops/waits | 0/0/0 | 0/0/0 | unchanged |
| mailbox evicted/misses | 0/0 | 0/0, high-water 2/8 | unchanged |
| seek_epoch | completed=1, healthy=1 (narrow) | completed=1, healthy=1, deltas ≥1 | consistent |
| D3 sync p50/p95 | ~14.8 / ~22–25 ms | ~13.0–13.3 / ~18.8–24.9 ms | overlapping noise; no material change |
| D3 ctor/descriptor | ~6.9/~6.8, ~10–10.6/~9.8–10.5 ms | ~6.5–6.6/~6.4–6.5, ~13.7–14.1/~13.3–13.9 ms | same shape; D3 class stands |

## X1 → X2 comparison (2160p)

Main phase: X1 RED (C2 at ~tick 4) → X2 healthy 300/300 with 150/150
sustained reads, latencies p50/p95/max 8776/10704/12432 µs (first real
distribution; ~3.7× the 1080p p50 — resolution scaling now measurable).
Seek epoch: X1 all-zero incomplete → X2 all-zero incomplete (stable across
two runs); consistent with the same harness-budget limitation observed in X1 —
incomplete data, not a product verdict. Desktop/conversion: blocked in both.

## X2 decision: **A. I1 CONFIRMED BY CONTROLLED X2**

1. C2 gone on 1080p: yes (no duplicate error in 300 ticks + seek epoch).
2. C2 gone on 2160p main: yes (300/300 healthy, no error string anywhere).
3. Both main phases healthy: yes.
4. Intended tick workload completed: yes, 300/300 both.
5. Sustained reads: yes, 150 issued + 150 completed each.
6. Queue drops: none (0/0/0 both).
7. Mailbox evictions: none; misses: none.
8. Visual misses: 0 both.
9. Seek epochs: 1080p completed + healthy; 2160p incomplete-but-error-free (same as X1 signature).
10. Newly measurable: read-latency distributions, sustained pump counts, D3 unchanged-shape confirmation.
11. D3: no material change (noise-overlapping); classification stands.
12. Conversion still blocked by Qt: yes.
13. New correctness bug uncovered: **none** — full CTest 15/15 green, no new error strings, stale count 0, no drops.

## Limitations

Debug build; synthetic fixtures (same commands and sizes as X1; byte identity
not hash-verified); two D3 runs; fake-clock harness (counts
deterministic, wall/latency observational); 2160p seek data incomplete (same
signature as X1, no verdict);
no desktop coverage; no "fast/slow" claims — X1 throughput numbers were
error-limited and are not compared as performance.
