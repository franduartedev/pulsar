# ADR 0019 — M10 Preview Audio Mix

- Status: accepted
- Date: 2026-09-22

## Context

M10 A8 plays one embedded audio stream through one `AudioOutput`. A9 adds
independent audio tracks, so preview playback needs simultaneous contributors
without creating one device stream or clock per clip. Editorial time and media
source time remain exact rational values and must not inherit a device format.

## Decision

`resolve_audio_contributors` resolves an immutable `ProjectSnapshot` over a
half-open timeline range. It returns embedded video audio once, followed by
independent audio tracks in stable project order. Each result carries track,
clip, media, source, stream, range, gain, mute and native format identity.

`AudioMixSession` owns the active decoder sources and streaming conversion
state. A published snapshot is a generation boundary: source sessions and
queued PCM from the previous generation are discarded before new output can
be observed. Contiguous renders retain sources; discontinuous renders and
seeks recreate only the contributors required at the new position.

Preview mix format is 48 kHz stereo, interleaved Float32. This is a playback
policy, not editorial or export truth. Native mono and stereo at different
rates use the existing streaming converter; channel counts above two are
reported as unsupported because no speaker-layout policy exists. Input is
bounded to 32,768 frames per decoded block, output calls to 8,192 frames, and
queued normalized PCM to 65,536 frames per contributor.

Gain and mute are applied per contributor before deterministic summation.
There is no track-count normalization, limiter, AGC or compressor. The final
preview mix is hard-clamped to [-1, 1] before it crosses the output boundary.

A missing, starved, failed or early-ended contributor produces silence only
for that contributor and an explicit issue; healthy contributors continue.
The mixer emits one PCM block and never owns a device. A9.3 may feed that
single stream to the existing single `AudioOutput`. The device clock is the
master only while usable mixed audio is being submitted and the output is
running; otherwise timeline playback uses its software clock.

## Consequences

The mixer remains in application code with no Qt or FFmpeg dependency.
Timeline/source mapping stays exact until sample-grid rounding: contributor
starts and ends use the first 48 kHz sample at or after each half-open
boundary. Streaming resampler state makes output invariant to decode and
render block partitioning within the tested tolerance. Desktop audible
integration, track controls, buses, effects and export mixing remain deferred.
