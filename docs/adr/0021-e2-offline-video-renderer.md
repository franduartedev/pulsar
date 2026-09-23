# ADR 0021 — E2 offline timeline video renderer

Status: accepted

## Context

E1 freezes the editorial timeline as an immutable `ExportPlan`. Preview
playback remains realtime and is not a valid export renderer because it has
device-clock, starvation and recovery semantics.

## Decision

`OfflineVideoRenderer` consumes one `ExportPlan` and renders output frames in
strictly increasing frame-index order. For frame `n` it computes the exact CFR
timeline time, resolves only the designated video track through
`TimelineClipResolver`, maps the resolved clip through
`clip_source_position`, and selects the source frame using presentation
timestamps:

```text
candidate.pts <= source_time < next.pts
```

The source keeps a persistent decoder, candidate and one-frame lookahead. A
seek is performed only when entering a clip/source segment or after a source
discontinuity. Source EOF before the requested presentation interval is an
error; it is never converted to black or a freeze-frame.

Application receives a neutral RGBA8888 buffer with explicit width, height and
stride. The FFmpeg adapter owns all FFmpeg types and converts decoded visual
leases through a cached `SwsContext`. Application performs deterministic
nearest-neighbour aspect-preserving scale and centered letterbox/pillarbox.
The canvas is initialized to opaque black (`RGBA = 0,0,0,255`).

Memory is bounded to the active decoder, candidate, lookahead and one output
buffer. The renderer exposes counters for opens, seeks, decoded source frames,
conversions, output frames and peak live source frames.

## Consequences

The application export contract has no FFmpeg, Qt or playback dependencies.
Fake sources can exercise editorial and timestamp semantics without external
media. FFmpeg integration is tested separately. Stream timestamps are
normalized by the adapter: FFmpeg `AVStream::start_time` is added before a raw
seek and subtracted from returned PTS values, so source time remains relative
to the imported asset.

The neutral source frame may carry an optional presentation duration. This is
used only when a final VFR frame has no lookahead; without sufficient interval
information, a request after the last PTS fails explicitly as early EOF.
