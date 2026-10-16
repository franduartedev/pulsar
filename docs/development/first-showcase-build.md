# PULSAR 1.2 — first showcase build

## Reproducible showcase

1. Import a video with audio and a transparent PNG (or a second video).
2. Put the main video on **Video 1**, split it after at least one second, and leave the
   incoming half with source handle. Select either side of the edit and choose
   **Agregar fundido cruzado** in the Inspector.
3. Add **Video 2**, place the secondary source there, select it, then move and resize it
   in the Viewer to the lower-right corner.
4. Add another visual track for the PNG/graphic if desired.
5. Add a title at the playhead, edit its text, and drag it in the Viewer.
6. Use the track-header `− / M / S / +` controls to adjust audio gain, mute, or solo.
7. Preview, export MP4, save, close, and reopen the `.pulsar` project.

The project remains format v6. `TRANSITION` records represent edit relationships and
`TRACK_AUDIO` records represent mixer state; neither is encoded as a media clip.

## Performance checkpoint

Release build, same 1920×1080 / 60-frame FFmpeg benchmark used by E2.1. The supplied
E2.1 reference was 98.4 fps identity, 15.7 fps transformed, and 13.8 fps for three
transformed layers. The final run used the README's generated MPEG-4 fixture:

| Workload | fps | frame time | peak decoders | retained source frames |
|---|---:|---:|---:|---:|
| 1× identity | 92.9 | 10.8 ms | 1 | 2 |
| 1× transformed | 15.7 | 63.8 ms | 1 | 2 |
| 3× transformed | 15.9 | 62.9 ms | 3 | 6 |
| 2 clips + cross dissolve | 18.6 | 53.8 ms | 2 | 4 |
| showcase visual composition | 12.6 | 79.5 ms | 4 | 8 |

Safe optimizations clip transformed traversal to its visible ROI, skip zero-opacity
layers before decode/composition, and fast-path opaque alpha quads. Synthetic transformed
three-layer throughput improved from the E2.1 16.4 fps reference (17.5 fps at the local
pre-fast-path checkpoint) to 20.2 fps. The rotated near-full-canvas case remains dominated
by deterministic bilinear sampling and source-over arithmetic. The 30/24 fps targets were
not reached; lowering sampling quality or changing pixels was rejected.

All decoder/frame bounds remained proportional to active clips (two retained source
frames per FFmpeg decoder); no unbounded render state was introduced.

## Intentional limits

- Cross dissolve only; duration is up to one second and requires an incoming source handle.
- Rotation remains Inspector-driven.
- Static titles only; no animation, subtitle semantics, or templates.
- Track gain/mute/solo only; no buses, automation, plug-ins, or meter in this build.
- CPU composition remains authoritative; no GPU renderer.

## Visual polish checkpoint

The public-facing workspace uses one graphite token palette and a restrained blue
accent for focus, selection, the playhead, and primary actions. The Viewer remains
the dominant region, the Timeline is the second-largest editing surface, and the
Inspector is divided into contextual **VIDEO / AUDIO / EFECTOS** tabs. Unsupported
effects are represented by an explicit empty state rather than inactive controls.

Timeline presentation includes a zoom-aware ruler, full-height playhead, compact
`Vn` / `An` track identities, clear mixer states, bounded asynchronous waveforms,
bounded clip thumbnails, and a dissolve overlay drawn over its real pre-edit
interval. The dissolve remains an edit relationship; no synthetic media clip was
introduced.

Qt offscreen captures were inspected at 1280×720, 1366×768, 1680×1050, and
1920×1080 for the empty workspace, and at 1920×1080 with imported media. The compact timeline toolbar
switches to glyph-first controls below 1250 px; secondary top-bar actions yield
below 1120 px so the project identity and Export action remain readable. The
default 1280×760 workspace also launches without QML warnings.

No media presentation bounds changed during this pass: waveform generation still
admits one active cache job, and timeline thumbnails still use one worker with a
192-entry LRU cache. UI paint paths consume prepared waveform geometry and cached
160×90 thumbnails; they do not synchronously decode media.

Final automated gate (2026-10-16): desktop 55/55, ASAN/UBSAN 41/41, including the
FFmpeg integration, export, waveform, still-image, transition, title-parity, safety,
shutdown, and 100-clip stress coverage.

## Human polish fix 02

The real Viewer interaction failure had two event-lifecycle causes. Move and scale
were gated on `drag.active` from zero-size invisible drag targets, and the first
interactive transform update cleared `previewSource`. That hid the QML item owning
the pointer grab before subsequent move/release events. Viewer gestures now measure
the pointer directly in stable Viewer coordinates and retain the presented frame
until the asynchronously recomposed frame replaces it. A QML event-level test sends
real press/move/release events through the body and all four handles, verifies a live
frame replacement without losing the grab, and covers undo, redo, resize mapping,
and inactive selection behavior.

