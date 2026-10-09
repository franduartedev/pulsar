# Visual composition (PULSAR 1.2 / E1)

`visual_track_ids` is the explicit bottom-to-top stack. VIDEO 1 remains the
primary canvas/FPS master and the only visual track contributing embedded audio.
Editorial gaps and declared source presentation gaps do not paint a layer.
Opaque fitted pictures (including their letterbox bars) cover lower layers;
the initial canvas is opaque black. Titles are composited last.

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
