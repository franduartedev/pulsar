# ADR 0012: M8 visual frame lease boundary

Status: Accepted

M8 is the first milestone that needs decoded image data. M7 intentionally transported only temporal `VideoSample` metadata so playback clocking, scheduling, seek semantics, and worker ownership were not tied to a CPU pixel format. M8 keeps that separation. A visual frame consists of identity/time metadata, an image descriptor, and a storage lease whose lifetime is independent of the reusable decoder frame.

The FFmpeg adapter may expose an adapter-specific `FFmpegVideoFrameLease`, but it must not expose `AVFrame` in application or domain headers. The lease is implemented with FFmpeg refcounted frame references so a decoded visual frame can survive the next `decode_next` call and can outlive the decode session that produced it. This avoids copying pixels only to extend lifetime while preserving an explicit future migration point for GPU-backed storage.

The M8 descriptor preserves the metadata required to make preview decisions later: coded dimensions, current visible dimensions, pixel format name, sample aspect ratio when present, color range, color primaries, transfer characteristic, and color matrix. M8 currently treats visible dimensions as the decoded frame dimensions. Container/display rotation, crop rectangles, interlace handling, alpha semantics, and full color management are not implemented in this gate; unsupported values must not be silently converted into claims of correct rendering.

CPU access is deliberately not provided as `data()` on the lease. M8 adds an explicit `FFmpegPreviewConverter` at the adapter/presentation boundary. It converts one leased FFmpeg frame at a time to an RGBA preview buffer using libswscale for desktop presentation. That buffer is a replaceable preview artifact, not the universal engine frame format, and it must not force future VA-API, NVDEC, Vulkan, or zero-copy paths through CPU RGB.

The converter preserves source timing and descriptor metadata alongside the preview buffer. M8 SDR preview uses libswscale defaults and reports the preserved color metadata; it does not claim a complete color-managed or HDR-correct pipeline.
