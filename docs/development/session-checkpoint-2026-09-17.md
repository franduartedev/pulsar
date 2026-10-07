> **HISTORICAL.** Internal checkpoint written on 2026-09-17 during milestone M9. It describes the
> development state of that day and is superseded by the current docs (see [`../README.md`](../README.md)).
> Kept only for traceability of the I1 fix reasoning.

# Session checkpoint — 2026-09-17

This checkpoint exists so the current PULSAR state can be resumed safely after shutting down the machine. The active development workspace was RAM-backed and will disappear on reboot.

## Repository state

- Repository: `franduartedev/pulsar`
- Branch: `feat/m9-cut-and-save`
- Last verified development commit before this checkpoint:
  `2eaf92216bb9370b71a8bfd3e6ea0fb8bb77dcfe`
  (`docs: reconcile x1 baseline interpretation`)
- X1 baseline document:
  `docs/performance/x1-pre-i1-baseline.md`
- All coherent product/instrumentation work through X1 is pushed to GitHub.


## Completed M9 remediation sequence

Completed and frozen:

- G1 — reset active clip on playback snapshot change.
- G2 — enforce global clip-ID uniqueness during restore.
- G3 — release playback read-in-flight state on matching cancellation.
- G4 — align preview image-provider/controller lifetime.
- F1 — distinct fatal playback-worker event semantics.
- M1A — bounded non-Qt playback diagnostics.
- M1A hardening — cooperative async-pump measurement, read completion count, visual-mailbox consumer simulation.
- M1A final calibration — explicit main/seek phase health and temporal-error reporting.
- M1B — desktop/edit/conversion performance instrumentation.
- X1 — pre-I1 baseline measured, documented, reconciled.

Important recent commits:

- `4a6c1345280e0ed6ee9e476e4ac33620aa111626` — F1 fatal worker semantics.
- `487703f46830fcd51aa3be65650063c913713a97` — bounded playback diagnostics.
- `13dc17d4d5033c8e7ffe6d91d6c6a704044c7e8c` — async-pump metrics hardening.
- `04b97340bbdb8eb618c503dfa7fd89618b1b4687` — surface metric phase errors.
- `ffa429182de9a5b74b2a6172effaa875cfd772fc` — desktop performance diagnostics.
- `747f33541da382d7acf5b27fee9a14c7da5cc072` — record X1 baseline.
- `2eaf92216bb9370b71a8bfd3e6ea0fb8bb77dcfe` — reconcile X1 interpretation.

## Confirmed C2 / I1 defect

X1 reproduced the same product defect on both controlled 1080p and 2160p H.264 fixtures:

`duplicate timestamp arrived after that timestamp was presented`

Confirmed path:

1. `FFmpegVideoDecodeSession::seek_to_presentation_timestamp_visual()` decodes a physical lookahead frame.
2. It returns that frame as seek lookahead and also retains it in `pending_visual_frame_`.
3. The next `decode_next_visual()` exposes the same physical frame again.
4. `FFmpegPlaybackWorker::to_sample()` currently gives each exposure a new ordinal.
5. The scheduler can therefore observe the same physical frame / same PTS as two distinct samples.
6. If the timestamp has already been presented, the scheduler correctly raises the duplicate-timestamp temporal error.

Do not weaken the scheduler to hide this. I1 must fix physical-frame identity / repeated exposure semantics.

## X1 baseline summary

Environment:

- Ryzen 3 3200G, 4 logical CPUs.
- 16 GB RAM.
- GTX 1650 present but not used by measured CPU paths.
- Debug build.
- FFmpeg CLI 6.1.1.
- Controlled synthetic H.264/yuv420p/30 fps fixtures.

Playback:

- 1080p: RED, exit 2, main failed after a handful of ticks with the exact C2 temporal error.
- 2160p: RED, exit 2, same exact C2 error.
- No worker queue drops before failure.
- No mailbox evictions or visual misses before failure.
- Pre-failure descriptor/open observations:
  - 1080p: roughly 5.5 ms descriptor / 7.4 ms open.
  - 2160p: roughly 19.6 ms descriptor / 21.5 ms open.
- Steady-state playback conclusions are blocked until I1.

D3 edit/synchronization:

- Actual generation-changing workload:
  import, 2 add, 3 move, 4 inward trim, 1 split, 1 delete, 4 undo, 2 redo = 18 document-generation changes.
