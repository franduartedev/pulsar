# PULSAR Audio FX roadmap

This document describes the next audio improvements after the first `Distant Music`
vertical slice. Effects remain non-destructive, undoable and shared by preview and export.

## Product direction

PULSAR should make cinematic audio adjustments approachable without hiding the timeline's
precision. A user can start from a preset, listen immediately, then expose the exact controls
when they want to refine the result.

## Near-term sequence

### 1. Stabilize Distant Music

- Validate preview/export parity with real stereo music on Linux.
- Replace stepped Inspector buttons with editable numeric fields and compact sliders.
- Add a small gain/filter curve preview.
- Add a bypass button and an explicit “Restablecer” action.
- Add project migration fixtures for version 7.

### 2. General effect-chain model

Move from one field on `Clip` to an ordered, bounded list of effects:

```text
AudioEffect {
  type
  enabled
  parameters
}
```

The first supported types should be `DistantMusic`, `Telephone`, `Underwater` and
`RoomAmbience`. The list must have a small hard limit, stable serialization and one undo step
for adding, removing or reordering an effect.

### 3. Presets

Presets should be data, not hard-coded UI actions. Initial built-ins:

- Distant Music — music that approaches from far away.
- Radio — narrow band, light saturation and room noise.
- Underwater — low-pass, low-mid emphasis and muffled transients.
- Voice Focus — high-pass, presence lift and gentle de-reverb.
- Cinematic Impact — short low-end lift, transient emphasis and tail ambience.

User presets can come later, after the project format and effect-chain model are stable.

### 4. Mix safety and feedback

- Per-track peak meters.
- Master peak and short-term loudness display.
- True look-ahead limiter with a shared preview/export implementation.
- Clip warnings when gain staging is likely to cause heavy limiting.
- Optional solo-safe and pre-fader audition modes.

### 5. Automation

Add keyframes only after the static effect model is reliable. The first useful automation target
is Distant Music's progress: a clip can start distant, pause, and approach again. Later targets
include gain, filter cutoff, reverb mix and effect bypass.

## Broader PULSAR improvements

- Proxies and adaptive preview for long or high-resolution media.
- Media relink UI and a missing-media report.
- Native-looking dialogs on Linux and Windows.
- Vertical 9:16 and square canvas presets.
- More transitions, animated titles and subtitle tracks.
- Hardware-adaptive decode/encode where it improves real projects.
- Windows packaging once the path, resource and filesystem portability debt is closed.

## Engineering guardrails

Every effect must:

1. process bounded PCM blocks without per-sample allocations;
2. reset deterministically after seek and project reconfiguration;
3. have a pure application-layer implementation independent of Qt and FFmpeg;
4. be covered by unit tests and preview/export parity tests;
5. round-trip through the project codec and remain undoable.

