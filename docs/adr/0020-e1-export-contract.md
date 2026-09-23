# ADR 0020 — E1 export contract v0.1

Status: accepted

## Context

PULSAR preview playback is realtime and may involve device clocks, starvation,
recovery and bounded playback queues. Export must instead be an offline,
deterministic representation of an immutable `ProjectSnapshot`.

## Decision

`application::exporting::ExportPlan::build(snapshot, ExportProfile)` freezes all
editorial decisions required by later export milestones. The plan owns a copy of
the snapshot, exposes the document generation identity, and contains no Qt,
FFmpeg, playback or device types.

The only Beta profile is MP4, H.264, `yuv420p`, CFR, AAC-LC, 48 kHz stereo,
192 kb/s. Concrete encoder names and availability belong to the FFmpeg adapter.

The master is the first video asset referenced by the designated video track,
in snapshot track order. Its nominal rational FPS and dimensions define the
output. A missing master, missing FPS or invalid dimensions is an explicit
planning error. Output dimensions are normalized upward to even values for
`yuv420p`; this adds at most one pixel per axis and avoids implicit crop or
stretch.

Video is CFR. Frame `n` represents `[n/F, (n+1)/F)`. All conversion uses exact
rational arithmetic. `frame_count = ceil(editorial_duration * F)` and
`output_duration = frame_count / F`. Audio is generated on the 48 kHz sample
grid through `output_duration`; any tail caused by the final partial video
frame is silence after the editorial end.

Video gaps are black frames. Audio gaps are zero samples. There is no
freeze-frame and no ripple. If audio continues after video, video remains black
while audio continues until the output duration. Known missing media is a hard
planning error, never an implicit black/silent substitute.

`MediaAsset` currently stores no orientation or sample-aspect-ratio metadata.
E1 therefore does not invent support: the presentation policy is
`RejectUnsupported`. Future probe/model metadata that cannot be represented by
the plan must fail explicitly rather than export silently rotated or stretched
content.

## Consequences

The plan size is independent of output duration and frame count. Later renderers
must consume this contract and may not derive their own rounding, gap,
master-selection or missing-media policies. Preview and export remain separate
execution paths while sharing the snapshot and exact time model.