Waveform columns now render as a symmetric filled peak envelope around a distinct
zero axis, using the larger absolute real extremum per column. Clip-wide
normalization preserves relative peak shape while making quiet recordings legible;
the existing asynchronous cache, one-active-job bound, and 16,384-column geometry
cap are unchanged.

`Separar audio` now accepts clips on every visual track, including VIDEO 2. It places
the same media/source range on the first non-overlapping audio track (or creates one),
copies gain and fades, and mutes the source clip in one grouped command. The v6
representation remains ordinary clips plus the existing mute state, so undo/redo,
save/reopen, preview, waveform generation, and export require no format change.

## Human polish fix 03 — Viewer correctness gate

The Fix 02 Viewer gate was insufficient. Its pointer owner still lived inside the
live selection bounds, its availability still depended on the asynchronous preview
image, move deltas were unnecessarily rotated into clip-local coordinates, and
scale averaged independent X/Y heuristics. Those choices could pass synthetic
events while failing or mapping incorrectly on the Linux desktop.

The Viewer now has one stable, full-canvas interaction overlay above the rendered
frame. A single left-button `MouseArea` with `preventStealing` performs handle-first
hit testing, owns the complete gesture, and stays alive while preview textures are
replaced. It computes the displayed project-canvas rectangle from the persistent
canvas aspect rather than `Image.paintedWidth`/`paintedHeight`. Press snapshots the
canvas rectangle, screen bounds, pointer, and complete initial transform. Every
move is derived from that snapshot: translation is direct screen delta divided by
the displayed canvas dimensions, while scale is the pointer vector projected onto
the press-time corner vector around the renderer's fixed centre anchor. Scale is
finite and clamped before crossing the anchor can mirror the image.

Debug builds log BEGIN, FIRST_MOVE, RELEASE/CANCEL records with clip id and the
captured/proposed geometry. The Qt Quick integration gate uses the production
Viewer hierarchy at 960×540 and checks both transform values and screen bounds for
identity/scaled movement, every corner direction, anchor crossing, resize,
letterbox, pillarbox, live Inspector properties, history coalescing, and frame
replacement during an active pointer grab.

## Human polish fix 04 — Timeline drag lifecycle and workspace split

Two workspace defects remained after Fix 03. First, the Timeline could keep
moving or scrolling after the pointer left it: the track `Flickable` still
accepted mouse drags and coasted on momentum, and a clip/title drag kept
following the pointer (and could autoscroll) far outside the Timeline. Second,
the 16:9 Program Viewer was height-starved — the Timeline took a third of the
window, so the canvas was much narrower than the monitor and sat between large
lateral bars.

**Timeline drag lifecycle.** `TimelinePane` now owns a single pointer gesture.
Clip and title blocks call `beginTimelineDrag(block, x, y)`,
`updateTimelineDrag(x, y)` and `endTimelineDrag()` on their host; the host tracks
whether the pointer is inside the Timeline region and whether an edge is
engaged. Movement and edge-autoscroll are only allowed while the gesture is
live, the left button is held, and the pointer stays inside the region. Leaving
the region freezes the preview and stops autoscroll immediately; release,
cancel, and window deactivation terminate the gesture and clear the single
autoscroll `Timer`, so nothing is left running. Release outside the Timeline
commits the last in-region position rather than jumping. The track `Flickable`
is `interactive: false` (vertical scrolling stays on an explicit wheel handler),
so a pointer that leaves the Timeline can never leave a pan or flick coasting.
Edge-autoscroll re-derives the live preview against the new viewport on each
tick, so a dragged block stays under the pointer while the view scrolls beneath
it. Debug builds log `timeline_gesture` BEGIN / POINTER_EXIT / AUTOSCROLL_START
/ AUTOSCROLL_STOP / END records.

**Workspace split.** `Main.qml` places a draggable divider between the Viewer
module and the Timeline. The split is session UI state only: it is never written
to the project, never touches `VisualTransform`, `CanvasSpec`, a clip transform,
preview, or export. The default split is proportional (`h × 0.28`, clamped to
176–240 px) and re-derives on window resize until the user drags the divider, at
which point their split survives resizing; double-clicking restores the default.
The Viewer canvas is computed from the persistent canvas aspect at whatever
height it is given, so a taller Viewer yields a larger, still exactly 16:9
canvas with no stretch and no crop. A small `Ajustar` / `100%` control switches
between fit and one project pixel per screen pixel; both are display-only and
cannot affect export, `CanvasSpec`, clip transforms, or the saved project.

Measured on the real QML hierarchy (canvas width × height, and per-side lateral
bar), default split:

| Screen | Before | After |
|---|---:|---:|
| 1280×720 | 418×235, bar 139 | 519×292, bar 88 |
| 1280×800 | 508×286, bar 94 | 622×350, bar 37 |
| 1366×768 | 473×266, bar 137 | 581×327, bar 83 |
| 1680×1050 | 825×464, bar 92 | 1008×567, bar 0 |
| 1920×1080 | 878×494, bar 185 | 1092×614, bar 78 |
