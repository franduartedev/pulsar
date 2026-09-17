# ADR 0013: M8 desktop media viewer

Status: Accepted

M8 introduces the first real PULSAR desktop application as an opt-in Qt Quick target, `pulsar-desktop`. The core layering remains unchanged: domain and application do not include Qt, FFmpeg, Vulkan, SQLite, or Linux-specific UI APIs. The desktop app is an outer adapter that depends on application timing primitives and the FFmpeg media adapter.

The M8 viewer is a source media viewer, not an editing timeline. It can open a local video path from CLI, drag/drop, or the path field, show the first decoded frame, display real probe/stream metadata, scrub the source range, play, pause, resume, and close cleanly. It deliberately does not expose export, editing timeline tracks, effects, transforms, audio, project persistence, proxies, or hardware decode.

The visual path is explicit. FFmpeg decoding yields a refcounted `FFmpegVideoFrameLease`; `FFmpegPreviewConverter` converts one leased frame at a time to an RGBA CPU preview buffer via libswscale. Qt receives a copied `QImage` presentation artifact through an image provider. This is a desktop preview boundary, not an engine-wide frame model, and it does not preclude later VA-API, NVDEC, Vulkan images, storage leases, or zero-copy paths.

Thread ownership is fixed for M8. The GUI thread owns QML, QObject state, and the image provider API. A single desktop media worker thread owns `FFmpegVideoDecodeSession`, physical decode, presentation seek, and preview conversion. The worker never manipulates QML objects directly; it publishes results back to the GUI thread through queued `QMetaObject::invokeMethod` calls. The controller joins the worker during destruction.

Backpressure is intentionally small. The command side stores at most one pending worker command; seeks replace earlier pending work, and reads are not queued when another command is pending. The visual side keeps only the current provider image and at most one decoded pending frame waiting for its presentation timestamp. Stale generations are rejected before presentation. M8 reports basic diagnostics for displayed frames, stale frames, read/seek counts, conversion time, and seek-to-visible latency.

Playback uses application `PlaybackTransport` for logical play/pause/seek position. The UI polling rate is not temporal authority. During playback the controller displays a decoded pending frame only after the transport-derived source target reaches the frame timestamp; a late preview indicator is shown when visible video lags behind logical playback by a small diagnostic threshold.

Color scope is SDR preview. The adapter preserves pixel format and color metadata in the descriptor and inspector. The current libswscale conversion uses the available decoded frame metadata for reporting but does not claim a complete color-managed or HDR-correct pipeline. HDR/10-bit correctness and display color management are deferred.

Some Qt 6.4 distributions can provide Qt Quick while omitting the runtime QML import directory for `QtQml.WorkerScript`, even though QtQuick's dependency chain may request the module. PULSAR embeds a minimal QML import fallback for that absent module so development builds can start without host package surgery. Normal Qt installations continue to use their real runtime modules first; the app itself does not use WorkerScript.
