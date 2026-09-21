# PULSAR — Engineering Risk Register & Minefield Map

- Status: living document. Research only — no production code changed.
- Baseline: `186f01a` (competitive study) on branch `chore/pulsar-risk-register`.
- Primary sources: `docs/research/video-editor-competitive-study.md` (2026-09-21 snapshot)
  and the actual PULSAR repository (verified by reading code, not by assumption).
- Rule: do NOT copy competitor complaints into this register unless they plausibly
  apply to PULSAR. Each entry states its evidence strength honestly.
- Governing rule: **design early what is expensive to change later; measure before
  optimizing what can safely change later.**

---

## 1. Executive Summary

PULSAR's foundations are unusually sound for its stage: an exact rational time core,
explicit seek rounding, stream-origin plumbing, a strict presentation scheduler,
transactional atomic save, a strict project-codec parser, and value-semantic undo with
bounded depth. Most risks in this register are therefore **future credible risks**,
not current fires — with a small number of exceptions.

**Already observable in PULSAR today (fix/measure first):**

1. `Track::split_clip` rebuilds the track via N re-validated inserts → O(n²)
   (~22 s/op at 10,000 clips). Same family: linear overlap scan on every insert,
   linear lookup/duration, full-track copy per edit.
2. Every edit pays a full snapshot copy + full read-model rebuild (`rebuild_read_model`
   formats 3 strings/clip) + full QML `Repeater` re-instantiation behind one coarse
   `timelineChanged` signal. Selection alone rebuilds everything.
3. Waveform manager registry (`unordered_map` of every unique cache key) is never
   evicted on success — confirmed long-session memory growth vector.
4. No derived cache (present or future) has a limit, eviction policy, purge path,
   orphan sweeper, or size telemetry. One more cache type copies the pattern and the
   "900 GB cache" thread class becomes ours.
5. Video and audio compute timeline→stream mapping at different layers
   (video bakes origin into the session map; audio adds it per position) — a silent
   A/V offset bug on nonzero-`start_time` files after cross-clip seek with session reuse.
6. Preview color conversion (`sws_getContext` defaults) ignores the color metadata
   PULSAR carefully preserves — every preview frame is affected; export will inherit
   the mistake if it reuses the preview path.
7. No VFR detection or normalization anywhere (probe reads only `avg_frame_rate`).
   Phone footage — the dominant ingest — drifts by construction once trim/duration
   math assumes linear source time.
8. No autosave, no backups, no versioned save, no crash recovery. The entire
   unsaved-work window is exposed.
9. Media identity is path-only with no fingerprint and no relink path. Rename/move
   = permanent Offline; same-path content swap = silently accepted.
10. Preview `sws` convert + `memcpy` + provider scale run on the 16 ms UI tick;
    decode-worker `join()` also runs on the UI thread on clip switch.

**What this document is NOT saying:** PULSAR does not need a database, ECS, plugin
system, render farm, microservices, or a distributed scheduler. Every mitigation here
is scoped to the smallest change that preserves future options.

---

## 2. Method

- 8 parallel read-only audit agents, one per risk family, each instructed to cite exact
  file paths, function names, and line-level mechanisms, and to label speculation.
- Primary agent (this document) deduplicated overlapping findings (e.g. three agents
  independently found the unbounded-cache problem from different angles — merged),
  spot-verified load-bearing claims against the tree (`track.cpp` split loop,
  `waveform_cache_manager.cpp` registry, `desktop_controller.hpp` signals,
  `file_project_repository.cpp` rename, VFR/autosave absence by grep), prioritized by
  cost-to-fix-late, and wrote this register.
- Evidence strength per entry uses exactly one of: **Observed in PULSAR** /
  **Observed repeatedly in competitors** / **Strong architectural inference** /
  **Speculative**. Absence verified by grep is marked as observed absence.
- Impact: Low / Medium / High / Critical. Likelihood: Low / Medium / High.
  Cost-to-fix-later: Low / Medium / High / Very High.
- Timing: NOW / BEFORE INTERNAL ALPHA / BEFORE PUBLIC ALPHA / BEFORE BETA /
  BEFORE 1.0 / LATER / MONITOR. Decision: LOCK NOW / ADD GUARDRAIL NOW /
  MEASURE NOW / DESIGN BEFORE FEATURE / SAFE TO DEFER / WATCH ONLY.

---

## 3. Risk Register

### R-01 — Quadratic split rebuild

- ID: R-01
- NAME: `Track::split_clip` rebuilds the track via N re-validated inserts
- DOMAIN: Timeline scaling
- DESCRIPTION: `Track::split_clip` (`src/domain/src/timeline/track.cpp:105-141`)
  finds the clip, then builds an empty `Track replacement` and re-inserts all n clips
  one by one, each `insert_clip` running a full O(k) overlap scan plus `vector::insert`
  memmove. `Timeline::split_clip` adds one more full-track copy on top.
- FAILURE MODE: Single split latency grows ~quadratically; UI freeze of seconds at
  1k–10k clips per track. Reported signal: ~22 s/op at 10,000 clips.
- WHY IT HAPPENS: O(n) validation × n re-inserts = O(n²) rational comparisons +
  O(n²) `Clip` memmoves for what is logically a 1-element splice.
- USER IMPACT: High. Splitting is a core gesture; long-form/podcast sessions with
  thousands of splits become unusable.
- TECHNICAL IMPACT: High. Same rebuild pattern infects split-undo (`restore_split`).
- EVIDENCE: Code path cited above. **Observed in PULSAR** (known benchmark signal;
  harness comment `benchmarks/main.cpp:313-314` admits single-track 10k insert is
  "not reasonable for this local harness yet"). Competitor parallel
  **Observed repeatedly in competitors** (study §5.7: Resolve DB bloat, Premiere
  1000–5000-source collapse).
- CURRENT PULSAR EXPOSURE: Severe at scale; invisible at demo clip counts.
- EARLY WARNING SIGNAL: Split p95 at 1k clips > 50 ms; benchmark trend 100→1000
  superlinear.
- METRIC / DETECTION METHOD: Per-op wall time for split at 100 / 1k / 10k clips on a
  single track (harness currently skips this case — un-skip after fix).
- COST IF FIXED NOW: ~1 day (in-place splice; overlap safety already established by
  the strict-inside check).
- COST IF FIXED LATE: 1–2 weeks + compat shims once undo/restore and QML assumptions
  multiply after alpha. (Cost-to-fix-later: High.)
- DECISION NEEDED: MEASURE NOW (add the missing benchmark) + fix at the next
  timeline touch.
- DEADLINE / STAGE: NOW.
- MITIGATION DIRECTION: Replace rebuild loop with in-place splice: `find_if` once →
  compute left/right → `*it = left; clips_.insert(it+1, right)`. Keep `insert_clip`
  validation for genuinely new clips only.
- GUARDRAIL / INVARIANT: "Split/trim/move/remove performs ≤ 2 linear passes + ≤ 1
  vector memmove; never calls `insert_clip` per existing clip."
- TEST OR BENCHMARK: `benchmark_track_split(n)` at 100/1k/10k single-track; existing
  `timeline_model_test.cpp` split-order tests must keep passing.
- STATUS: Open. Impact: High. Likelihood: High. Evidence: Observed in PULSAR.

### R-02 — Linear overlap scan on every insert

- ID: R-02
- NAME: Every `insert_clip` scans all clips for overlap
- DOMAIN: Timeline scaling
- DESCRIPTION: `Track::insert_clip` (`track.cpp:17-21`) loops all existing clips for
  overlap before `lower_bound` + insert, so sequential build of n clips is O(n²).
  `Timeline::insert_clip` adds a global `find_clip` scan on top.
- FAILURE MODE: Slow project open / import / paste of many clips (the Resolve
  "paste-between-timelines slowness" class, study §5.7).
- WHY IT HAPPENS: Sorted order is not exploited; only neighbors around the
  `lower_bound` position can overlap a sorted non-overlapping array.
- USER IMPACT: Medium (felt at import/open, not per keystroke).
- TECHNICAL IMPACT: Medium.
- EVIDENCE: `track.cpp:17-27`. **Observed in PULSAR**.
- CURRENT PULSAR EXPOSURE: Moderate now; blocking at hundreds of clips per track.
- EARLY WARNING SIGNAL: `benchmark_track_insert(1000)` total ≫ 100× the 100-clip total.
- METRIC / DETECTION METHOD: Batch-build total ms for 100/1k clips; per-insert tail ms.
- COST IF FIXED NOW: Hours (check only predecessor/successor around `lower_bound`).
- COST IF FIXED LATE: Still cheap, but persisted-order assumptions may accumulate.
  (Cost-to-fix-later: Medium.)
- DECISION NEEDED: ADD GUARDRAIL NOW (sub-quadratic budget on the insert benchmark).
- DEADLINE / STAGE: NOW.
- MITIGATION DIRECTION: `lower_bound` first, overlap-test only adjacent clips; keep the
  exact error contract (existing overlap-throw tests must still throw).
- GUARDRAIL / INVARIANT: "Bulk insert of n sorted non-overlapping clips is O(n log n)
  or better."
- TEST OR BENCHMARK: Un-skip 10k single-track insert after fix; neighbor-overlap unit cases.
- STATUS: Open. Impact: Medium. Likelihood: High. Evidence: Observed in PULSAR.

### R-03 — Linear lookup, linear duration, resolver miss pays full scan

- ID: R-03
- NAME: `find_clip` / `find_clip_at` / `duration()` are all scans; resolver miss path
  calls `duration()` per query
- DOMAIN: Timeline scaling
- DESCRIPTION: No ID index, no cached duration, no interval structure.
  `Timeline::find_clip` is O(tracks × clips); `duration()` is O(tracks × clips); the
  resolver calls `duration()` on every gap query (`timeline_clip_resolver.cpp:52`).
  Resolver also scans assets linearly per resolve.
- FAILURE MODE: Per-hit-test / per-playhead resolve degrades linearly; scrubbing janks
  on large timelines; every gap query becomes a full-timeline scan.
- WHY IT HAPPENS: No index; duration recomputed instead of cached.
- USER IMPACT: Medium (scrub/playhead responsiveness at scale).
- TECHNICAL IMPACT: Medium.
- EVIDENCE: Functions cited above. **Strong architectural inference** for hot-loop
  impact (per-frame call frequency not fully traced); competitor pattern
  **Observed repeatedly in competitors** (study §6.7).
- CURRENT PULSAR EXPOSURE: Low at current clip counts; compounds with asset growth.
- EARLY WARNING SIGNAL: `benchmark_timeline_lookup(10k)` average rising; scrub FPS
  drop with N.
- METRIC / DETECTION METHOD: `find_clip` first/middle/last ms at 100/1k/10k;
  `duration()` ms at 10k (add both to harness).
- COST IF FIXED NOW: Cheap (cache duration; add benchmarks). Full index later.
- COST IF FIXED LATE: Index retrofit across snapshot copies and undo. (High.)
- DECISION NEEDED: MEASURE NOW; DESIGN BEFORE FEATURE for a real index.
- DEADLINE / STAGE: Measure NOW; index design BEFORE INTERNAL ALPHA (before features
  multiply call sites).
- MITIGATION DIRECTION: Cache `Track::duration_` / `Timeline::duration_` on mutation
  (all mutations go through `Track`/`Timeline` methods, so invalidation is local).
  Defer ID→position index until track counts grow; key it off a generation counter so
  snapshot copies stay correct.
- GUARDRAIL / INVARIANT: "`duration()` is O(1); resolver never calls O(n) `duration()`
  on the hot path."
- TEST OR BENCHMARK: Duration + hit/miss lookup series in harness; unit test that the
  duration cache matches after insert/remove/split/trim/undo.
- STATUS: Open. Impact: Medium. Likelihood: High. Evidence: Strong architectural inference.

### R-04 — Full snapshot copy + full read-model rebuild per edit

- ID: R-04
- NAME: Every edit copies the whole timeline and rebuilds all N view entries
- DOMAIN: Timeline scaling / UI invalidation
- DESCRIPTION: All `Timeline` mutations deep-copy one whole track vector; then
  `EditorSession::synchronize_document_state` snapshots the entire timeline + assets,
  `rebuild_read_model` clears `views_` and re-formats 3 time strings per clip, and
  `DesktopController::publishTimeline` rebuilds a `QVariantMap` per clip. A 1-field
  gain change pays O(n) three times over.
- FAILURE MODE: Per-edit cost stacks copy + format + QML churn; dominates at 500–1k
  clips; type-to-paint latency crosses 50 ms.
- WHY IT HAPPENS: No incremental view update, no dirty-range; snapshot-by-value on
  every edit path including undo/redo.
- USER IMPACT: High (every keystroke edit).
- TECHNICAL IMPACT: High.
- EVIDENCE: `timeline.cpp:38-184`, `project_document.cpp:230-234`,
  `editor_session.cpp:107-162`, `desktop_controller.cpp:121-152`.
  **Observed in PULSAR**. Competitor prescription **Observed repeatedly in
  competitors** (study §6.7: virtualized timeline + lazy thumbnails + bounded undo).
- CURRENT PULSAR EXPOSURE: Moderate; fine under ~100 clips, visible at 500–1k.
- EARLY WARNING SIGNAL: `EditorSyncDiagnostics` per-edit durations trending up;
  `views_.size()` churned per edit equals N instead of 1.
- METRIC / DETECTION METHOD: `synchronize_document_state` p50/p95 at 100/1k/10k clips.
- COST IF FIXED NOW: Days (incremental patch by affected `ClipId`; lazy time-text).
- COST IF FIXED LATE: QML model rewrite once delegates depend on full-list shape
  (weeks). (Very High.)
- DECISION NEEDED: ADD GUARDRAIL NOW (budget on sync duration) + DESIGN BEFORE
  FEATURE (virtualized model before per-clip thumbnails/waveforms land).
- DEADLINE / STAGE: Guardrail NOW; virtualization design BEFORE PUBLIC ALPHA.
- MITIGATION DIRECTION: Patch only affected entries; viewport-windowed `timelineClips`
  (visible range + overscan) once N passes threshold; format `time_text` lazily per
  visible delegate.
- GUARDRAIL / INVARIANT: "A single-clip edit rebuilds ≤ K view entries (small
  constant), not N."
- TEST OR BENCHMARK: Sync-duration harness at scale; unit test: single gain change ⇒
  exactly 1 view entry mutated (after mitigation).
- STATUS: Open. Impact: High. Likelihood: High. Evidence: Observed in PULSAR.

### R-05 — QML Repeater instantiates every clip delegate

- ID: R-05
- NAME: Non-virtualized timeline delegates (`Repeater` + 3 MouseAreas per clip)
- DOMAIN: Virtualized timeline
- DESCRIPTION: `VideoTrack.qml` uses `Repeater` over `controller.timelineClips`;
  each `ClipBlock` carries 3 `MouseArea`s plus hover/drag state. No `ListView`, no
  recycling, no `cacheBuffer`. QML object count and signal wiring scale with N.
- FAILURE MODE: Linear memory/startup/resize cost; selection change rebuilds all
  delegates; the Shotcut 28-track → 19 GB + freeze class (study §5.8) is the precedent.
  Adding per-clip thumbnails/waveforms without virtualization repeats the competitor
  failure exactly.
- WHY IT HAPPENS: `Repeater` is non-virtualizing by design, combined with R-06's
  single-signal invalidation.
- USER IMPACT: Medium now; Critical once thumbnails land without virtualization.
- TECHNICAL IMPACT: High.
- EVIDENCE: `VideoTrack.qml:8-24`, `ClipBlock.qml`, `TimelinePane.qml:43-52`.
  **Observed in PULSAR** (structure); at-scale projection is **Strong architectural
  inference**.
- CURRENT PULSAR EXPOSURE: Low (single track, small N) but structural.
- EARLY WARNING SIGNAL: Delegate count == clip count; frame drops on
  selection/resize at 500+ clips.
- METRIC / DETECTION METHOD: Delegate instances and QML memory at 100/1k clips;
  selection-change-to-paint ms.
- COST IF FIXED NOW: Design-only (cheap).
- COST IF FIXED LATE: Painful once delegates accumulate state (drag/preview offsets).
  (Very High.)
- DECISION NEEDED: DESIGN BEFORE FEATURE; WATCH ONLY for implementation until N > ~500
  or thumbnails land.
- DEADLINE / STAGE: Design BEFORE PUBLIC ALPHA; implementation BEFORE BETA.
- MITIGATION DIRECTION: Horizontal `ListView` with `cacheBuffer` + delegate recycling;
  model exposes windowed slice + stable `clipKey`; keep `Repeater` only under an
  explicit small-N fast path. Per-clip artwork lazy + bounded cache.
