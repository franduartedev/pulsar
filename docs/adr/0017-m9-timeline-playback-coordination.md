# ADR 0017 — M9 Timeline Playback Coordination

- Status: accepted
- Date: 2026-09-17

## Context

M7's `PlaybackSession` is a source-level decoder session. M9's edited
 timeline needs a higher-level coordinator without introducing a second
 source or timeline playback path. M9.3 resolves an immutable project
 snapshot into a clip, editorial gap, missing media, or timeline end.

## Decision

`TimelinePlaybackCoordinator` owns the single authoritative
`PlaybackTransport` for edited-timeline position and user intent. It owns
one immutable `ProjectSnapshot` value and orchestrates at most one active
source `PlaybackSession`. `PlaybackSession` remains timeline-agnostic.

The coordinator evaluates a source session at an explicit
`PlaybackPosition` through `PlaybackSession::update_at`; its internal
transport is not a running clock under coordination. `source_time` from a
resolver result is source-relative playback time. The source session's
`SingleStreamPlaybackClockMap` alone converts it to a stream timestamp,
including non-zero stream origins.

For a clip, source position is mapped exactly as
`source_range.start + (timeline_position - timeline_range.start)`. Adjacent
clips are resolved at the half-open boundary with no synthetic gap. A
same-media transition reuses the session but performs an exact presentation
seek; a different media ID creates a new session. Session destruction may
join its worker and is therefore not promised non-blocking.

Editorial gaps and missing media have explicit metadata-only presentation
states and do not stop a playing timeline clock. Preparing/seeking and
source starvation hold the timeline clock while preserving Playing intent.
Timeline end pauses at the exact duration. Source early EOF becomes
`SourceEndedEarly` while the asset remains available; source errors become
`SourceError` and do not mutate the project catalog.

Snapshot replacement resets the active source session, discards the old
segment, and re-resolves the preserved exact position. Playback-session
 generations, ownership/destruction, and snapshot state identity provide
 stale-work protection; no extra coordinator generation is introduced.

The M7 `VideoSample` contract remains metadata-only. Pixel/frame-lease
integration and replacement of the M8 desktop decoder path are deferred to
M9.6; the desktop must not run a third independent playback path.

M9 transport mutations are centralized through one coordinator gate that
combines user intent with current timeline advance permission.
This is not an audio/master-clock policy; that remains M10.

## Consequences

M9 playback is deterministic and backend-neutral, with bounded ownership
and no Qt/FFmpeg dependency in application code. It intentionally holds
 during source preparation/starvation and has no audio master clock yet.
