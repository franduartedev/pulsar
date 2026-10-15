# Visual composition (PULSAR 1.2 / E1)

`visual_track_ids` is the explicit bottom-to-top stack. VIDEO 1 remains the
compatibility anchor that establishes the canvas/FPS and the only visual track
contributing embedded audio.
Editorial gaps and declared source presentation gaps do not paint a layer.
Opaque fitted pictures (including their letterbox bars) cover lower layers;
the initial canvas is opaque black. Titles are composited last.

## Stable canvas and eligibility

`ProjectCanvas` records the editorial dimensions and frame rate when the first
visual clip establishes them (VIDEO 1 takes precedence; an otherwise empty
project can establish it from VIDEO 2). A still-only canvas is provisional until
the first real VIDEO 1 clip arrives (the historical real-video-first rule); then
it freezes. Moving, deleting and reordering clips never alter it. Legacy v1–v5
projects synthesize it from their historical
VIDEO 1 master (real video first, otherwise the first still). Unreleased v6
persists `CANVAS` alongside `VISUAL_TRACKS`; single-track projects whose canvas
still matches their legacy master retain their older on-disk version. An empty
VIDEO 1 can therefore coexist with a valid, persistent canvas and VIDEO 2
content. An audio-only project has no canvas and cannot export visual output.

`VisualRenderContext` validates the stack and canvas and supplies render size,
editorial duration and frame clock. It does not check asset availability.
The exact-time resolver checks only active visual layers. Preview constructs
this context directly; an active unavailable visual layer reports an error,
whereas offline audio or visual clips inactive at the requested time do not
block the picture. Export builds the same context **after** its stricter
whole-output media validation; VIDEO 1 need not currently contain a clip when
another visual track provides the picture and the canvas is established.

## Shared renderer and preview

`OfflineVideoRenderer` owns one decode state per active visual track. Its
sequential `render_frame` entry point serves export; `render_at` serves exact-time
preview without quantizing the editorial timestamp to an output frame grid.
Both use the composition resolver, shared frame-coverage contract, fit geometry,
scaler port and CPU composition operation. A backward discontinuity reopens/seeks
the affected source; inactive tracks release their source and scaled cache.

Multi-track preview uses `AsyncCompositionPreview`: one persistent worker owns
the source factory, scaler and renderer. There is at most one in-flight request,
one replaceable pending request and one completed-result mailbox. A request
contains an immutable snapshot and an exact time; it publishes only a complete
stack or an error. No decoder, conversion or scale operation runs in the UI path.

Seek/edit epochs invalidate pending and completed work. Cancellation is checked
between layers and decode steps; a cancelled epoch cannot publish its surface or
error. Invalidation releases decoder/cache state on the worker even when no new
frame is requested. Shutdown signals under the wait mutex and joins the worker.

Ordinary clock ticks coalesce pending requests rather than repeatedly cancelling
in-flight decoding. A late preview may hold an older **complete** composite with
its actual editorial timestamp; it never mixes layers from different times.
The audio device remains authoritative once running. After an edit/seek, the
first complete composite is handed off before audio is restarted. The desktop
applies titles at the composite's timestamp, then publishes the image.

Single-track playback retains its established worker/mailbox path, using the
same frame-coverage contract. Preview diagnostics expose completed, coalesced,
discarded and replaced counts plus shared renderer layer/decoder/frame and
scale/composition metrics. They are observations, not editorial policy inputs.

## Track edits (E1.6)

Adding an auxiliary visual track appends it to the top of `visual_track_ids`.
Only an empty auxiliary track can be removed; VIDEO 1 is never removable.
Reorder commands move auxiliary tracks above VIDEO 1 in the explicit stack,
independently of the timeline's storage order. Inverse commands restore the
original stack position, so undo/redo and v6 persistence use the same order.
Clip insertion and moving can name any visual track; default Insert/Append
continue to target VIDEO 1. Timeline lanes display top-to-bottom composition
order, with audio lanes below them. VIDEO 1 alone contributes embedded audio.
