# Beta editor experience pass — decisions and deferrals

Scope: presentation and gesture behaviour of the desktop editor for v0.1 Beta.
No change to playback or export internals.

## Behaviour added

- **Magnetic snapping** (move and trim). Tolerance is 8 screen pixels, converted
  with the current viewport only to decide whether an edge is "near"; the result
  is always an exact `MediaTime` (another clip's start/end, the playhead or zero).
  Evaluated only during a gesture (`EditorSession::resolve_move` /
  `resolve_trim_edge`, pure) and shown live with an amber guide line.
- **Collision stop.** A dragged clip is confined to the free span between its
  neighbours on its own track. It never overlaps, never jumps over a clip and
  never rips anything else.
- **Zero-gap append.** `append_video_asset` places the clip at the exact end of
  VIDEO 1; covered with rational durations (4.004 s, 10.957333 s, 7 frames @29.97).
- **Trim fixed.** The QML passed pointer coordinates offset by the track-header
  width; trims were wrong or rejected.
- **Separar audio.** The embedded audio of a VIDEO 1 clip becomes an independent
  audio clip with the same timeline start and source range; the video clip is
  muted; gain moves with the sound. One undo/redo step (`CommandProcessor`
  groups). Persists as ordinary clips and exports identically (e2e compares the
  exported audio before/after).
- Ruler with major/minor "nice" ticks, click/drag to seek; playhead never
  intercepts the mouse; **Ajustar** (Shift+Z) fits the program in view.

## Deliberately deferred (v0.1.1+)

- Mouse-wheel / pinch zoom anchored at the pointer.
- Drag from Library to VIDEO 1 (double-click inserts at the playhead today).
- Ripple/overwrite edit modes; multiple video tracks.
- Relinking missing media; autosave/recovery.
- Waveform: RMS band, clipping marker, per-clip gain reflected in the shape.
- `EditorSession::rebuild_read_model` copies a `MediaAsset` per clip via
  `find_asset` on every document change; fine at beta scale, index by id before
  hundreds of clips.