- GUARDRAIL / INVARIANT: "Visible delegates ≤ visible window + overscan constant,
  independent of N."
- TEST OR BENCHMARK: QML bench: delegate count at 2k clips ≤ window budget.
- STATUS: Open. Impact: High. Likelihood: Medium. Evidence: Strong architectural inference.

### R-06 — One signal invalidates everything

- ID: R-06
- NAME: Coarse `timelineChanged` invalidates clips + ruler + selection + undo state
- DOMAIN: UI invalidation
- DESCRIPTION: 8+ properties (`timelineClips`, duration text, ruler ticks, selection,
  gain/mute, `canUndo`/`canRedo`) all `NOTIFY timelineChanged`. Every `publishTimeline`
  emits it, and selection-only changes also call `publishTimeline` ⇒ full model rebuild
  + full `Repeater` re-evaluation on every click.
- FAILURE MODE: O(N) QML re-evaluation per selection; hover/selection jank; any future
  playhead-driven refresh through this path would nuke the track at 60 Hz.
- WHY IT HAPPENS: Single coarse signal instead of per-property signals / granular
  model roles.
- USER IMPACT: Medium (selection latency is the canary).
- TECHNICAL IMPACT: Medium.
- EVIDENCE: `desktop_controller.hpp:44-53`, `desktop_controller.cpp:121-162`.
  **Observed in PULSAR** (verified).
- CURRENT PULSAR EXPOSURE: Moderate.
- EARLY WARNING SIGNAL: Selection-change paint > 16 ms at 500 clips; more than one
  `timelineChanged` emission per single edit.
- METRIC / DETECTION METHOD: Signal emissions per user action (edit/select/seek);
  QML binding evaluations per selection change (QSignalSpy test).
- COST IF FIXED NOW: Hours (split NOTIFY signals per property).
- COST IF FIXED LATE: Days untangling QML bindings that assumed atomic refresh.
  (Medium.)
- DECISION NEEDED: ADD GUARDRAIL NOW (≤1 `timelineChanged` per edit; selection emits
  selection-only signal).
- DEADLINE / STAGE: BEFORE INTERNAL ALPHA.
- MITIGATION DIRECTION: Separate `clipsChanged` / `selectionChanged` / `rulerChanged` /
  `editActionStateChanged`; selection path updates flags in place; adopt
  `QAbstractListModel` with `dataChanged` roles when virtualizing (R-05).
- GUARDRAIL / INVARIANT: "Selection change emits selection signal only, zero
  `timeline_clips_` reallocations."
- TEST OR BENCHMARK: Signal-spy test per action (edit=1, select=0 clips-rebuilds);
  selection latency bench at 1k clips.
- STATUS: Open. Impact: Medium. Likelihood: High. Evidence: Observed in PULSAR.

### R-07 — Drag commits pay the full stack; failures rebuild too

- ID: R-07
- NAME: Trim/move commit runs domain + snapshot + rebuild; failed commits rebuild
- DOMAIN: UI invalidation
- DESCRIPTION: QML correctly commits on release (good), but each commit runs the R-04
  chain, and `moveTimelineClip` recomputes `publishTimeline` even on failure. If a
  future change wires live `onPositionChanged` preview through the command stack, it
  becomes per-tick O(n).
- FAILURE MODE: Every drop freezes past the N cliff; failed drops do wasted work.
- WHY IT HAPPENS: No lightweight preview/commit split at the stack level; no
  no-rebuild-on-failure rule.
- USER IMPACT: Medium.
- TECHNICAL IMPACT: Low now.
- EVIDENCE: `ClipBlock.qml` release-only commit (positive) + heavyweight commit chain.
  **Observed in PULSAR**; per-tick risk is **Strong architectural inference**.
- CURRENT PULSAR EXPOSURE: Low–moderate.
- EARLY WARNING SIGNAL: Drop-to-paint ms at 1k clips; failed drop still rebuilding
  (observable today).
- METRIC / DETECTION METHOD: Commit latency move/trim at 100/1k/10k.
- COST IF FIXED NOW: Hours (skip rebuild on failure; bench trim).
- COST IF FIXED LATE: Days retrofitting live-drag preview under load. (Medium.)
- DECISION NEEDED: ADD GUARDRAIL NOW.
- DEADLINE / STAGE: NOW.
- MITIGATION DIRECTION: Failed commit ⇒ error/status only, no `publishTimeline`;
  rule: "pointer-move preview is QML-transform-only; domain commits on release."
- GUARDRAIL / INVARIANT: "Zero domain/snapshot/rebuild work on pointer-move; failed
  commits perform zero model rebuilds."
- TEST OR BENCHMARK: Invalid move leaves `timeline_clips_` identity untouched with no
  `timelineChanged`; add trim to interactive-latency benchmark.
- STATUS: Open. Impact: Medium. Likelihood: Medium. Evidence: Observed in PULSAR.

### R-08 — Undo latency inherits edit latency; redo stack unbounded by convention

- ID: R-08
- NAME: Bounded undo memory, unbounded undo latency; redo never capacity-trimmed
- DOMAIN: Undo/redo scale
- DESCRIPTION: Memory is fine (256-entry cap, small value-semantic commands ≈ 50 KB).
  But `undo()`/`redo()` re-run the O(n) full-track-copy mutation, so undo freezes as
  long as the edit itself. `redo_stack_` is never capacity-trimmed and entries are
  copied (not moved) between stacks; `ImportAssetCommand` copies a whole `MediaAsset`
  per transfer.
- FAILURE MODE: "Undo is broken" perception on large projects; slow import-heavy
  sessions pay asset copies per history transfer.
- WHY IT HAPPENS: Value-semantic inverse design (good for correctness) without
  incremental apply; asymmetric trimming.
- USER IMPACT: Medium.
- TECHNICAL IMPACT: Medium.
- EVIDENCE: `command_processor.hpp:14,51-68`, `command_processor.cpp:125-160,262-266`;
  memory-bounded claim **Observed in PULSAR**; latency claim **Strong architectural
  inference** (no undo benchmark at scale — harness only covers add at 100/1000).
  Competitor analogue (undo bloat) **Observed repeatedly in competitors** (study §6.7).
- CURRENT PULSAR EXPOSURE: Memory-safe today; latency-exposed at scale.
- EARLY WARNING SIGNAL: Single-undo latency at 10k ≫ 16 ms; undo-after-split slower
  than undo-after-gain at same N.
- METRIC / DETECTION METHOD: Undo/redo ms at 100/1k/10k for add/remove/split/gain;
  history bytes at full capacity; `sizeof(Command)`.
- COST IF FIXED NOW: Free for latency (R-01 fix covers split-undo); 3 lines for
  redo-trim + moves.
- COST IF FIXED LATE: Redesigning inverse representation under a shipped history
  format. (High.)
- DECISION NEEDED: MEASURE NOW; SAFE TO DEFER any representation change.
- DEADLINE / STAGE: Benchmarks NOW; representation review BEFORE BETA (before any
  history persistence format freezes).
- MITIGATION DIRECTION: Keep `ExecutedCommand` shape; make restore paths use the same
  in-place splice as fixed split; trim `redo_stack_` to capacity; move entries between
  stacks.
- GUARDRAIL / INVARIANT: "Undo/redo latency ≤ 2× forward-op latency at same N; undo
  memory ≤ capacity × max-command-size (`static_assert sizeof(Command)` budget);
  derived UI/playback/cache state never enters undo."
- TEST OR BENCHMARK: Extend interactive-latency harness to split/remove/middle undo at
  10k; capacity test (300 edits ⇒ undo ≤ 256, redo ≤ 256).
- STATUS: Open. Impact: Medium. Likelihood: High. Evidence: Strong architectural inference.

### R-09 — No VFR detection or normalization (flagship media gap)

- ID: R-09
- NAME: Variable-frame-rate footage is ingested as if it were constant-frame-rate
- DOMAIN: VFR
- DESCRIPTION: Probe captures only `avg_frame_rate` → `nominal_frame_rate`
  (`ffmpeg_media_probe.cpp:16-36`); `r_frame_rate` is never read; no packet-duration
  variance analysis; no VFR flag on asset/probe/UI; no CFR normalization path. Grep for
  vfr/normalize/cfr in `src/` returns nothing. Playback tolerates VFR passively (PTS
  ordering) but every edit operation built on linear source-time mapping is wrong, and
  there is no import-time decision point.
- FAILURE MODE: Phone/screen-capture VFR footage plays with progressive A/V drift;
  trims, durations, and audio-gap math diverge from picture over minutes. Exactly the
  r/premiere drift threads (study §5.3). Study mandates "detect-and-normalize on
  import or drift is guaranteed" (§8, §6.3, §13-NEXT.6).
- WHY IT HAPPENS: `to_sample()` hard-codes `duration: std::nullopt`; scheduler
  reconstructs timing purely from successive PTS.
- USER IMPACT: Critical for target users (phone footage is the dominant ingest).
- TECHNICAL IMPACT: Very High — export, caching, proxy, and waveform all inherit CFR
  assumptions if built first (study §6.1: fix cost ×10 once baked in).
- EVIDENCE: Probe/worker code + negative grep. **Observed in PULSAR** (absence
  verified). Trigger rate **Observed repeatedly in competitors**.
- CURRENT PULSAR EXPOSURE: High, currently masked because export/trim-duration math
  does not exist yet — the cheap window is now.
- EARLY WARNING SIGNAL: First VFR phone clip in test corpus drifts > 40 ms over 5 min
  vs audio clock.
- METRIC / DETECTION METHOD: A/V divergence on known-VFR fixtures; probe
  VFR-detection precision/recall on corpus (phone, screen capture, OBS, webcam,
  social-media download).
- COST IF FIXED NOW: Days (detect + warn) + weeks (normalize design).
- COST IF FIXED LATE: Architectural retrofit across decoder/timeline/effects/export.
  (Very High.)
- DECISION NEEDED: DESIGN BEFORE FEATURE. Product decision required on strategy
  (native timestamp handling vs preflight warning vs transparent/proxy normalization
  vs explicit transcode) — do NOT choose blindly; decide on fixture evidence.
- DEADLINE / STAGE: Detection + surfacing NOW; normalization NEXT (BEFORE BETA,
  before export ships).
- MITIGATION DIRECTION: (1) Probe: capture `r_frame_rate`, min/max packet-duration
  spread, avg-vs-r divergence → `frame_rate_mode {CFR, VFR, Unknown}`. (2) Import:
  warn/badge VFR assets; refuse silent linear mapping. (3) Normalize via background
  CFR transcode or PTS-resampled presentation map under render-from-originals
  discipline.
- GUARDRAIL / INVARIANT: "No clip's source mapping assumes constant frame duration
  unless the asset is flagged CFR."
- TEST OR BENCHMARK: VFR fixture corpus; probe flags VFR; playback divergence bounded;
  post-normalization frame count == CFR expectation. Keep fixtures forever.
- STATUS: Open. Impact: Critical. Likelihood: High. Evidence: Observed in PULSAR.

### R-10 — Per-frame durations discarded; B-frame edge cases unhandled

- ID: R-10
- NAME: `VideoSample.duration` always `nullopt`; missing-PTS latches fatal session error
- DOMAIN: Media time
- DESCRIPTION: `FFmpegPlaybackWorker::to_sample` hard-codes `duration = nullopt`, so
  the scheduler's `explicit_sample_end` path is dead in production; end-of-clip hold
  relies on heuristics. Separately, one `nullopt` PTS (NOPTS frames in screen
  captures/concatenated MP4s) latches `temporal_error_` and drops everything after —
  correct-but-brittle. B-pyramid seeks decode unbounded frames with no cap.
- FAILURE MODE: VFR judder miscounted; last-frame freeze length wrong at clip
  boundaries; future export must re-derive durations the playback path threw away;
  broken-PTS files hard-error instead of degrading to gap/hold; seek latency spikes.
- WHY IT HAPPENS: Decoder has duration data but the adapter drops it at the boundary;
  error path treats recoverable degradation as fatal.
- USER IMPACT: Medium.
- TECHNICAL IMPACT: Medium (high for future export/VFR work that needs durations).
- EVIDENCE: `ffmpeg_playback_worker.cpp:258-272`,
  `presentation_scheduler.cpp:85-116,162-167`, `ffmpeg_common.cpp:108-115`.
  **Observed in PULSAR**.
- CURRENT PULSAR EXPOSURE: Medium.
- EARLY WARNING SIGNAL: Export implementation re-opening the demuxer for durations;
  session error rate on "wild" corpus; seek p95 on H.264/H.265 B-pyramid fixtures.
- METRIC / DETECTION METHOD: % of production samples with duration present
  (currently 0); seek latency budget (p95 < 300 ms 1080p); zero session-death on
  NOPTS fixture.
- COST IF FIXED NOW: Small (1-frame lookahead duration; bounded post-seek decode;
  gap-degrade policy).
- COST IF FIXED LATE: Export/VFR work duplicates demux logic. (High.)
- DECISION NEEDED: DESIGN BEFORE FEATURE (feed-forward for R-09/export); harden
  NEXT.