- Constructor contributes the initial synchronization.
- Result: 19 performed synchronizations + 2 skipped no-ops.
- Two runs:
  - sync p50 ~14.8–14.9 ms
  - sync p95/max ~22.2–24.9 ms
  - worker ctor p50 ~6.8–6.9 ms
  - worker ctor p95 ~10–10.6 ms
  - descriptor p50 ~6.7–6.8 ms
  - descriptor p95 ~9.8–10.5 ms
- Worker construction/descriptor is roughly 40–50% of corresponding sync p50/p95 values.
- Descriptor latency is contained inside constructor time; do not add them together.
- Classification: POTENTIALLY HARMFUL (borderline), but no D3 optimization yet.

Conversion latency remains blocked/insufficient because the Qt environment could not run the real desktop preview path (`QtQuick.Window` unavailable).

## Performance policy — keep this invariant

PULSAR performance is a product constraint, not cleanup at the end.

Official rule:

> Design for performance from the beginning. Optimize with evidence.

For meaningful optimization work always answer:

1. What is slow or wasteful?
2. How was it measured?
3. What is the before result?
4. What changed?
5. What is the after result?
6. What trade-off was introduced?

Do not optimize structural paths because they merely look suspicious.

UI interactions should remain responsive; heavy decode/encode/proxy/waveform/effect work must not live on the UI thread. Significant caches must remain bounded, observable, and have explicit eviction policy. 8 GB RAM remains a supported target; 16 GB is the normal target. The low-end N4020/4 GB machine remains a future torture-test target for graceful degradation.

M12 remains the dedicated large performance milestone for proxies, bounded caches, decode/presentation pipeline work, hardware acceleration, memory pressure and responsiveness, but every earlier milestone must respect performance budgets.

## Multiplatform plan — parked, do not start yet

Keep architecture portable, but do not actively port now.

Sequence remains:

M9 close -> M10 Audio Foundation / A-V Sync -> M10.5 Editor Experience v0.2 -> M11 Export -> M12 Performance -> later Windows bring-up.

Windows is the second platform target. First goal there will be functional parity (open/play/edit/save/export) before platform-specific acceleration. macOS comes later when real hardware is available.

No Windows implementation work should start during current M9 remediation.

## Next exact step on resume

NEXT: **I1 — stable physical-frame identity**.

Do not reopen M1 instrumentation unless a confirmed instrumentation defect appears.
Do not optimize D3 yet.

I1 intent:

- physical decoded frame gets a stable session-lifetime identity at decode time;
- repeated exposure of the same physical frame reuses that identity;
- the worker must stop assigning fresh identity merely because a frame is emitted again;
- seek lookahead remains a peek/re-exposure contract unless explicitly changed;
- mailbox publication of the same physical identity should be idempotent;
- presentation logic must distinguish same physical frame repeated from a genuinely distinct later frame with the same timestamp;
- identity counter must not reset on seek;
- preserve application/domain boundaries and existing epoch/generation protections.

After I1:

NEXT: **X2** — rerun the same X1 fixture/workload and compare before/after field-by-field.

Primary X2 expectation:

- main playback becomes healthy on the controlled fixtures;
- exact duplicate temporal error disappears without weakening scheduler validation;
- read pump reaches sustained behavior;
- seek epoch becomes meaningful;
- then evaluate queue/mailbox/starvation/read latency honestly;
- desktop conversion p95 remains dependent on a usable Qt runtime unless that environment is fixed separately.

Only after X2 evidence should D1 or D3 optimization decisions be made.

## Wider M9 remediation sequence after X2

Planned order remains:

I1 -> X2 -> P0 repository error normalization -> P1 Save integration -> P2 Open/fresh-session replacement -> P3 dirty UI -> S1 clock-write centralization seam -> Z1 M9 acceptance gate.

Then proceed to M10 Audio Foundation / A-V Sync.

## Product direction to preserve

PULSAR:
- Linux-first.
- C++20 + Qt6/QML.
- FFmpeg in adapters, never leaking into domain/application.
- Exact time model; FPS is stream metadata, not document authority.
- Timeline/commands/persistence remain exact and deterministic.
- UI simple by default, power progressively disclosed.
- Performance and low-resource operation are differentiators.
- Do not add fake features only to make the UI look richer.
- Visual redesign milestone M10.5 occurs after audio and before export.
