# PULSAR v1.2 → v1.3

PULSAR should become a complete editor without losing its strongest promise: a small,
predictable tool where preview, timeline and export agree. The versions below are product
milestones, not a promise to ship every item at once.

## v1.2 — The complete editor

### Editing foundation

- Reliable multi-track video and audio editing with multi-select.
- Keyframes for transform, opacity, gain and effect parameters.
- Real transitions beyond cross-dissolve: dip to colour, wipe and audio crossfade.
- Subtitle track with SRT import/export and safe-area preview.
- 9:16, 1:1 and 4:5 canvas presets.
- Relink missing media with a project-wide missing-media report.

### Audio that feels cinematic

- Stable Audio FX chain with bypass, reorder, presets and undo.
- Distant Music, Radio, Underwater, Voice Focus and Room Ambience.
- Track and master meters, loudness readout and a real look-ahead limiter.
- Audio keyframes and a visible gain/effect envelope on the timeline.

### Performance and trust

- Proxies generated per project with a clear “preview quality” indicator.
- Background waveform and thumbnail work with cancellation and bounded memory.
- Project snapshots, autosave history and a recovery browser rather than one opaque recovery file.
- Export queue with resumable jobs and a final “what changed since preview” report.
- Linux and Windows release candidates with the same project format, preview/export behavior,
  Audio FX coverage and documented manual smoke checklist.

## v1.3 — PULSAR's distinctive layer

### Pulse Map

A visual map of a project’s rhythm. PULSAR detects cuts, silence, transients and music beats,
then shows them as optional guides above the timeline. Nothing is changed automatically: the
editor can snap a cut, title or transition to a beat with one explicit action.

### Scene Cards

A lightweight storyboard beside the timeline. Each card can hold a thumbnail, a short intent,
colour tag, dialogue note and continuity warning. Cards stay linked to ranges in the timeline,
so reorganising a scene never loses its editorial context.

### Cinematic Recipes

An effect recipe combines audio, transition, title and colour decisions into one reversible,
previewable operation. Examples:

- **Arrival** — Distant Music, slow title reveal and dip from black.
- **Memory** — softened highlights, room ambience and gentle crossfade.
- **Urgency** — tighter cuts, transient lift and short title emphasis.

Recipes are plans applied through the same command history as manual editing. They never edit a
project invisibly and can be previewed on a temporary snapshot first.

### Continuity Guard

PULSAR can flag likely continuity issues without changing the edit: abrupt audio jumps,
unmatched frame orientation, clip gaps, dialogue under music, and shots that exceed a chosen
duration target. The user chooses whether to fix each suggestion.

### Local-first collaboration

- Portable project manifest with media fingerprints, proxies and notes.
- Review package that can be opened without the original media tree.
- Timecoded notes and decisions stored inside the project, not in a cloud account.

## Technical order

1. Finish Distant Music validation and migrate it into a general effect-chain interface.
2. Add keyframe primitives and a versioned project migration layer.
3. Build meters, limiter and audio envelope rendering.
4. Add subtitles, relink and canvas presets.
5. Add proxies and export queue.
6. Build Pulse Map and Scene Cards from read-only analyzers.
7. Add Cinematic Recipes and Continuity Guard through validated command plans.

## Non-negotiable quality bar

Every major feature must be undoable, deterministic, testable without the UI, safe for old
projects and explicit about what it changes. PULSAR can be ambitious in capability while
remaining calm and trustworthy in behaviour.