- DEADLINE / STAGE: Design NOW; implement NEXT.
- MITIGATION DIRECTION: Propagate optional duration from decode session (reuse the
  visual path's `pending_visual_frame_` lookahead); cap forward-decode-after-seek with
  async-refine fallback; convert missing-PTS from fatal-latch to bounded gap-degrade.
- GUARDRAIL / INVARIANT: "`VideoSample.duration` absent only when genuinely unknowable
  (pre-EOS tail), never by default."
- TEST OR BENCHMARK: CFR fixture durations == 1/fps; VFR durations match PTS deltas;
  NOPTS-injected fixture ⇒ PresentationGap, session survives.
- STATUS: Open. Impact: Medium. Likelihood: Medium. Evidence: Observed in PULSAR.

### R-11 — Video ignores stream origin, audio applies it

- ID: R-11
- NAME: Asymmetric timeline→stream mapping between video and audio
- DOMAIN: A/V sync
- DESCRIPTION: Audio maps `timeline → source` as `origin + clip_source_position`
  (`timeline_clip_resolver.cpp:33-38`). Video maps it as bare `clip_source_position`
  (`timeline_playback_coordinator.cpp:293-295`) while the playback-zero map adds origin
  once at session construction. On cross-clip session reuse, the video session keeps
  the OLD clip's origin map while audio recomputes per position.
- FAILURE MODE: Clips from nonzero-`start_time` media (MP4 edit lists, transport
  streams, concatenated captures): correct on first enter, wrong after cross-clip
  seek-with-reuse or audio-clock reanchor — intermittent A/V offset by the
  `start_time` delta. The most expensive support class ("sometimes out of sync after
  seeking", study §5.3).
- WHY IT HAPPENS: Origin applied at different architectural layers for A and V with
  no single shared mapping function.
- USER IMPACT: High (silent correctness bug; `start_time != 0` files are common).
- TECHNICAL IMPACT: High.
- EVIDENCE: Lines cited above; reuse branch seeks bare `clip.source_time` against a
  stale map (`playback_session.cpp:18-25`). **Observed in PULSAR** (code paths);
  manifestation **Strong architectural inference** (needs nonzero-origin multi-clip
  fixture).
- CURRENT PULSAR EXPOSURE: High-ish.
- EARLY WARNING SIGNAL: Two-clip timeline from same nonzero-origin file diverging
  after seek storm.
- METRIC / DETECTION METHOD: Post-seek A/V offset == 0 on nonzero-origin fixtures
  (e.g. `start_time ≈ 1.4 s`, negative-PTS-edge files).
- COST IF FIXED NOW: Days (unify mapping).
- COST IF FIXED LATE: Intermittent lip-sync hunt across export/proxy/transcription.
  (Very High.)
- DECISION NEEDED: ADD GUARDRAIL NOW (fixture + test); fix NOW/NEXT with R-14.
- DEADLINE / STAGE: NOW.
- MITIGATION DIRECTION: Single `timeline_to_stream(clip, position, origin)` used by
  both video `evaluate_clip` and audio target computation; make the session map
  updatable on reuse (or forbid cross-clip reuse when origin differs).
- GUARDRAIL / INVARIANT: "Video and audio stream targets for the same timeline
  position differ only by their respective stream origins, computed by one function."
- TEST OR BENCHMARK: Nonzero-origin 2-clip timeline + scripted seek storm; assert
  video PTS and audio block PTS converge within 1 frame every time. Keep forever.
- STATUS: Open. Impact: High. Likelihood: Medium. Evidence: Observed in PULSAR.

### R-12 — No AAC priming / encoder-delay compensation

- ID: R-12
- NAME: First ~1024 AAC samples treated as audible
- DOMAIN: A/V sync
- DESCRIPTION: Zero references to priming/`initial_padding`/gapless metadata in `src/`
  (grep-verified absence). Audio blocks are stamped with raw `best_effort_timestamp`;
  gross offsets are handled but not the standard 1024-sample AAC priming (~21 ms).
- FAILURE MODE: Systematic ~20 ms audio lead/lag on every AAC import (the dominant
  phone/MP4 audio codec); audible lip-sync error at dialogue onsets.
- WHY IT HAPPENS: FFmpeg `skip_samples` side data / decoder delay never consumed.
- USER IMPACT: Medium (below casual threshold, above quality bar for dialogue/music).
- TECHNICAL IMPACT: Medium (bakes into cached waveforms and user trims if late).
- EVIDENCE: Absence + `ffmpeg_audio_decode_session.cpp:370-374`.
  **Observed in PULSAR** (absence); prevalence **Observed repeatedly in
  competitors** (universal NLE need).
- CURRENT PULSAR EXPOSURE: Medium-high (nearly every AAC file affected).
- EARLY WARNING SIGNAL: AAC sine-onset fixture shows first-audible-sample offset vs
  video PTS.
- METRIC / DETECTION METHOD: A/V onset alignment ≤ 2 ms on AAC fixture with known
  priming.
- COST IF FIXED NOW: Days (consume side data + `avctx->delay` at open, trim priming).
- COST IF FIXED LATE: Baked into waveforms/trims; migration problem. (High.)
- DECISION NEEDED: DESIGN BEFORE FEATURE.
- DEADLINE / STAGE: NEXT (needs fixture work); design acknowledged NOW. BEFORE BETA.
- MITIGATION DIRECTION: At audio open, drop priming frames before first PTS stamp;
  record applied priming in asset metadata for waveform consistency.
- GUARDRAIL / INVARIANT: "Block PTS 0 == first audible sample, not first encoded
  sample, for AAC/MP3."
- TEST OR BENCHMARK: AAC fixture with known 1024 priming vs WAV control. Keep forever.
- STATUS: Open. Impact: Medium. Likelihood: High. Evidence: Observed in PULSAR.

### R-13 — Device-clock takeover without drift servo

- ID: R-13
- NAME: Video transport reanchored to raw device frame counter every tick
- DOMAIN: A/V sync
- DESCRIPTION: When audio runs, the coordinator reanchors video transport to the audio
  device clock unconditionally each `update()`; the clock derives from a
  processed-frames counter with no smoothing or deadband. Pause/resume re-trims via
  flooring math each cycle, random-walking sync.
- FAILURE MODE: Rate-error devices (Bluetooth, cheap USB, VM audio, 44100↔48000
  resample paths) inject unfiltered error into timeline position → video judder/hold
  oscillation; pause/resume loop walks sync.
- WHY IT HAPPENS: Hard Software↔AudioDevice clock switch with no PLL/deadband;
  unconditional reanchor.
- USER IMPACT: Medium (correct on good hardware; fragile on real-world audio).
- TECHNICAL IMPACT: Low–medium.
- EVIDENCE: `timeline_playback_coordinator.cpp:370-395,32-35`,
  `audio_playback_session.cpp:227-238`. **Observed in PULSAR**.
- CURRENT PULSAR EXPOSURE: Medium.
- EARLY WARNING SIGNAL: BT-headset playback present/hold oscillation; pause/resume
  sync walk ≠ 0.
- METRIC / DETECTION METHOD: Reanchor delta distribution; 100 ppm-skewed device
  fixture hold rate; pause/resume loop resync exactness.
- COST IF FIXED NOW: Moderate (deadband + slew).
- COST IF FIXED LATE: "Video stutters on my headphones" class; moderate. (Medium.)
- DECISION NEEDED: SAFE TO DEFER (design noted).
- DEADLINE / STAGE: NEXT/LATER.
- MITIGATION DIRECTION: Reanchor only when |audio − transport| > ½ frame; slew inside
  threshold; keep software clock as flywheel during underrun.
- GUARDRAIL / INVARIANT: "Transport reanchor requires |delta| > threshold; threshold
  and rate estimate are logged/observable."
- TEST OR BENCHMARK: Simulated 100 ppm fast device; pause/resume loop bit-exact
  resync test.
- STATUS: Open. Impact: Medium. Likelihood: Medium. Evidence: Observed in PULSAR.

### R-14 — Float lattice inside the exact time core; double escape hatches

- ID: R-14
- NAME: `StreamingAudioConverter` steps a `double` lattice; every time type offers
  `operator double()`
- DOMAIN: Media time
- DESCRIPTION: All timeline/source mapping is exact rational/`__int128`, but the
  resampler tracks position as `double next_position_` with double step, and
  `Rational`/`FrameRate`/`MediaTime` all expose `operator double()`. Any future
  UI/export consumer can silently route through double (the classic 23.976 NTSC
  off-by-one at export). Converter `reset()` vs `flush_tail()` correctness also
  depends on unenforced "fresh converter per segment" lifetime coupling; audio seek
  uses truncating `av_rescale_q` while video uses exact floor — the two can land on
  opposite sides of a frame boundary.
- FAILURE MODE: Sub-sample phase divergence on long streams (split-vs-contiguous
  convert differs at boundaries); NTSC frame-index off-by-one in future export;
  1-frame post-seek A/V offset on fine-grained time bases; click on seek if converter
  lifetime ever outlives the seek.
- WHY IT HAPPENS: Two exact systems bridged by a float lattice with no proven error
  bound; convenience conversions accrete at call sites; two implementations of
  "MediaTime → stream timestamp" with different rounding.
- USER IMPACT: Medium (long-form drift, seek clicks, future export accuracy).
- TECHNICAL IMPACT: High (every downstream feature inherits the mapping).
- EVIDENCE: `audio_converter.cpp:136-205`, `rational.cpp:71-73`,
  `frame_rate.cpp:16-18`, `media_time.cpp:212-214`,
  `ffmpeg_audio_decode_session.cpp:277-295` vs
  `ffmpeg_video_decode_session.cpp:272-282`. **Observed in PULSAR** (hatches exist;
  future misuse **Speculative**).
- CURRENT PULSAR EXPOSURE: Low today, growing with every new time consumer.
- EARLY WARNING SIGNAL: `static_cast<double>` on time types appearing outside tests;
  split-vs-contiguous convert divergence; audio/video seek targets differing for
  shared MediaTime.
- METRIC / DETECTION METHOD: Split-vs-contiguous byte-compare over ≥30 min streams
  (must be 0); NTSC `frame_at` round-trip over 24 h (must stay exact); cross
  time-base seek-target identity.
- COST IF FIXED NOW: Hours–days (integer lattice or proven bound + test; lint
  covenant; unify on one floor helper; document converter ownership).
- COST IF FIXED LATE: Frame-accuracy bug hunt across export. (Very High.)
- DECISION NEEDED: ADD GUARDRAIL NOW (double-use covenant + tests) + unify seek
  conversion NOW/NEXT with R-11 (one bug class: "two implementations of the same
  mapping").
- DEADLINE / STAGE: NOW.
- MITIGATION DIRECTION: Integer-lattice resampler (or formally bounded double +
  exact-equivalence test); double allowed only at device/clock boundary and display
  (rename to `to_display_seconds()`); single `media_time_to_stream_timestamp_floor()`
  helper for A+V; converter lifetime ⊆ output-open ⊆ clip segment by documented rule.
- GUARDRAIL / INVARIANT: "Split conversion is sample-exact equal to contiguous
  conversion for any block partition; timeline/source mapping stays rational; double
  conversions outside tests require justification comment."
- TEST OR BENCHMARK: Random-partition sine-sweep byte-compare; NTSC 24 h round-trip;
  440 Hz sine 50-seek click test; time-base cross-product floor-identity test.
- STATUS: Open. Impact: High. Likelihood: Medium. Evidence: Observed in PULSAR.

### R-15 — Preview color conversion ignores preserved color metadata

- ID: R-15
- NAME: `sws` defaults decide appearance; no export color policy exists
- DOMAIN: Color / pixel pipeline
- DESCRIPTION: The converter calls `sws_getContext(..., SWS_BILINEAR, nullptr,
  nullptr, nullptr)` — no `sws_setColorspaceDetails`, no range handling — converting
  any input to RGBA. The rich `FFmpegVideoImageDescriptor` (range, primaries,
  transfer, matrix, SAR) rides alongside but never influences conversion. Fixed
  bilinear with no dither flag; SAR captured but with no verified consumer; alpha
  semantics undocumented. No colorspace module exists besides this call site.
- FAILURE MODE: Limited→full expansion wrong (washed/crushed blacks on H.264, the #1
  consumer format); BT.601/709 matrix mismatch on mixed timelines (hue shift); HDR
  inputs silently clamped ("colors look wrong vs VLC" — a top editor complaint class,
  study §5.11); 10-bit banding; anamorphic aspect wrong. Worst: export inherits the
  preview path and "export looks different from preview" destroys trust (the classic
  NLE trust-killer; study §6.1 grade/crop-invariance rule).
- WHY IT HAPPENS: Metadata plumbing without conversion policy; sws version-dependent
  defaults decide output.
- USER IMPACT: High (every preview frame; reputation-level correctness).
- TECHNICAL IMPACT: High (preview/GPU/export triple-fork if not pinned now).
- EVIDENCE: `ffmpeg_video_decode_session.cpp:328-361`,
  `decoded_visual_video_frame.hpp:13-24`. **Observed in PULSAR**. Complaint class
  **Observed repeatedly in competitors**.
- CURRENT PULSAR EXPOSURE: High for SDR correctness; export risk latent (no export yet
  — cheapest moment to pin the contract).
- EARLY WARNING SIGNAL: Color-bar fixture (limited-range 709) preview RGB vs
  `ffmpeg -vf zscale` reference mismatch > 2 LSB.
- METRIC / DETECTION METHOD: ΔRGB vs reference on {601,709}×{limited,full} bars +
  2020/PQ smoke; preview-vs-export RGB identity once export exists.
- COST IF FIXED NOW: Days–weeks (explicit range+matrix policy + tests) + 1 page of spec.
- COST IF FIXED LATE: Dual-pipeline color fork; months. (Very High.)
- DECISION NEEDED: DESIGN BEFORE FEATURE (SDR policy NOW; HDR policy NEXT).
- DEADLINE / STAGE: NOW for SDR range+matrix + 1-page pipeline contract; NEXT for HDR.
- MITIGATION DIRECTION: Set `sws_setColorspaceDetails` from frame colorspace+range;
  define HDR policy (clamp-with-badge vs tonemap — pick one, badge it); carry the
  descriptor so export/GPU share the SAME policy function; scaler flags parameterized
  (default bilinear); `SWS_ACCURATE_RND`/dither for >8-bit; route SAR explicitly.
- GUARDRAIL / INVARIANT: "Preview RGB is a pure function of (pixels, range, matrix)
  shared with export; HDR inputs are badged, never silently clamped; preview
  converter is presentation-only and replaceable — export must never consume
  `FFmpegPreviewFrame.pixels`."
- TEST OR BENCHMARK: Fixture matrix run against every pixel path; all agree. HDR badge
  test. Conversion-time budget per scaler (already instrumented).
- STATUS: Open. Impact: High. Likelihood: High. Evidence: Observed in PULSAR.

### R-16 — No bounded global cache: every derived cache grows forever

- ID: R-16
- NAME: No limit, eviction, purge, orphan sweep, or size telemetry on any disk cache
- DOMAIN: Cache architecture
- DESCRIPTION: `find_or_create` only ever adds `.pwfc` files; nothing deletes, sizes,
  or ages them. Crash leaves `.partial.*` + `.publishing`; schema bumps orphan whole
  files; version/key mismatches return `nullopt` without deleting. No cache root, no
  quota, no XDG policy, no `purge()` API. `budgets.md` already mandates
  limit+eviction+observability; compliance is 0/3 for disk.
- FAILURE MODE: The exact "Resolve 900 GB / Premiere 100 GB+ / CapCut silent disk"
  thread class (study §5.2, §6.2, §13-NOW.4). Second cache type (thumbnails/proxies)
  copies the pattern and multiplies it.
- WHY IT HAPPENS: No shared cache layer; each subsystem will own an independent cache
  without a global policy.
- USER IMPACT: High (disk exhaustion; "find the cache folder" lore).
- TECHNICAL IMPACT: High (migration of N scattered dirs/formats later).
- EVIDENCE: `waveform_cache_manager.cpp:72-103`, `waveform_cache.cpp:157,260-335`,
  negative grep for evict/purge/quota/CacheDir. **Observed in PULSAR** (absence).
  Failure class **Observed repeatedly in competitors**.
- CURRENT PULSAR EXPOSURE: Low today (small `.pwfc` files); HIGH the moment a second
  cache family appears.
- EARLY WARNING SIGNAL: Cache-dir bytes growing monotonically across sessions;
  `.partial.*` files older than process start.
- METRIC / DETECTION METHOD: `cache_dir_bytes_total`, `cache_file_count`,
  `bytes_written_per_session`, `orphan_bytes`.
- COST IF FIXED NOW: Cheap (shared `CacheStore`: quota + LRU + `purge_orphans()` +
  size accounting) before the 2nd cache type lands.
- COST IF FIXED LATE: Migration + user-data cleanup tooling. (Very High.)
- DECISION NEEDED: ADD GUARDRAIL NOW — block new disk-cache types on using the shared
  primitive.
- DEADLINE / STAGE: BEFORE PUBLIC ALPHA (before any thumbnail/proxy/preview cache).
- MITIGATION DIRECTION: Single cache root + per-type quota + LRU by atime + startup
  sweep + versioned layout (`<root>/v<schema>/`) + one-click purge hook for future UI.
  (FNV-64 filename collision: harden opportunistically with a secondary hash —
  probability negligible, fix-while-touching.)
- GUARDRAIL / INVARIANT: "Every new `*cache*` path registers a limit + eviction
  policy (lint gate); purge never loses edits; render/export always from originals."
- TEST OR BENCHMARK: Fill-past-quota ⇒ oldest evicted, total ≤ quota; restart sweeps
  orphans; plant stale partial/publishing/old-version ⇒ init removes, valid retained.
- STATUS: Open. Impact: High. Likelihood: High. Evidence: Observed in PULSAR.

### R-17 — Waveform registry grows for manager lifetime (A7 signal confirmed)

- ID: R-17
- NAME: `WaveformCacheManager::Impl::registry` never evicts `Ready` entries
- DOMAIN: Cache architecture / memory budgets
- DESCRIPTION: Registry inserts one entry per unique `stable_text()` and only erases
  `Failed` entries on re-request. Each pins key string + reader impl (path, key,
  index vector). Re-imports/edits changing size/mtime allocate fresh entries while
  old ones stay pinned. (Shutdown itself is correct: `stopping` + cancel + join.)
- FAILURE MODE: Long sessions touching many clips/versions → monotonic in-memory
  growth; multi-version podcast sessions are the worst case.
- WHY IT HAPPENS: Dedup-by-identity correct for coalescing, but lifetime == manager
  lifetime with no LRU/weak-ref/cap.
- USER IMPACT: Medium (session bloat; RSS creep correlated with distinct assets).
- TECHNICAL IMPACT: Medium.
- EVIDENCE: `waveform_cache_manager.cpp:79-103,185-190`. **Observed in PULSAR**
  (verified — the A7 warning signal is real).
- CURRENT PULSAR EXPOSURE: Low per-entry (~hundreds of bytes), unbounded count;
  medium in long-form/multi-version sessions. (Not a product issue of `/dev/shm`
  worktrees — that path is development-only.)
- EARLY WARNING SIGNAL: `registry_size` growing while open-file count is flat.
- METRIC / DETECTION METHOD: `waveform_registry_entries`, `registry_bytes_estimate`.
- COST IF FIXED NOW: Small (LRU cap 256–1024 + `weak_ptr` for Ready + disk reopen on
  demand — open is cheap and validated).
- COST IF FIXED LATE: Session-bloat debugging under real workloads. (High.)
- DECISION NEEDED: ADD GUARDRAIL NOW (cap + test) with R-16.
- DEADLINE / STAGE: NOW (same milestone as R-16; before waveform UI wiring — manager
  is currently dormant in UI, the window is open).
- MITIGATION DIRECTION: LRU-capped registry, strong refs only for in-flight; evicted
  Ready re-opens from disk.
- GUARDRAIL / INVARIANT: "Registry size ≤ cap after K distinct keys; all still
  resolvable via disk reopen."
- TEST OR BENCHMARK: 10k distinct synthetic keys ⇒ size ≤ cap, no worker deadlock;
  RSS delta budget in benchmark.
- STATUS: Open. Impact: Medium. Likelihood: High. Evidence: Observed in PULSAR.

### R-18 — Fingerprint trusts size+mtime; path-bound identity misses on move

- ID: R-18
- NAME: Same-size rewrite serves stale waveform; rename/move regenerates + orphans
- DOMAIN: File fingerprinting
- DESCRIPTION: Key = `weakly_canonical(path)|size|mtime` + stream params. (a) In-place
  rewrite preserving size with coarse mtime (FAT 2 s granularity, network FS,
  mtime-preserving copies) yields identical keys for different content → stale
  waveform served as `Ready` with validation passing (payload internally consistent,
  wrong media). Existing test covers append-only (size changes), not same-size
  rewrite. (b) Path in identity ⇒ rename/move/copy regenerates fully + orphans old
  file (R-16). (c) Stream-param coupling (correct against misinterpretation) risks
  miss-churn if FFmpeg reports `time_base`/`start_timestamp` differently across
  versions — mismatch error names no field.
- FAILURE MODE: "Waveforms lie" distrust after overwrite-in-place (study §5.12);
  "cache never hits" + disk duplication after folder reorganize.
- WHY IT HAPPENS: `size+mtime` is a heuristic, not identity proof; no content sample;
  path ≠ content.
- USER IMPACT: Medium.
- TECHNICAL IMPACT: Medium.
- EVIDENCE: `waveform_cache.cpp:125-138`; `waveform_cache_test.cpp:149-159`
  (append-only gap). **Strong architectural inference** on mechanism; field drift
  **Speculative**.
- CURRENT PULSAR EXPOSURE: Medium-low (editors usually change sizes; same-size PCM
  bounces and folder reorganizes are real but occasional).
- EARLY WARNING SIGNAL: Waveform mismatch after "overwrite export with same
  settings"; miss-rate spike after folder moves; duplicate `.pwfc` count > asset count.
- METRIC / DETECTION METHOD: `fingerprint_stale_hit_count`,
  `duplicate_content_entries`, `cache_hit_rate`, per-field mismatch counts.
- COST IF FIXED NOW: Cheap (content sample in key) to moderate (move-aware reuse).
- COST IF FIXED LATE: Trust damage + per-format firefighting; full content-addressed
  redesign expensive. (High.)
- DECISION NEEDED: DESIGN BEFORE FEATURE — harden before first user-facing waveform
  release.
- DEADLINE / STAGE: BEFORE PUBLIC ALPHA.
- MITIGATION DIRECTION: Key += cheap content probe (hash of first/last N KB,
  verified lazily on open; O(1) I/O, never full-file hash on hot path). On miss, probe
  for same (size, sample, stream params) under a different path ⇒ hard-link/copy
  instead of regenerating (one copy, not one decode). Structured mismatch errors
  naming the field. Defer full content-addressed storage.
- GUARDRAIL / INVARIANT: "Same-size rewrite ⇒ key differs (spec test); rename ⇒
  re-request costs ≈ file copy, not full decode."
- TEST OR BENCHMARK: Same-size overwrite with forced-equal mtime ⇒ key differs, old
  cache not reused; rename ⇒ factory not called, single payload on disk; reference
  media key stability across FFmpeg versions.
- STATUS: Open. Impact: Medium. Likelihood: Medium. Evidence: Strong architectural inference.

### R-19 — Blind caches and budget-less memory

- ID: R-19
- NAME: No cache-system telemetry; `budgets.md` numbers have no code counterpart
- DOMAIN: Cache observability / memory budgets
- DESCRIPTION: Build *progress* hooks are good (`WaveformProgress`, `WaveformBuildReport`,
  `query_partial`), but zero *cache-system* telemetry: no hits/misses, bytes, dir size,
  evictions, orphans, collisions. (The in-memory visual mailbox's
  published/evicted/high-water counters are the pattern to copy.) Likewise no RSS
  tracker, per-subsystem byte budget, or pressure response — each component is
  reasonable, the sum exceeds patata-PC budgets under concurrency (the Shotcut-19 GB
  no-limiter class, study §5.8). Decoder block size into the builder is observed, not
  clamped; manager pending queue unbounded.
- FAILURE MODE: Cannot detect R-16/R-17/R-18 in the field; silent disk/RAM growth;
  death by a thousand bounded components.
- WHY IT HAPPENS: Counters never added at the 3 call sites; budgets doc has no code
  counterpart (`RSS|memory_budget|getrusage` grep: zero).
- USER IMPACT: Medium (invisible until crisis).
- TECHNICAL IMPACT: High (flying blind through proxy/thumbnail rollout).
- EVIDENCE: `waveform_cache.hpp:155-161` (no bytes/hit fields); `budgets.md:11-55`
  vs zero instrumentation. **Observed in PULSAR**.
- CURRENT PULSAR EXPOSURE: Medium (blind to growth until users complain).
- EARLY WARNING SIGNAL: "How big is the cache?" has no programmatic answer; RSS over
  400 MB idle / 1.5 GB simple-1080p in engine-lab runs.
- METRIC / DETECTION METHOD: `cache_hits/misses/regenerations/bytes_stored/
  orphan_bytes/registry_entries` per type + aggregate; `process_rss_bytes`,
  `per_subsystem_bytes`, `peak_input_frames`, `mailbox_high_water_mark`.
- COST IF FIXED NOW: Cheap (counters at 3 sites + snapshot struct; RSS + subsystem
  counters in engine-lab/benchmarks).
- COST IF FIXED LATE: Architectural retrofit under user pain. (High.)
- DECISION NEEDED: MEASURE NOW — instrumentation now, byte-budget enforcement with
  proxy/thumbnail work.
- DEADLINE / STAGE: NOW (instrumentation); enforcement BEFORE BETA.
- MITIGATION DIRECTION: `CacheStats{hit,miss,bytes,file_count,evicted,orphan,
  registry}` + per-request `hit: bool`, mailbox-style cumulative counters; surface in
  engine-lab/logs first, UI inspector later. Clamp decode block size; bound pending
  queue with backpressure (`Busy`, never unbounded growth).
- GUARDRAIL / INVARIANT: "Every cache operation updates counters; benchmarks assert
  `peak_pending_buckets`, `peak_input_frames`, and RSS delta within budget."
- TEST OR BENCHMARK: Scripted miss→build→hit→reopen-hit sequence asserts exact
  counters and `bytes_stored == Σ file_size`; oversized-block + 10k-request-flood
  tests assert clamps, not OOM.
- STATUS: Open. Impact: Medium. Likelihood: High. Evidence: Observed in PULSAR.

### R-20 — Workers couple to the UI thread; no shared execution budget

- ID: R-20
- NAME: Decode `join()` on UI thread; control-queue `wait()` on worker; probe
  head-of-line blocking; per-subsystem threads with no global cap
- DOMAIN: Background jobs / thread ownership / shutdown
- DESCRIPTION: (a) `FFmpegPlaybackWorker` dtor joins the blocking decode thread
  synchronously on the caller — the 16 ms UI tick path on clip switch/seek-teardown;
  no decode-interrupt token (`cancel_pending` clears slots only). (b) The bounded
  event queue (cap 8) makes *control* events `wait()` when full — the worker blocks
  until the UI drains it, inverting liveness (worker depends on UI tick). (c) The
  probe worker is single-slot, non-cancellable; a slow (network/4K) probe blocks all
  import feedback (results are correctly dropped by generation, but the slot is held).
  (d) Each subsystem self-threads (probe 1, decode 1/live clip, waveform pool N) with
  no shared cap — fine today, oversubscription on 2-core patata PCs once
  proxy/thumbnail/export workers arrive. No `QThread`/`QtConcurrent`/pool exists yet.
- FAILURE MODE: Scrub/clip-switch stalls UI for one decode+seek latency; shutdown
  hangs on bad media; under UI stall the worker deadlocks behind the full queue;
  future thread-count explosion starves playback.
- WHY IT HAPPENS: No async destroy/detach; control and sample lanes share one
  blocking queue; no cancellation token into FFmpeg; no central scheduler.
- USER IMPACT: Medium now (single clip, local files); High once scrub/multi-clip
  matters.
- TECHNICAL IMPACT: High.
- EVIDENCE: `ffmpeg_playback_worker.cpp:56-72,137-256`,
  `playback_worker_event_queue.hpp:18-32`,
  `timeline_playback_coordinator.cpp:366-368`, `desktop_controller.*` worker loop.
  **Observed in PULSAR**. Stall class **Observed repeatedly in competitors**
  (study §5.6/§5.7).
- CURRENT PULSAR EXPOSURE: Low now, load-bearing for every seek/clip-enter.
- EARLY WARNING SIGNAL: Tick p95/max rising; `worker_ctor_p95_us` / `read_p95` rising;
  `control_wait_count != 0`; UI freeze on seek with HDD/network media.
- METRIC / DETECTION METHOD: UI-tick duration ring (missing — add), worker-join
  latency, existing `read_latency_ring`/`events_dropped`/`high_water`/`control_wait`
  exported to `performanceSnapshot`.
- COST IF FIXED NOW: Cheap–moderate (reaper thread for deferred destroy; non-blocking
  control lane; generation-checked probe interrupt).
- COST IF FIXED LATE: Baked into every transition; virtualized timeline multiplies
  churn. (Very High.)
- DECISION NEEDED: DESIGN BEFORE FEATURE — fix before multi-clip/scrub-heavy
  milestones; central bounded executor (playback > audio > waveform > thumbnail
  priority) before proxy/thumbnail/export workers exist.
- DEADLINE / STAGE: NEXT (1–2 sprints); executor BEFORE PUBLIC ALPHA.
- MITIGATION DIRECTION: Destroy workers off UI thread (reaper); real decode-interrupt
  (avio interrupt callback + generation token); split sample lane (droppable) from
  control lane (newest-wins, never `wait` on worker); probe takes
  `atomic<bool>`/generation with bounded-time respect; one bounded executor with
  priorities + cancellation tokens everywhere (waveform's `cancel()` is the template).
- GUARDRAIL / INVARIANT: "No blocking `join()` on UI/control thread; worker-thread
  `enqueue` is wait-free/lock-bounded; probe respects generation within ~100 ms;
  workers never touch `ProjectDocument`/QML directly (`QueuedConnection` pattern)."
- TEST OR BENCHMARK: 100-seek storm asserts tick p95 < budget; destroy-while-decoding
  with slow fake worker; shutdown-with-blocked-decode; queue-full control-storm fuzz;
  double-`openPath` race (only second commits).
- STATUS: Open. Impact: High. Likelihood: Medium. Evidence: Observed in PULSAR.

### R-21 — Preview conversion runs on the UI thread

- ID: R-21
- NAME: `sws` convert + `memcpy` + provider scale inside the 16 ms tick
- DOMAIN: UI rendering / threading
- DESCRIPTION: `updateTick` → coordinator update (decode drain + audio fill,
  synchronous) → `FFmpegPreviewConverter::convert()` + row `memcpy` + `setImage` +
  `previewChanged` — all on UI thread. Provider then copies + smooth-scales under
  mutex on the request path. Study §7 + budgets ban heavy work on UI thread;
  `conversion_p95` is measured but not budgeted; no tick-duration ring exists.
- FAILURE MODE: 1080p+ convert+scale per frame at 60 Hz → tick overrun → playhead
  stutter, input latency on drag.
- WHY IT HAPPENS: No background convert; single `image://` invalidation is correct
  but the work is inline.
- USER IMPACT: Medium (fine for alpha stills, degrades with motion/4K).
- TECHNICAL IMPACT: Medium.
- EVIDENCE: `desktop_controller.cpp:58-63,88-89,330-333`,
  `preview_image_provider.cpp:8-19`. **Observed in PULSAR**.
- CURRENT PULSAR EXPOSURE: Medium.
- EARLY WARNING SIGNAL: `conversion_p95/max_us` approaching 8–16 ms; tick overruns.
- METRIC / DETECTION METHOD: Add tick-duration ring; budget convert < 4 ms, tick < 8 ms.
- COST IF FIXED NOW: Moderate (convert on worker, UI swaps `shared_ptr<QImage>`).
- COST IF FIXED LATE: Entrenched jank blamed on decoder. (High.)
- DECISION NEEDED: MEASURE NOW; offload before proxy/preview-scale work.
- DEADLINE / STAGE: NEXT perf pass (BEFORE PUBLIC ALPHA).
- MITIGATION DIRECTION: Convert on probe/worker or small convert pool; cache scaled
  sizes in provider; `FastTransformation` for scrub, smooth on settle (Lite coupling).
- GUARDRAIL / INVARIANT: "No FFmpeg convert/memcpy/scale on UI thread (profiling CI);
  `performanceSnapshot` includes the tick ring."
- TEST OR BENCHMARK: 1080p tick-budget test (p95); convert-off-thread test (UI thread
  never enters `convert`); provider set∥request concurrency test.
- STATUS: Open. Impact: Medium. Likelihood: High. Evidence: Observed in PULSAR.

### R-22 — Shutdown ordering residuals

- ID: R-22
- NAME: Timer outlives session in dtor order; mailbox order undocumented
- DOMAIN: Shutdown / cancellation
- DESCRIPTION: Mostly correct today (`main.cpp` constructs engine before controller
  so reverse destruction is ordered; candidate-mailbox swap on project open; waveform
  shutdown joins). Residuals: (a) controller dtor stops the probe worker but never
  `tick_timer_.stop()` — a queued timeout can fire mid-destruction and touch the
  session being destroyed (member order: worker → session → mailbox → timer).
  (b) Mailbox epoch/deactivate-before-join ordering is correct but undocumented — a
  future reorder resurrects stale frames. Audio affinity (`QtAudioOutput` owner ==
  UI tick today) is implicit; any future fill thread turns every call into a blocking
  UI round-trip, and off-owner destroy is `qFatal`.
- FAILURE MODE: Rare timing-dependent use-after-free on fast quit during playback;
  stale-frame resurrection after refactor; future audio-thread blocking cycle.
- WHY IT HAPPENS: Implicit QObject timer lifetime; affinity by construction thread,
  not asserted.
- USER IMPACT: Low (rare crash on quit).
- TECHNICAL IMPACT: Low now; medium if audio moves threads.
- EVIDENCE: `main.cpp:18-24,62-66`, `desktop_controller.hpp:148-153`,
  `qt_audio_output.cpp:101-146,458-487`. **Observed in PULSAR**.
- CURRENT PULSAR EXPOSURE: Very low.
- EARLY WARNING SIGNAL: Crash stacks in `updateTick` during quit; audio underruns /
  `DeviceUnavailable` flips.
- METRIC / DETECTION METHOD: Clean quit-under-load smoke (100 iterations, TSan);
  underrun count, pump adherence.
- COST IF FIXED NOW: Trivial (2 lines + comments + affinity asserts).
- COST IF FIXED LATE: Trivial too, but crash reports cost trust. (Low.)
- DECISION NEEDED: SAFE TO DEFER as scheduled work — harden opportunistically NOW
  (cheap).
- DEADLINE / STAGE: NOW (hygiene).
- MITIGATION DIRECTION: `tick_timer_.stop()` + disconnect first in dtor; document
  mailbox rule ("activate clears; deactivate(epoch) idempotent; deactivate before
  join"); `Q_ASSERT(on_owner_thread())` on audio backend calls; engine mutex never
  held across backend (already documented — enforce in review); keep all `AudioOutput`
  calls on owner thread, future fill via lock-free queue + `wake_pump`.
- GUARDRAIL / INVARIANT: "Dtor order test + timer-stopped assert; no `BlockingQueued`
  calls from any realtime path."
- TEST OR BENCHMARK: Quit-during-playback/probe smoke under TSan; mailbox
  deactivate-then-join vs join-then-deactivate equivalence; underrun→Starved→
  Recovering→Ready transition test.
- STATUS: Open. Impact: Low. Likelihood: Low. Evidence: Observed in PULSAR.

### R-23 — No autosave, backups, versioning, or crash recovery

- ID: R-23
- NAME: Single manual-overwrite save is the only durability story
- DOMAIN: Project durability
- DESCRIPTION: Only user-invoked `saveProject(path)` exists. No periodic autosave, no
  rotation, no FCP-style versions, no Shotcut-style autosave folder, no recovery
  dialog. Grep for autosave/backup/recover/journal outside research docs: zero. ADR
  0006 deferred journaling/autosave/recovery out of M3 and it never returned.
  `projectDirty` exists only as a title `*`; nothing acts on it. (What DOES exist is
  good: real atomic save — temp + `O_EXCL` + write + fsync + rename + dir-fsync with
  honest `CommittedDurabilityUncertain` — and a strict ordered-section parser with
  bounds + golden-fixture roundtrip. Do not regress either.)
- FAILURE MODE: Crash/power-loss/OOM between manual saves loses everything since last
  save; "user forgot / crash before first save" — exactly the Premiere 2024–26 loss
  threads (study §5.6). Resolve Live-Save class (overwrite-sole-copy corruption) is
  avoided by the atomic writer, but the dominant loss mode is untouched.
- WHY IT HAPPENS: No background saver, no dirty-timer, no sidecar.
- USER IMPACT: Critical (total loss of unsaved work; trust-defining before users
  commit real projects).
- TECHNICAL IMPACT: Medium (reuses existing atomic writer).
- EVIDENCE: `desktop_controller.cpp:201-219` sole save entry; negative grep.
  **Observed in PULSAR** (absence). Loss class **Observed repeatedly in
  competitors**. Gold standard (FCP continuous + versioned) in study §4.9/§13-NOW.7.
- CURRENT PULSAR EXPOSURE: Total for unsaved work; grows with session length
  (export/perf work lengthens sessions — do this first).
- EARLY WARNING SIGNAL: "Lost work after crash" reports; long-uptime dirty sessions.
- METRIC / DETECTION METHOD: Max work-loss window (time since last Durable save);
  target ≤ 60 s via autosave draft; zero loss of last *saved* state.
- COST IF FIXED NOW: Days (crash-safe sidecar reusing `FileProjectRepository::save`).
- COST IF FIXED LATE: Collides with relink/path semantics + retention UX once users
  habituate. (High.)
- DECISION NEEDED: DESIGN BEFORE FEATURE — before any export/perf work lengthening
  sessions.
- DEADLINE / STAGE: BEFORE INTERNAL ALPHA.
- MITIGATION DIRECTION: Autosave to a SEPARATE draft (never overwrite sole copy —
  study §6.8 lesson); reuse atomic writer for `<project>.autosave`/drafts dir; on open,
  newer-draft ⇒ Recover/Discard prompt with surfaced path (Shotcut lesson);
  retention policy; debounced/off-UI-thread snapshot-capture-then-write; must not
  mark clean. Plus: PID in temp names + startup temp sweeper + per-platform fsync
  abstraction for the Windows gate (R-35).
- GUARDRAIL / INVARIANT: "Autosave never touches the user file; only the
  captured-at-save-start `history_state_id` may be marked saved; never sweep user
  `*.pulsar`, only own temp pattern."
- TEST OR BENCHMARK: Kill -9 mid-edit ⇒ relaunch offers draft; draft-write failure
  never alters main file; edit-during-background-save fuzz ⇒ dirty stays true.
- STATUS: Open. Impact: Critical. Likelihood: High. Evidence: Observed in PULSAR.

### R-24 — Save/open are full synchronous GUI-thread serialization

- ID: R-24
- NAME: Full encode + fsync inline on the Qt GUI thread; open is full parse +
  O(n·m) validation
- DOMAIN: Save performance
- DESCRIPTION: Every save re-encodes the entire project (one `ostringstream`, sorted
  tracks+assets) with blocking write/fsync/rename/dir-fsync inline in `saveProject`.
  No incremental/delta path, no background write, no debounce. Open reads ≤128 MiB
  fully, tokenizes all lines, then re-validates with per-clip `find_if` over assets.
  (Positive: `save_project` snapshots first and marks the *captured* history ID — keep
  that; it is what makes a future background writer safe. Positive: derived/runtime
  state correctly excluded from persistence — no cache/undo/playback in the file.)
- FAILURE MODE: Study §5.7 "slow save/open" cliff at 1000s of clips; fsync stalls on
  slow media freeze the UI unboundedly.
- WHY IT HAPPENS: Deliberate simplicity (ADR 0014: linear search, no DB) — correct
  for now, architectural later.
- USER IMPACT: Medium at scale; negligible today.
- TECHNICAL IMPACT: Medium.
- EVIDENCE: `desktop_controller.cpp:201-219`, `project_codec.cpp:192-281`,
  `file_project_repository.cpp:40-53`, `project_document.cpp:58-148`. Caps (128 MiB /
  100k records) bound worst case, not latency. **Observed in PULSAR**.
- CURRENT PULSAR EXPOSURE: Negligible for tiny timelines.
- EARLY WARNING SIGNAL: Save latency p95 growth; GUI frame drop during save; record
  counts approaching caps.
- METRIC / DETECTION METHOD: Save/open wall-time p50/p95/max by project size; target:
  no GUI-thread block > 50 ms (instrument via `performanceSnapshot` pattern NOW).
- COST IF FIXED NOW: ~Zero (measure only).
- COST IF FIXED LATE: Background writer + generation fencing; moderate. (Medium —
  do NOT "fix" with a database.)
- DECISION NEEDED: MEASURE NOW; background save NEXT (same PR as R-23 fencing).
- DEADLINE / STAGE: Instrument NOW; background write NEXT (before
  virtualized-timeline scale work).
- MITIGATION DIRECTION: Snapshot-on-GUI → encode+write on worker → mark-saved with
  captured ID on completion; serialize concurrent saves; keep full-snapshot format
  (incremental unjustified). Open: hash-index assets by ID during decode when needed,
  validation strictness unchanged.
- GUARDRAIL / INVARIANT: "Edits during background save are never lost or mis-marked
  clean; validation strictness never weakens for speed."
- TEST OR BENCHMARK: Edit-during-save test (save N, edit, complete ⇒ still dirty);
  10k-clip synthetic open benchmark; validation-reject tests stay green.
- STATUS: Open. Impact: Medium. Likelihood: High. Evidence: Observed in PULSAR.

### R-25 — Schema v1 with strict-reject and no migration framework

- ID: R-25
- NAME: First v2 field strands files without a migration path
- DOMAIN: Project durability
- DESCRIPTION: `PULSAR_PROJECT 1` header; any other version rejected. Forward-compatible
  A0 audio-suffix pattern exists (optional trailing fields tolerated), but no
  version-dispatch, migration registry, or deprecation path. Golden v1 fixture pins
  the format (good).
- FAILURE MODE: First v2 field (roles, multicam, nested timelines) makes old builds
  reject new files and tempts `if version==` sprawl or silent field-dropping on
  downgrade in new builds.
- WHY IT HAPPENS: Correct strictness today, zero migration scaffolding.
- USER IMPACT: Medium (file stranded across versions).
- TECHNICAL IMPACT: High once v2 lands.
- EVIDENCE: `project_codec.cpp:201,322,373-402`; `project_v1_golden.pulsar`; zero
  migrat/upgrade/compat hits in `src/`. **Observed in PULSAR**.
- CURRENT PULSAR EXPOSURE: Zero (single version) — load-bearing at the next change.
- EARLY WARNING SIGNAL: First PR adding a record/field without a version plan.
- METRIC / DETECTION METHOD: v1 files open byte-identical on all future builds
  (golden test kept green).
- COST IF FIXED NOW: Trivial (one-paragraph version-policy ADR).
- COST IF FIXED LATE: Forked parsers, stranded user files. (Very High.)
- DECISION NEEDED: LOCK NOW (policy prose only).
- DEADLINE / STAGE: NOW; migration code LATER (when v2 needed).
- MITIGATION DIRECTION: Policy: unknown version ⇒ clean error naming file version;
  new version ⇒ explicit pure `migrate(v1→v2)`; keep golden v1 forever; encoder writes
  newest, decoder never guesses, no silent downgrade writes.
- GUARDRAIL / INVARIANT: "No silent downgrade writes; golden v1 decode test never
  deleted."
- TEST OR BENCHMARK: Existing golden v1 decode test + future golden v2 +
  downgrade-reject test.
- STATUS: Open. Impact: Medium. Likelihood: Medium. Evidence: Observed in PULSAR.

### R-26 — Evicted undo history strands the dirty flag

- ID: R-26
- NAME: Undo past the save point can never return to clean after eviction
- DOMAIN: Undo/save interaction
- DESCRIPTION: Dirty = `current_history_state_id != saved_history_state_id`. The
  256-cap trims oldest entries silently; if the entry that produced the saved ID is
  evicted, undo can never reach the saved state — permanent `*` with no way back
  except re-save. Covered tests stop at basic dirty/undo, not eviction-vs-saved.
- FAILURE MODE: "I undid everything, why still dirty?" — save-trust erosion.
- WHY IT HAPPENS: History IDs monotonic, never re-validated against stack contents.
- USER IMPACT: Low–medium.
- TECHNICAL IMPACT: Low.
- EVIDENCE: `command_processor.cpp:262-266`, `project_document.hpp:29-45`;
  test gap confirmed. **Strong architectural inference** (logic verified).
- CURRENT PULSAR EXPOSURE: Low (256 edits/session in alpha).
- EARLY WARNING SIGNAL: Dirty == true with `can_undo() == false`.
- METRIC / DETECTION METHOD: Reachability of saved ID in undo/redo stacks;
  dirty-accuracy acceptance test.
- COST IF FIXED NOW: Cheap (define semantics + test).
- COST IF FIXED LATE: Cheap too, but mental-model damage accrues. (Low.)
- DECISION NEEDED: SAFE TO DEFER — define with R-23 work.
- DEADLINE / STAGE: NEXT (with autosave).
- MITIGATION DIRECTION: Accept + document + test (eviction keeps dirty == true), with
  optional "history truncated — re-save to set baseline" hint. Pinning the saved
  entry against eviction only if user testing shows pain.
- GUARDRAIL / INVARIANT: "Never derive dirty from stack sizes or generation
  (generation is bumped by undo/redo too)."
- TEST OR BENCHMARK: 257 edits → save at 100 → undo-to-empty ⇒ still dirty +
  `can_undo()==false`; save ⇒ clean; undo once ⇒ dirty.
- STATUS: Open. Impact: Low. Likelihood: Medium. Evidence: Strong architectural inference.

### R-27 — Path-only media identity with no relink

- ID: R-27
- NAME: Move/rename = permanent Offline; same-path swap = silently accepted
- DOMAIN: Media offline / relink
- DESCRIPTION: `MediaAsset.source_locator` (path) is the ONLY persisted identity —
  no hash, size, mtime, or probe fingerprint. Load checks existence+readability only.
  No "locate file" action, no search-nearby, no bulk missing overview (reason string
  exists in model but is dropped before QML — only `missing: bool` reaches the view).
  `validate_new_clip` forbids editing on Unavailable media with no guidance.
  (Positive, load-bearing: Offline is non-destructive — clips/identity/timeline
  untouched, duration asserted — keep it.)
- FAILURE MODE: (a) Rename/move/drive-disconnect/external-drive or absolute-locator
  project move ⇒ permanent Offline with no fix path (study §5.1 relink-confusion
  class). (b) Same-path different-content ⇒ silently accepted (same-length swaps
  undetectable). (c) Same file twice ⇒ two assets, double probe cost.
- WHY IT HAPPENS: Hashing deferred (correct — keeps load probe-free); relink UI never
  built; absolute paths stored verbatim.
- USER IMPACT: High (any media reorganization orphans assets).
- TECHNICAL IMPACT: Medium–high (retroactive fingerprinting needs re-probe pass).
- EVIDENCE: `media_asset.hpp:39-72`, `project_codec.cpp:215-248`,
  `file_project_repository.cpp:55-76`, `editor_session.cpp:145-150`; zero relink
  hits. **Observed in PULSAR**.
- CURRENT PULSAR EXPOSURE: Moderate (single-asset sessions today).
- EARLY WARNING SIGNAL: Offline badges after folder reorganization; repeated opens
  without relink; sessions with ≥1 Unavailable asset.
- METRIC / DETECTION METHOD: Relink success rate; % sessions with ≥1 Unavailable;
  time-to-relink.
- COST IF FIXED NOW: Moderate (lightweight fingerprint fields — additive codec change
  like the A0 audio suffix; surface expected-path + reason in UI).
- COST IF FIXED LATE: High once libraries are large. (High.)
- DECISION NEEDED: DESIGN BEFORE FEATURE — fingerprint fields NOW-or-NEXT; minimal
  relink (per-asset locate + search-nearby) NEXT.
- DEADLINE / STAGE: Fingerprint BEFORE PUBLIC ALPHA; relink UI NEXT (BEFORE BETA).
- MITIGATION DIRECTION: Extend ASSET line with optional trailing fingerprint
  (size + mtime + probed duration; content hash deferred); mismatch ⇒ Unavailable
  with reason "file changed"; relink updates locator as undoable command preserving
  clip IDs; prefer-relative-paths on save-as/move; surface expected runtime path +
  reason in tooltip/panel; save-time absolute-path warning. Never auto-rewrite
  locators on load (offline-first trust, study principle 9); relink explicit only,
  never silent reinterpretation.
- GUARDRAIL / INVARIANT: "Fingerprint never alters edit truth (duration/IDs); load
  never auto-re-probes or rewrites locators."
- TEST OR BENCHMARK: Rename ⇒ Offline with reason; replace-with-different-bytes ⇒
  mismatch; relink ⇒ Available, clips intact, IDs stable; relative-move ⇒ Available.
- STATUS: Open. Impact: High. Likelihood: High. Evidence: Observed in PULSAR.

### R-28 — Single-clip audio pipeline; A9 must reuse clip geometry

- ID: R-28
- NAME: One decoder → one gain → one queue; no mix stage; embedded audio has no
  independent identity
- DOMAIN: Audio scale
- DESCRIPTION: `AudioPlaybackSession` is documented "one selected clip's audio
  pipeline": single `clip_`, single scalar `ClipGain` (−60/+12 dB) + mute, single
  bounded device queue. No track gain, summing bus, ducking, roles, envelope,
  loudness. Converter handles mono↔stereo only; >2 channels hard-fail (by design,
  documented). Embedded audio resolves through the SAME video `clip_source_position`
  (`timeline_clip_resolver` header: "canonical editorial mapping shared by video and
  embedded-audio playback"). Seek = full source recreate + device reopen + 250 ms
  refill — correct, but a seek/audition storm (scrub, Q/W preview, 191-SFX audition)
  means file-open + probe + seek churn with fd/device-click cost.
- FAILURE MODE: First overlap (dialogue + music, J/L-cut, SFX over video) has nowhere
  to go — second source silent, clicks, or clipping with no headroom policy. A9
  "independent audio" built as a parallel resolver diverges trim/split/ripple
  semantics ⇒ A/V drift twins disagreeing on gain/mute/seek (study §5.3
  coordinate-fragile class). Audition spam starves the timeline session's device.
- WHY IT HAPPENS: Mixing was never scoped into A5/M10; single-pipeline
  correctness-first design with no reuse layer.
- USER IMPACT: High at A9 (blocks "two sounds at once"); medium now.
- TECHNICAL IMPACT: High (A9 rework surface: session, engine queue accounting,
  persisted gain model).
- EVIDENCE: `audio_playback_session.hpp:76-77,128`, `audio_playback_session.cpp`
  seek/fill paths, `clip_gain.*`, `audio_converter.cpp:22-44`, zero
  duck/roles/stems/loudness/envelope hits in `src/`. **Observed in PULSAR**.
- CURRENT PULSAR EXPOSURE: Medium (single-clip preview correct; any overlap demo
  blocked).
- EARLY WARNING SIGNAL: Sounds audition routed through the timeline session
  (device re-open per preview); new "audio clip" struct with own offset math not
  calling `clip_source_position()`; 5.1 files hitting `UnsupportedChannels` as crash
  instead of message.
- METRIC / DETECTION METHOD: Max simultaneously audible sources (== 1, assert);
  seek-to-Ready latency histogram; file opens per audition; underrun growth under
  audition burst.
- COST IF FIXED NOW: ~1 day (mix contract spec + resolver pinning tests + audition
  debounce); full mix stage later.
- COST IF FIXED LATE: Desync-class bugs + persisted-project migration. (Very High.)
- DECISION NEEDED: LOCK NOW — freeze single-pipeline as A5 truth; A9 requires a
  design doc (where sum happens, whose clock wins, backpressure across N decoders)
  before code; `media_asset.cpp:20` audio-only rejection is the flagged A9 touchpoint.
- DEADLINE / STAGE: NOW (invariant + tests); mix design gates A9 kickoff; audition
  path BEFORE sounds browser lands.
- MITIGATION DIRECTION: A9 audio-only clips are `Clip`s resolved through the SAME
  `resolve_clip_at()` + `clip_source_position()` (locator may differ; geometry handling
  stays shared). Mix at float32 `AudioBlock` pre-convert with per-source linear gain;
  decide 5.1 now (explicit error vs fold-down — never silent); separate lightweight
  audition preview (debounced, never touching timeline session state; cap concurrent
  decoders like the waveform manager); future retained-decoder + intra-file seek only
  when locator unchanged.
- GUARDRAIL / INVARIANT: "Any new audio-resolve path round-trips through
  `clip_source_position()` (grep gate); no second `AudioOutput::open()` per audible
  source; derived UI/playback state never persisted."
- TEST OR BENCHMARK: Split ⇒ both halves' `audio_source_position` equals pre-split
  mapping; two-source sum + mute matrix; mono/stereo→device matrix; 6ch ⇒
  deterministic `UnsupportedChannels`; 20-previews-in-5s ⇒ no fd leak, no clock jump;
  engine-lab seek-series monotonicity.
- STATUS: Open. Impact: High. Likelihood: High. Evidence: Observed in PULSAR.

### R-29 — 191 sounds with no index, search, favorites, or lazy peaks

- ID: R-29
- NAME: Sounds pack ships as files first ⇒ findability collapse + decode-per-row trap
- DOMAIN: Sounds-pack scale
- DESCRIPTION: Study's explicit PULSAR lesson (§4.12, §6.5, §13-NOW.6): "191-SFX needs
  search + favorites + audition-in-place, not just a folder"; "single-bin hierarchy
  doesn't survive 1000s" (§5.9). Today: zero browser/index code (no `sound*`,
  `library`, `browser`, `index` for audio assets; pack mentions are research-only).
  The only waveform producer is full-file decode through a single-worker manager with
  unbounded registry, no quota, and per-query `flush()` + `file_size` + fresh
  `ifstream` — designed for few large timeline clips, not hundreds of tiny SFX rows.
  Benchmarks cover 1/10 min files, never 191×3 s.
- FAILURE MODE: Raw-folder dump ⇒ findability plateau at 191, collapse by ~1–2k;
  audition latency (full decode + device open per tap) kills the beat-cut loop;
  opening the browser decodes 191 files behind one worker, filling disk with `.pwfc`;
  owned-vs-bundled mixing confusion (Filmora/CapCut anti-pattern, study §10).
- WHY IT HAPPENS: Tagging + indexing + audition latency is the hard part (study
  §6.5); nothing starts it; timeline waveform pipeline misapplied to library rows.
- USER IMPACT: High (the pack's raison d'être — beat-cut/dialogue workflows).
- TECHNICAL IMPACT: Medium–high (re-tag 191→10k + migrate favorites if late).
- EVIDENCE: Negative grep (absence) + `waveform_cache_manager.cpp` single-worker +
  unbounded registry. **Observed in PULSAR** (absence). Demand **Observed repeatedly
  in competitors** (Filmora/CapCut beat/detect daily-use vs Kdenlive/Shotcut gap).
- CURRENT PULSAR EXPOSURE: Low (pack not shipped) — decision debt accrues if 191
  files land without an index.
- EARLY WARNING SIGNAL: 191 files land without `index.json`/tags/durations/peaks;
  QML directory listing presented as "browser".
- METRIC / DETECTION METHOD: Time-to-first-audible-preview (< 150 ms, cached peaks
  instant); keystroke→filtered-list < 50 ms at 10k rows; cache bytes per SFX second;
  rows decoded vs rows visible (≈ visible + prefetch).
- COST IF FIXED NOW: 2–3 days (static prebuilt sidecar index + virtualized list +
  single audition voice).
- COST IF FIXED LATE: Discovery UX rebuild + re-tag at 10k + favorites migration.
  (High.)
- DECISION NEEDED: DESIGN BEFORE FEATURE — index + search + favorites +
  audition-in-place ship WITH the 191, not after.
- DEADLINE / STAGE: BEFORE PUBLIC ALPHA (before pack integration; index format is the
  contract).
- MITIGATION DIRECTION: Static prebuilt index (id, path, duration, rate/ch,
  tags/category, favorite flag, precomputed peaks) — startup never probes 191 files;
  virtualized lazy rows; browser reads sidecar peaks, NEVER touches
  `WaveformCacheManager` (timeline import of an SFX builds the full pyramid on
  demand); one debounced audition channel with duck-under-timeline; favorites in user
  config (pack dir read-only; never overwrite sole copy); badge bundled-vs-user
  inline. Reserve extension points only (marker rail, role tag, envelope hook) for
  beat/duck/roles/loudness DSP — explicitly deferred, but `.pwfc` kSchema bump path
  kept tested so onset data never gets smuggled into v1 without migration.
- GUARDRAIL / INVARIANT: "Index read is O(1) open; `WaveformCacheManager::request()`
  calls from browser code == 0; startup file probing == 0; peaks-transient sanity
  (click track visible at all pyramid levels — no beat-blind thumbnails)."
- TEST OR BENCHMARK: 10k-row synthetic index (filter latency, memory cap,
  favorite round-trip); 191×2 s browser test (zero `generate_waveform_cache` calls,
  first paint within startup budget, scroll without decode); schema test (v1 opens
  after change).
- STATUS: Open. Impact: High. Likelihood: High. Evidence: Observed in PULSAR.

### R-30 — Export does not exist: lock its invariants before M11

- ID: R-30
- NAME: No encoder/muxer/queue/spec — every export trap is still avoidable
- DOMAIN: Export
- DESCRIPTION: Zero encode/mux grep hits in `src/`/`apps/` (only `ProjectCodec::encode`
  text serialization); `src/domain` zero export hits; viewer ADR explicitly excludes
  export; checkpoint parks export at M11. Above-decode error reporting is
  strings-only (`optional<string>` in playback sessions; string-only
  `ProjectRepositoryError`) while adapters have coded errors — strings cannot drive
  hw-fallback, disk-full retry, or log-expander UX. Audio decode normalizes to
  float32 native rate/channels with no resample/rematrix — export's mixed-rate/5.1
  policy is undeclared.
- FAILURE MODE: The full study §5.11/§6.6 catalog if bolted on live: Resolve
  live-linked queue mutation, CapCut fps-mismatch artifacts, wrong-codec
  pixelation/oversize, Shotcut exit-code-3/hw-encoder failures, silent disk-full
  partial files, proxy leaking into export, silent 5.1 fold-down, resample drift.
- WHY IT HAPPENS: No snapshot semantics, no match-source default, no preflight, no
  coded errors — yet.
- USER IMPACT: Critical once export ships (trust-defining); zero today.
- TECHNICAL IMPACT: Very High (retrofitted snapshot = Resolve-class bug).
- EVIDENCE: Negative greps + ADR-0013 + checkpoint. **Observed in PULSAR**
  (absence). Every failure mode **Observed repeatedly in competitors**.
- CURRENT PULSAR EXPOSURE: Zero runtime; maximum design exposure — every
  timeline/playback decision made now becomes implicit export contract.
- EARLY WARNING SIGNAL: Any PR introducing "render" that reads the live
  `ProjectDocument`, reuses `FFmpegPreviewFrame` RGBA as engine frames, hardcodes
  1080p30/x264, or returns `optional<string>` errors for media failures.
- METRIC / DETECTION METHOD: `domain/` headers mentioning export/render/job == 0
  (keep at 0 until M11 design); then: queued-job-from-snapshot coverage 100%;
  preflight checklist (disk/fps/hw-fallback/audio-layout) each with forced-failure
  test; % user-visible failures with stable machine code + action hint (100%).
- COST IF FIXED NOW: 1 ADR + snapshot struct stub + error-code enums (cheap).
- COST IF FIXED LATE: Live-queue corruption, tutorial-lore support load,
  string-matching fallback logic. (Very High.)
- DECISION NEEDED: LOCK NOW — M11 acceptance criteria as prose before any encoder code.
- DEADLINE / STAGE: NOW (M9 close / M10 start).
- MITIGATION DIRECTION: (1) Export renders from an IMMUTABLE timeline snapshot, never
  the live document; preview pixels never feed the encoder (`domain` carries no
  `AV*`/`QImage`/preview buffers). (2) Match-source res/fps defaults; software encode
  default; HW strictly opt-in per-job with automatic software fallback + visible
  notice (never hw-on-by-default). (3) Preflight (disk/fps/hw-fallback) +
  plain-language errors + raw-log expander. (4) Decoupled background/batch
  (multi-aspect) queue — live edits never mutate queued jobs (reuse
  `project_snapshot` + epoch stale-rejection). (5) `ExportErrorCode`/`PreflightErrorCode`
  (disk-full, fps-mismatch, hw-unavailable-fell-back, codec-unsupported,
  corrupt-clip, cancelled) — adapter pattern: code public, `av_strerror` text
  diagnostic-only. (6) Explicit audio resample + channel-map policy (no silent
  downmix; `can_convert==false` ⇒ export error). (7) Export color spec independent of
  SDR preview (R-15); unsupported HDR errors loudly. (8) VFR→CFR policy decided
  before M11 (R-09).
- GUARDRAIL / INVARIANT: "Queued job carries snapshot/revision, never a live
  document ref (type-level if possible); purge/mode-switch determinism tests;
  mutate-after-queue ⇒ queued bytes unchanged; disk-full injection ⇒ plain-language
  error, no partial masquerading as success; no `throw runtime_error{userSentence}`
  in new media code."
- TEST OR BENCHMARK: Queue-2-jobs → edit mid-encode ⇒ outputs equal pre-edit snapshot
  renders; per-code forced-failure units; mixed-rate + 5.1 fixture exports (duration
  accuracy + policy); HW-absent leg ⇒ software fallback with logged code.
- STATUS: Open. Impact: Critical. Likelihood: High (if built without gate). Evidence: Observed in PULSAR.
- NOTE: This entry consolidates codec-environment and error-model findings that have
  no other home; dependency pinning (FFmpeg version floor, `PULSAR_BUILD_FFMPEG=OFF`
  preservation) and cross-platform seams are tracked in R-35.

### R-31 — Proxy truth separation not codified

- ID: R-31
- NAME: No proxy model; render-from-originals is convention, not rule
- DOMAIN: Proxies
- DESCRIPTION: No proxy type, generation, cache, relink, or truth/proxy distinction
  (ADR-0007: "sin proxies, cachés"). Checkpoint assigns proxies to M12. Before
  proxies exist, the traps are known (study §5.1, §6.1): proxy drift, wrong timebase,
  duration mismatch, incorrect audio, color shift, stale proxy, export accidentally
  using proxy, 700 MB→5.6 GB transcode surprise, silent disk growth.
- FAILURE MODE: Grade/crop shift between proxy and export; transcode bloat;
  unlinked-file traps; "export used the proxy" corruption.
- WHY IT HAPPENS: Proxy correctness spans decoder→timeline→effects→export; easiest
  to guarantee if the split is architectural from day one.
- USER IMPACT: High once proxies ship; zero today.
- TECHNICAL IMPACT: Very High ("900 GB cache" + export-from-proxy class).
- EVIDENCE: Zero proxy hits in `src/domain`; checkpoint M12. **Observed in PULSAR**
  (absence). Traps **Observed repeatedly in competitors**.
- CURRENT PULSAR EXPOSURE: None now; high later.
- EARLY WARNING SIGNAL: Any thumbnail/peak/waveform cache added without
  limit+eviction+observability (violates budgets); any "render" reading proxy paths.
- METRIC / DETECTION METHOD: Every cache: limit + eviction + observability +
  one-click purge; proxy-edit RAM (Chill ~700 MB target); mode-switch determinism.
- COST IF FIXED NOW: One invariant sentence + budgets extension (free).
- COST IF FIXED LATE: Cache-bloat migration + export-corruption class. (Very High.)
- DECISION NEEDED: LOCK NOW.
- DEADLINE / STAGE: NOW (extend cache rule to explicit proxy-truth ADR before M10.5
  waveform/thumbnail work).
- MITIGATION DIRECTION: Absolute invariant — "proxy may change preview cost, must
  NEVER change editorial truth; final export uses intended source unless explicitly
  configured otherwise." Coupled Preview+Proxy single default (540p-class) on first
  import + visible state + async replace keeping trims; Lite couples
  preview/proxy/waveform-res/effects, truth invariant (R-32 table).
- GUARDRAIL / INVARIANT: "Delete entire proxy cache mid-project ⇒ timeline hash +
  export hash unchanged; Lite↔Pro toggle ⇒ resolved ranges bit-identical;
  purge-never-loses-edits."
- TEST OR BENCHMARK: Proxy-purge invariance; mode-switch determinism (same suite as
  R-32).
- STATUS: Open. Impact: High. Likelihood: High (if built without gate). Evidence: Observed in PULSAR.

### R-32 — Profiles and settings: truth-variance, sprawl, oscillation

- ID: R-32
- NAME: Auto/Lite/Balanced/Pro + configuration explosion + Auto thrash
- DOMAIN: Configuration / adaptive performance
- DESCRIPTION: No settings store, profile enum, adaptive controller, or snap/ripple
  state machine exists (exhaustive grep: zero hits — good, no debt yet). The dangers
  are ordered and known: (a) Lite accidentally changing semantics/sync/export source
  (the #1 profile risk); (b) each feature adding its own toggles until first-10-min
  needs a manual (Kdenlive/Shotcut "proxy vertical res / MLT sharp edges" trap,
  study §5.1; wrong preset ⇒ "expensive mess later" at export); (c) hidden/buried
  states regressing silently (Premiere 2026 dynamic-ripple backlash, Filmora 14.3.2
  snap regression, Shotcut hw-on-by-default exit-code-3 — study §5.5); (d) Auto
  flapping Lite↔Pro without hysteresis (no competitor does Auto well — PULSAR-novel,
  no proven pattern); (e) background pipelines (proxy/waveform/thumbnail/export)
  landing on the UI thread or starving playback (Shotcut rebuild lag, 19 GB freeze,
  Resolve foreground caching).
- FAILURE MODE: Silent export divergence (trust-catastrophic); manual-sized learning
  curve; behavior-change backlash; visible quality pumping + proxy-regen storms;
  editing blocked by background work.
- WHY IT HAPPENS: Cost policy and edit truth share structs; noisy load signals drive
  a threshold controller without dwell; background work added without owner/queue/
  backpressure discipline.
- USER IMPACT: High (a–e all user-visible).
- TECHNICAL IMPACT: High.
- EVIDENCE: Study §§5.1,5.5,6.1,7,13–14 (patterns [V+U], high); zero settings code
  (strong negative — the precedent-setting PR hasn't happened). **Strong
  architectural inference** for PULSAR exposure timing.
- CURRENT PULSAR EXPOSURE: None — legislating window is open.
- EARLY WARNING SIGNAL: Second independent proxy/preview/quality toggle in a
  different panel; ripple/snap implemented in QML without domain coverage; Auto PR
  with bare `if (load > X)` and no dwell constants; background PR with synchronous
  decode in controller or unbounded queue.
- METRIC / DETECTION METHOD: User-facing perf/correctness toggle count (target: ONE
  coupled switch at first import); snap/ripple/link transition coverage 100% in
  domain; mode switches/min during steady load (0 after settle); budgets (startup
  <2 s, idle <400 MB, simple-1080p ≤1.5 GB, Chill ~700 MB) + frame drops during
  background load; queue depth/drop counters.
- COST IF FIXED NOW: ~Free (sentences + table + spec paragraph).
- COST IF FIXED LATE: N×M toggle migration; thrash bugs are field-only; background
  architecture retrofit. (Very High.)
- DECISION NEEDED: LOCK NOW the truth-vs-cost table + coupled-switch rule +
  hysteresis spec (one paragraph); do NOT build a generic settings framework now.
- DEADLINE / STAGE: NOW (legislate); manual modes NEXT/M12; Auto LATER (after manual
  modes proven).
- MITIGATION DIRECTION: Modes live strictly in application/adapters; `domain` never
  reads mode (CI grep fails on `*Mode*|*Profile*|*Quality*` in `pulsar_domain`).
  Truth-vs-cost table (frozen):

  | Concern | MAY vary with profile | MUST NEVER vary |
  |---|---|---|
  | Preview resolution/scale | Yes (Lite 540p-class) | — |
  | Proxy vs original for playback | Yes | Export/render source (always originals) |
  | Waveform peak resolution | Yes (degrade path) | Waveform timing/sync conclusions |
  | Thumbnail density/lazy load | Yes | Clip bounds, gaps, links |
  | Effect preview quality/bypass | Preview-only, badged | Export effect stack/intent |
  | Background concurrency | Yes | Autosave versioning/recovery completeness |
  | HW decode on/off | Yes, with fallback | Frame-accuracy of seek/cut |

  Snap/ripple/link/gap-intent as explicit undoable domain state from the start
  (cheap now per study §13-NOW.1/3, expensive to retrofit); default changes require
  opt-in + changelog. One owning `ProxyPolicy` value-type + one Performance panel.
  Auto = slow controller: separate up/down thresholds (hysteresis band), minimum
  dwell (≥10 s), cooldown after proxy regen, manual override latches and sticks,
  never on UI thread, telemetry log of switches. Every background pipeline gets owner
  thread + bounded queue + backpressure (coalesce/seek-replace) + playback priority —
  reuse the playback-worker generation/epoch + single-pending-command coalescing
  template.
- GUARDRAIL / INVARIANT: "Same project × 4 modes ⇒ byte-identical timeline
  serialization + identical export-source IDs (only preview/cache stats differ);
  no new perf toggle without named owner struct + stated export-truth effect
  ('none — render from originals') + visible-state requirement; new caches require
  limit+eviction+observability or PR rejected; no heavy work on UI thread (PR
  checklist + budgets link)."
- TEST OR BENCHMARK: Mode-invariance suite (parameterized over modes) + VFR
  normalize-once test; domain delete/toggle/undo per mode; noisy-load-trace
  simulation (≤1 switch per dwell window + convergence; override pins mode);
  scrub-during-synthetic-load responsiveness; fill-over-cap ⇒ eviction + truth
  intact; purge ⇒ reload identical.
- STATUS: Open. Impact: High. Likelihood: High (if built without gate). Evidence: Strong architectural inference.

### R-33 — AI: gimmicks before usability; cloud-mandatory transcription

- ID: R-33
- NAME: Premature/gated AI burns trust and creates a privacy surface
- DOMAIN: AI / future extensibility
- DESCRIPTION: Zero AI/transcription code exists (strong negative — ordering still
  free). Study §11 splits useful (STT→text-edit + captions + filler/silence removal;
  isolate/enhance rescue; beat/silence detect; mask/reframe starting points) vs
  distracting (script generators, credit-metered basics, cloud-mandatory transcribe,
  flow-interrupting or export-gating AI). Premiere cloud STT is most accurate but
  privacy-concerning; ByteDance telemetry + Filmora credit-split confusion are churn
  drivers (study §5.10/5.13). PULSAR stance (validated): optional, offline-capable,
  never required for core edit.
- FAILURE MODE: Credit-meter distrust; telemetry backlash; accuracy shame (FCP
  captions); private material uploaded without consent; core edit held hostage by a
  model download.
- WHY IT HAPPENS: Roadmap order wrong (generative before transcription); network on
  the critical path; model/API/GPU-RAM competition unmanaged.
- USER IMPACT: Medium (trust + privacy); low urgency.
- TECHNICAL IMPACT: Medium (project-format coupling if AI results become state).
- EVIDENCE: Study §§11–13 (high); zero code (strong negative). Ordering risk is
  **Speculative** until scheduled.
- CURRENT PULSAR EXPOSURE: None.
- EARLY WARNING SIGNAL: AI milestone scheduled before reliable captions,
  silence/filler removal, beat markers, guided repair; transcription design with
  network on the critical path.
- METRIC / DETECTION METHOD: AI-absent build (flag off) ⇒ full suite green; 100% of
  core flows pass without AI; WER benchmark before text-edit ships; airplane-mode
  caption test.
- COST IF FIXED NOW: Free (sequence the roadmap).
- COST IF FIXED LATE: Privacy retrofit + trust damage + credit-model pressure.
  (High.)
- DECISION NEEDED: SAFE TO DEFER implementation; LOCK the ordering + local-first rule
  NOW (one paragraph).
- DEADLINE / STAGE: Order frozen NOW; transcription NEXT (after basic editor works);
  generative dead last, if ever; never gate export behind AI.
- MITIGATION DIRECTION: Frozen order: (1) local-first transcription→text-edit +
  captions + filler, (2) isolate/level/duck/loudness rescue, (3) beat/silence detect,
  (4) mask/reframe starting points, (5) generative last/if-ever. AI as optional
  adapter-side services; domain never depends on model output; results applied as
  editable undoable commands, not magic state. `TranscriptionService` interface with
  local impl first; cloud strictly opt-in per job with consent indicator; model
  downloads explicit + cached + bounded (ties to R-16).
- GUARDRAIL / INVARIANT: "Core edit fully functional without AI; no credit-meter on
  basics; no network I/O in default transcription path; audio never leaves device
  without a consent event."
- TEST OR BENCHMARK: AI-absent suite; transcription→undoable-commands test; STT
  accuracy regression corpus; consent/cloud-refusal test; model-cache bound test.
- STATUS: Open. Impact: Medium. Likelihood: Low (nothing scheduled). Evidence: Speculative.

### R-34 — Feature coupling: timing duplicated or mutated by newcomers

- ID: R-34
- NAME: Nested/multicam/effects/transitions/AI-results forking timeline truth
- DOMAIN: Feature coupling
- DESCRIPTION: No nested/multicam/effects/export-queue code exists — layering still
  designable. Known traps: nested/multicam before single-timeline truth is exact
  (multicam is the hardest sync problem — Avid/VEGAS pro-tier; Olive 0.2 node-timeline
  rewrite stalled years); effect parameters storing authoritative timing (removing an
  effect shifts sync; Lite bypass changes export); implicit transitions from overlap
  (Shotcut 3-frame trap); forcing simple titles through a node graph (Fusion pain);
  background export live-linked to the mutable timeline (Resolve queued-job mutation;
  Premiere queue loss).
- FAILURE MODE: Multiplicative desync; strip/bypass changing timelines; queued jobs
  corrupted by continued editing.
- WHY IT HAPPENS: New features duplicate the one timing authority instead of
  referencing it.
- USER IMPACT: High if triggered; zero today.
- TECHNICAL IMPACT: High (coupled rewrite when foundation shifts).
- EVIDENCE: Study §§1.1,1.7–1.8,12,13 (high); no code (strong negative).
  **Speculative** for PULSAR until proposed.
- CURRENT PULSAR EXPOSURE: None — keep it that way until gates pass.
- EARLY WARNING SIGNAL: Nested/multicam PR before VFR + visible-link + undoable
  ripple + gap-objects all ship; effect struct with own timeline start/duration;
  export job holding `ProjectDocument&`.
- METRIC / DETECTION METHOD: Foundation-gate checklist; strip/bypass invariance;
  queue-mutation test (enqueue → edit ⇒ output identical to enqueue-time snapshot).
- COST IF FIXED NOW: Free (rules).
- COST IF FIXED LATE: Coupled rewrite. (Very High.)
- DECISION NEEDED: SAFE TO DEFER features; LOCK the reference rules NOW.
- DEADLINE / STAGE: Rules NOW; nested/multicam LATER (gated); effects/transitions at
  first implementation (NEXT at earliest); snapshot queue with first background
  export (NEXT).
- MITIGATION DIRECTION: Timing lives in exactly one place (domain timeline).
  Nested = immutable child revision referenced by parent (epoch/generation
  protections); multicam sync-map = derived artifact, never mutating source timing.
  Effects/transitions reference clips by ID and own only parameters; timeline owns
  time; no implicit-transition-from-overlap; simple-titles path separate from any
  node graph. Export queue = decoupled snapshot queue (R-30): job captures immutable
  revision + source IDs; preflight + match-source defaults.
- GUARDRAIL / INVARIANT: "No effect type with timeline coordinates; remove-all-effects
  ⇒ serialization identical; export job constructor rejects live document refs."
- TEST OR BENCHMARK: Gate tests (VFR drift-free import; nested-reopen determinism;
  sync-map stability); strip/bypass invariance; overlap-no-auto-transition;
  snapshot-queue invariance + preflight plain-language + hw-fallback tests.
- STATUS: Open. Impact: High. Likelihood: Low (nothing proposed). Evidence: Speculative.

### R-35 — Build, codec-environment, and cross-platform seams

- ID: R-35
- NAME: Hard FFmpeg pkg-config dependency, CPU-only decode, Linux-only assumptions
- DOMAIN: Dependency / codec environment / cross-platform
- DESCRIPTION: `src/media/ffmpeg/CMakeLists.txt` REQUIRES 5 libs with no version
  floor/ceiling, vendoring, or feature probe — already bitten once (missing
  `libswscale` dev headers + Qt6 needed a header shim + `.pc` hack; product code
  untouched). Decode is CPU-only (`avcodec_open2` no opts; GTX present but unused) —
  correct default, but M11/M12 pressure to flip HW on globally repeats the
  Shotcut/CapCut hw-encoder failure class. Linux-only assumptions baked in:
  mandatory `__int128` (`#error` without), little-endian x86_64 assumption, symlink-
  resolving `canonical` paths rejecting `://`, no temp/cache-dir abstraction.
  Windows parked, macOS on real hardware only (correct), but export/proxy/temp paths
  are exactly where platform bugs concentrate (Kdenlive "Linux stable, Windows
  bumpy"; Shotcut AV-blocking-encoder; Controlled Folder Access). Perf tiers are
  Linux-measured only.
- FAILURE MODE: Fresh-checkout/CI breakage; FFmpeg 5/6/7 API drift; silent behavior
  change via system upgrade; global-HW flip ⇒ driver-variance export flood; MSVC
  (`__int128`), drive/UNC/case-insensitive paths, AV-blocked output, `/tmp`
  assumptions break bring-up; "patata PC" claim fails on Windows iGPU/macOS ARM.
- WHY IT HAPPENS: Unpinned system deps; HW treated as global toggle; platform policy
  scattered instead of behind a seam.
- USER IMPACT: Medium (build/install pain; export failures on specific machines).
- TECHNICAL IMPACT: Medium–high (combinatorial matrix at M11 if unprepared).
- EVIDENCE: CMakeLists, X2 build note, README core-only escape, `__int128`/`canonical`
  call sites, single-platform presets. **Observed in PULSAR**. Failure classes
  **Observed repeatedly in competitors**.
- CURRENT PULSAR EXPOSURE: Medium for dev machines; low runtime (no proxies/export/
  temp writes beyond fixtures) — high the moment M11 writes outputs.
- EARLY WARNING SIGNAL: `cmake` failing on `PkgConfig::FFMPEG`; new `lib*` without
  version floor; HW PR without fallback test; hardcoded `/tmp`, POSIX-only
  permissions, new `__int128` outside domain math.
- METRIC / DETECTION METHOD: Clean configure on documented baseline + core-only
  fallback green (CI); platform-assumption lint (0 raw `/tmp`, 0 `canonical` outside
  policy); per-platform X-series fixture runs before any tier claim.
- COST IF FIXED NOW: Cheap (doc table + seam stub).
- COST IF FIXED LATE: Kdenlive-class Windows support burden; hw-matrix blowup at M11.
  (High.)
- DECISION NEEDED: MEASURE NOW (pin + document) — no new media lib without floor +
  OFF-fallback test.
- DEADLINE / STAGE: Doc/seam NOW; populated at M11 (export outputs) / M12
  (proxy/cache dirs); no cross-platform perf claims until measured.
- MITIGATION DIRECTION: Record minimum/tested FFmpeg versions; keep
  `PULSAR_BUILD_FFMPEG=OFF` viable; M11 encode deps behind options + version checks,
  software-only default; per-job HW probe with software fallback (never global
  `hw_auto` without override + fallback test); single `LocalPathPolicy` +
  `TempDirPolicy` seam (`std::filesystem::temp_directory_path` + fallback +
  user-visible location); `__int128`/endian isolated behind `Rational`/`MediaTime`;
  `domain` free of OS APIs (already true); export audio never assumes
  PulseAudio/ALSA (Qt audio is the portability reference).
- GUARDRAIL / INVARIANT: "New filesystem/temp code goes through the seam; no raw
  `/tmp`; no `canonical` outside the policy wrapper (it throws — map to coded
  errors); any perf claim cites measurement doc + commit + fixture commands."
- TEST OR BENCHMARK: Full build on pinned FFmpeg + core-only with no FFmpeg; path
  units (dir/fifo/missing/`://` ⇒ correct codes); future Windows suite
  (drive/UNC/long-path), unwritable-temp ⇒ preflight action error,
  AV-blocked-output ⇒ plain language (not exit-code); HW-present + HW-absent legs
  with forced-failure injection.
- STATUS: Open. Impact: Medium. Likelihood: High. Evidence: Observed in PULSAR.

### R-36 — Creator-audio gap is structural (beat/duck/roles/loudness)

- ID: R-36
- NAME: No beat markers, ducking, roles/stems, or loudness path
- DOMAIN: Audio scale
- DESCRIPTION: Study §§4.10–11/5.12/9 prescribe beat detection + snap-to-beat,
  auto-ducking with visible envelope, roles/stems, and loudness preset+meter as CORE
  (Filmora/CapCut daily-use prove demand; Kdenlive/Shotcut weakness proves the gap;
  Premiere Essential Sound + FCP Roles patterns). PULSAR has none: scalar volume
  only; A8 pyramid stores min/max (no onset/energy band) so beats can't be derived
  without re-decode; no marker/role domain concept. Beat-cut and dialogue-over-music
  — the Sounds pack's raison d'être — would be done by eye against peak thumbnails
  (study §6.4 "waveforms lie about beats").
- FAILURE MODE: Shipping 191 SFX without beat/duck/roles repeats the FOSS weakness;
  ad-hoc per-file `.beats` sidecars appear.
- WHY IT HAPPENS: No domain concept for markers/roles; pyramid encoding can't answer
  onset queries.
- USER IMPACT: Medium–high strategically (highest-ROI audio per study).
- TECHNICAL IMPACT: Medium (schema bump cost if onset data must live in `.pwfc`).
- EVIDENCE: Negative grep (beat/duck/role/stem/loud/onset/tempo absent outside
  study). **Strong architectural inference** + competitor demand
  **Observed repeatedly in competitors**.
- CURRENT PULSAR EXPOSURE: Low (no promise made in code).
- EARLY WARNING SIGNAL: "Snap cut to beat" request with no marker model to attach to.
- METRIC / DETECTION METHOD: Beat precision/recall on reference music set (future);
  duck params surfaced or not (binary: not).
- COST IF FIXED NOW: 0.5 day (reserve extension points without DSP).
- COST IF FIXED LATE: Pyramid re-encode if `.pwfc` must carry onset data. (High.)
- DECISION NEEDED: WATCH ONLY for DSP; freeze schema-extensibility plan NOW with
  R-29.
- DEADLINE / STAGE: Extension points BEFORE PUBLIC ALPHA (before `.pwfc` fleet and
  pack metadata freeze); DSP NEXT/LATER per R-33 order.
- MITIGATION DIRECTION: Markers/roles as timeline-domain concepts computed
  off-UI-thread, stored outside `.pwfc`; keep kSchema bump path tested; never
  credit-gate basics.
- GUARDRAIL / INVARIANT: "No onset/beat fields in `.pwfc` v1 payload without
  schema+migration test; no cloud/credit dependency for beat/duck v1."
- TEST OR BENCHMARK: v1-opens-after-change schema test; marker-rail snap unit test
  once the type exists; click-track transient visibility at all pyramid levels.
- STATUS: Open. Impact: Medium. Likelihood: Medium. Evidence: Strong architectural inference.

---

## 4. Top Architectural Landmines (≤10 — extremely expensive if fixed late)

1. **R-09 VFR blindness** — every future subsystem (export, proxy, waveform, sync)
   bakes CFR assumptions; retrofit ×10. Detect NOW, normalize before export.
2. **R-11 A/V origin asymmetry** — intermittent post-seek desync on common files;
   every downstream feature inherits the mapping. Unify with R-14 now (days).
3. **R-16 Unbounded derived caches** — one more cache type without a shared bounded
   `CacheStore` creates the "900 GB" support class permanently. Gate before the 2nd
   cache family.
4. **R-23 No autosave/recovery** — total unsaved-work exposure; collides with
   relink/path semantics if added late. Before Internal Alpha.
5. **R-30/R-31 Export/proxy truth gates** — live-linked queue or proxy-sourced export
   is a correctness scandal, not a bug. Snapshot isolation + render-from-originals
   locked as prose NOW, before M11/M12.
6. **R-32 Profile truth-variance** — silent Lite-vs-export divergence destroys trust
   unfixably. One-sentence invariant + table NOW (free).
7. **R-04/R-05 Unvirtualized full-rebuild timeline** — full copy + full QML
   re-instantiation per edit; thumbnails on top without virtualization repeats
   Shotcut's 19 GB cliff. Guardrail NOW, design before thumbnails.
8. **R-01/R-03 Timeline O(n²)/O(n) core** — already observed (~22 s split); undo,
   save, and every future op inherit it. Benchmark + splice NOW.
9. **R-25 Schema with no migration story** — first v2 field strands files. Policy
   paragraph NOW (trivial).
10. **R-15 Preview color defaults** — silent range/matrix/HDR wrongness in every
    frame; export inherits it by convenience. SDR policy + 1-page contract NOW.

---

## 5. Point-of-No-Return Decisions

| Decision | Current state | Must remain flexible | When to lock |
|---|---|---|---|
| Time representation | Exact rational/`__int128` core, strong | Double excluded from mapping (R-14) | NOW (covenant + tests) |
| Timeline→stream mapping | Split-brain A vs V (R-11) | Single shared function | NOW (fix with R-14) |
| Timeline identity/containers | `vector<Clip>`, value semantics, linear ops | Splice cost, index strategy (R-01–R-03) | Index design BEFORE INTERNAL ALPHA |
| VFR policy | None — silent CFR assumption (R-09) | Detect/normalize strategy choice | Detect NOW; normalize BEFORE BETA |
| Media identity | Path-only (R-27) | Fingerprint fields additive | Fingerprint BEFORE PUBLIC ALPHA |
| Project schema | v1 strict, no migration (R-25) | Migration function shape | Policy NOW; code at v2 |
| Cache ownership | None — unbounded per-cache (R-16/17) | Shared bounded store | BEFORE PUBLIC ALPHA (before 2nd cache) |
| Thread ownership | Implicit affinity, UI joins (R-20–22) | Executor + cancellation tokens | Executor BEFORE PUBLIC ALPHA |
| Render pipeline | Preview-only sws defaults (R-15) | Export/GPU paths independent | Contract NOW; enforcement at M11 |
| Audio clip model | Linked embedded only (R-28) | A9 reuse of clip geometry | Invariant NOW; gates A9 |
| Profile/truth split | Nothing exists (R-32) | Domain never reads mode | NOW (free prose) |
| Export/proxy truth | Nothing exists (R-30/31) | Snapshot isolation, originals-only render | NOW (free prose) |

---

## 6. Feature Dependency Map

| Feature | Needs first | Architecture touched | Danger if early | Safe prerequisite (gate) |
|---|---|---|---|---|
| Sounds (191) | Index + search + favorites + audition (R-29) | App asset index + QML browser | Findability collapse; decode-per-row | Keymapped insert; favorites schema |
| Proxies | Coupled preview+proxy default + truth rule (R-31) | FFmpeg adapters + app policy + bounded cache | Transcode bloat; grade shift; unlink | R-32 invariance test; R-16 discipline |
| Autosave | Draft sidecar + recovery UX (R-23) | App snapshot/journal + retention | Sole-copy corruption; queue loss | Snapshot primitive; R-24 fencing |
| Text-based editing | Local STT + WER bar (R-33) | Optional AI adapter → commands | Unusable deliver; privacy hit | Manual captions; AI-absent suite green |
| Nested timelines | VFR + link-visible + undoable ripple + gaps (R-34) | Domain composition (immutable child refs) | Multiplicative desync; rewrite stall | Single-timeline desync suite green |
| Multicam | Nested gates + derived sync-map (R-34) | App sync service | Hardest sync; scope creep | Nested shipped + map stability test |
| Effects | Reference-by-ID rule (R-34) | Adapter render stack | Timing-in-effect desync | Strip/bypass invariance tests |
| Transitions | Explicit objects (R-34) | Domain transition type | 3-frame-overlap trap | No-auto-transition test |
| AI transcription | Core-without-AI gate + offline path (R-33) | App service interface | Credit/privacy backlash | Roadmap order; WER corpus |
| Background export | Snapshot queue + preflight (R-30) | App queue + worker | Live-link mutation | Enqueue-then-edit invariance |
| Adaptive Auto | Manual modes + hysteresis spec (R-32) | App slow controller | Oscillation; regen storms | Manual proven; dwell sim tests |

---

## 7. Performance / Resource Risks

R-01, R-02, R-03 (timeline compute), R-04–R-07 (invalidation), R-08 (undo),
R-16/R-17/R-19 (cache/memory), R-20/R-21 (threads/tick), R-24 (save),
R-32 (profiles/adaptation). Common thread: **PULSAR measures some things already
(latency rings, mailbox stats, conversion time) but not the load-bearing ones
(tick duration, sync duration, save/open duration, RSS, cache bytes).**
Instrumentation (R-19) is the cheapest unlock: every later decision becomes data, not
debate. Fix order by leverage: R-01 splice → R-02 neighbor check → R-03 duration
cache → R-06 signal split + R-07 no-rebuild-on-failure → R-04 sync budget + R-05
design → R-08 benchmarks (inherits R-01). Estimated NOW work: 2–4 days + benchmarks.

## 8. Media Timing Risks

R-09 (VFR), R-10 (durations/B-frames), R-11 (origin asymmetry), R-12 (AAC priming),
R-13 (device servo), R-14 (float lattice/double hatches/seek rounding).
PULSAR's rational core + floor-seek + strict scheduler already avoid the *naive*
versions of competitor bugs — remaining risks are **asymmetries and missing policies
at boundaries**, exactly where competitors fail. Cheapest bundle: R-11 + R-14 seek
unification (one bug class, days) + R-14 lint covenant + R-09 probe-side detection
fields + R-10 duration propagation design.

## 9. Project Durability Risks

R-23 (autosave — biggest loss window, do first), R-24 (save/open scale —
instrument now, background later with fencing), R-25 (version policy — prose now),
R-26 (dirty/eviction semantics — with autosave), R-27 (fingerprint + relink —
additive codec fields while changes are still cheap). Priority: R-23 → R-27/R-24
instrument → R-26/R-24 background → R-25 code at v2.

## 10. Threading / Lifetime Risks

R-20 (joins, queues, probe, executor), R-21 (tick convert), R-22 (shutdown residuals).
Ownership map today: UI/control thread owns controller, session, playback session,
coordinator, audio session, QtAudio affinity; workers own only probe slot, decode
slot, waveform pool — and workers never touch document/QML directly (keep it).
Cancellation tokens exist (waveform `cancel()`, playback generations) but no
interrupt reaches in-flight FFmpeg. Rule going forward: generation + interrupt pairs
everywhere; one bounded executor with playback-first priority before any new worker
family.

## 11. Cache / Proxy Risks

R-16 (global bounded store + orphan sweep), R-17 (registry cap), R-18 (fingerprint),
R-19 (observability), R-31 (proxy truth). Order: fingerprint hardening (R-18) before
user-facing waveforms → shared cache primitive + observability + registry cap
(R-16/R-17/R-19) before 2nd cache type → memory enforcement at proxy milestone.
Strong foundations to keep: transactional publish, strict `open()` validation
(magic/version/key/CRC/index-chain), streaming builder with bounded state, request
coalescing, off-UI-thread manager.

## 12. UI / Timeline Scale Risks

R-04 (rebuild per edit), R-05 (virtualization), R-06 (coarse signal), R-07 (drag
stack). QML drag already defers via preview-offset (good — lock it). Per-tick
position signals fire every 16 ms even when paused (gate them). Ruler (7 ticks) and
viewer gating are fine; metadata re-split per frame is negligible today.

## 13. Audio Risks

R-28 (single pipeline + A9 geometry lock), R-29 (sounds index/audition), R-36
(beat/duck/roles/loudness scaffolding). Waveform pyramid per-file math is healthy
(~1.8 MB per 10 min stereo; level-chain validated) — the risk is entirely
cross-file discipline (R-16/R-17) and misapplying the timeline pipeline to library
rows (R-29).

## 14. Export / Media Pipeline Risks

R-30 (export gates + coded errors + audio policy), R-15 (color contract), R-31
(proxy truth), R-35 (deps + platform seams). Ten-item M10→M11 gate checklist lives in
R-30's mitigation; the one-line version: snapshot-isolated, match-source,
software-default, preflighted, plain-language, decoupled, originals-only,
color-explicit, coded-error, version-pinned, path-seamed.

## 15. Future AI Risks

R-33 (ordering + local-first), R-36 (creator-audio DSP order). Absolute: PULSAR core
remains fully usable without AI; no credit-meter on basics; transcription local-first
with optional consented cloud; AI results become editable commands, never hidden
state. Model downloads explicit, cached, bounded (R-16 accounting applies).

---

## 16. Canary Scenarios

| Canary | Project | Detects | Invariant | Metric |
|---|---|---|---|---|
| CANARY-01 | 10-min 1080p H.264 normal edit | Baseline health | Edit→paint < 50 ms | Sync/commit latency p95 |
| CANARY-02 | 2-hour podcast (audio-long) | Long-stream drift, T1/T4/S3 | Split-contiguous convert identical; pause/resume walk 0 | Sample divergence; resync error |
| CANARY-03 | VFR phone recording | R-09 blindness | Probe flags VFR; divergence bounded | A/V divergence p95; detection recall |
| CANARY-04 | 1000 tiny clips | R-01–R-04 knee | Sub-50 ms split/sync/select | Op latency vs N curve |
| CANARY-05 | 10000 synthetic clips | Quadratic cliff | Completes within budget | Split/insert/lookup at 10k |
| CANARY-06 | 4 GB RAM machine | R-19 budget breach | RSS within budgets.md tiers | RSS; subsystem bytes |
| CANARY-07 | Media moved/offline | R-27 relink gap | Non-destructive Offline + reason + relink | Time-to-relink; IDs stable |
| CANARY-08 | Corrupt/truncated source | R-10/R-20 brittleness | Degrade to gap; session survives; plain error | Session-death 0; error code present |
| CANARY-09 | Long audio-late / AAC-priming source | R-12 onset error | Onset alignment ≤ 2 ms | A/V onset offset |
| CANARY-10 | Rapid open/close under worker load | R-20/R-22 teardown | Clean quit, no hang/crash (TSan) | Quit iterations clean |
| CANARY-11 | Nonzero-`start_time` 2-clip + seek storm | R-11 asymmetry | PTS convergence ≤ 1 frame every seek | Post-seek A/V offset |
| CANARY-12 | Limited-range 709 bars + 10-bit + PQ smoke | R-15 color policy | ΔRGB ≤ 2 LSB vs reference; HDR badged | ΔRGB; badge fires |
| CANARY-13 | Disk-full export attempt | R-30 preflight | Plain-language error, no partial success | Error code + message quality |
| CANARY-14 | 191-SFX browser + audition burst | R-29 decode storm | Zero manager decodes; preview < 150 ms | Manager invocations; paint latency |

---

## 17. Observability Plan (cheap to expose early)

Already measured (keep): sync/ctor/conv latency rings, mailbox
published/evicted/high-water, `read_latency_ring`, event drops, conversion time,
`EditorSyncDiagnostics`, build progress/reports. Missing (add now, code-free design
in R-19/R-20/R-24): **tick-duration ring; `synchronize_document_state` p50/p95;
`timelineChanged` emissions per action; delegate count; save/open wall-time by size;
RSS + per-subsystem bytes; cache hits/misses/bytes/evictions/orphans/registry per
type; worker-join latency; control-wait count; probe duration + stale-drop count;
underruns + reanchor deltas; seek-to-Ready histogram; file-opens-per-audition.**
No telemetry framework, no network, no tracking — local engine-lab/logs first, UI
inspector later. We cannot manage what we cannot measure; everything above is
counters and rings, not infrastructure.

---

## 18. Engineering Gates (safety/performance foundations only)

**BEFORE INTERNAL ALPHA:** R-01/R-02 fixes + benchmarks; R-03 measure + index design;
R-04 sync budget; R-06 signal split; R-11 + R-14 mapping unification + double
covenant; R-14 converter-ownership rule; R-15 SDR policy + color contract page;
R-23 autosave drafts + recovery; R-24 save/open instrumentation; R-25 version policy;
R-27 fingerprint fields; R-28 A9 invariant + tests; R-30/R-31/R-32 lock prose
(export/proxy/profile gates); R-32 snap/ripple/link domain state; R-35 FFmpeg pin +
seam stub. R-22 hygiene.

**BEFORE PUBLIC ALPHA:** R-05 virtualization design (+ R-04 incremental views);
R-20 UI-join removal + control-lane split + probe interrupt + shared executor design;
R-21 convert off UI thread; R-16 shared bounded cache + R-17 registry cap + R-19
telemetry live; R-18 fingerprint hardened; R-29 sounds index ships with the pack;
R-36 extension points frozen; R-09 VFR detection surfaced.

**BEFORE BETA:** R-05 virtualization implemented; R-03 index if N warrants; R-08
representation review; R-09 normalization shipped; R-12 priming compensation; R-27
relink UI; R-19 byte-budget enforcement; R-24 background save (with R-23/R-26
fencing).

**BEFORE 1.0:** R-13 servo only if field data demands; R-08 redo hygiene; full
CANARY-01–14 suite green; per-platform X-series runs for every perf claim (R-35);
AI only per R-33 order (transcription first, generative last/if-ever).

---

## 19. Do-This-Before-It-Hurts Table

| Risk | Not urgent because… | Becomes urgent when… | Decision deadline | First warning signal |
|---|---|---|---|---|
| R-01 split O(n²) | Demos are small | 1k-clip sessions exist | NOW | Split p95 > 50 ms at 1k |
| R-09 VFR | No export yet | Export/trim math lands | Detect NOW, normalize BEFORE BETA | Phone clip drifts > 40 ms |
| R-11 origin asymmetry | Few multi-clip origins | Cross-clip seek Hewitt | NOW | Seek-storm divergence |
| R-23 autosave | Sessions short | Real projects entrusted | BEFORE INTERNAL ALPHA | "Lost work" report |
| R-16 unbounded caches | Files tiny | 2nd cache family proposed | BEFORE PUBLIC ALPHA | Dir bytes monotonic |
| R-17 registry | Entries tiny | Long sessions | NOW | Registry ≫ open files |
| R-06 coarse signal | N small | Selection jank at 500 | BEFORE INTERNAL ALPHA | Select paint > 16 ms |
| R-05 virtualization | Single track | Thumbnails proposed | Design BEFORE PUBLIC ALPHA | Delegates == N at 500+ |
| R-15 color defaults | Preview-only | Export M11 starts | NOW (contract) | Bars mismatch > 2 LSB |
| R-30/31 export/proxy gates | No code exists | M11/M12 kickoff | NOW (prose) | "Render" touches live doc |
| R-32 profile truth | No modes exist | First Lite PR | NOW (prose) | Mode touches domain/ |
| R-25 migration | Single version | First v2 field PR | NOW (prose) | Field added sans plan |
| R-27 relink | Single-asset sessions | Folder reorganization | BEFORE PUBLIC ALPHA | Offline after move |
| R-20 UI joins | Local files, 1 clip | Scrub/multi-clip | NEXT (1–2 sprints) | Tick p95 rising |
| R-24 save thread | Tiny projects | 1000s of clips | Instrument NOW | Save p95 growth |
| R-28 A9 geometry | Single pipeline correct | A9 branches | NOW (invariant) | Own-offset-math appears |
| R-29 sounds index | Pack unshipped | 191 files land | BEFORE PUBLIC ALPHA | Files without index.json |
| R-35 deps/seams | Linux-only dev | M11 writes outputs | NOW (doc/seam) | Shim reappears |
| R-12 AAC priming | ~21 ms subtlety | Dialogue/music bar | BEFORE BETA | Onset fixture offset |
| R-33 AI order | Nothing scheduled | AI milestone proposed | NOW (order prose) | AI before captions |

---

## 20. Deferred / Watch-Only Risks

- **R-13 device servo** (NEXT/LATER): correct on good hardware; add deadband only on
  field evidence. **R-22 residuals** (hygiene NOW, trivial). **R-26 dirty/eviction**
  (NEXT with autosave). **R-10 hardening** (NEXT; design now).
- **R-33 AI implementation, R-34 nested/multicam/effects, R-36 DSP**: SAFE TO
  DEFER / WATCH ONLY with rules locked now. No database, ECS, plugin system, render
  farm, or microservices justified by anything in this register.
- **Pure speculation, excluded from action**: asset-scale history pressure beyond
  R-08's caps; cross-FFmpeg fingerprint drift (R-18c — instrument, don't fix);
  sub-frame splice bias audibility (R-14/S4-class — document tie rule at export).
- **Developer-only, not product architecture**: `/dev/shm` worktree volatility —
  explicitly out of scope per mission brief.

---

*End of register. Next review: every milestone, starting with the Internal Alpha
gate (§18). Update STATUS fields as risks close; never delete a guardrail without
replacing its test.*
