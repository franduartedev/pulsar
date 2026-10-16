# PULSAR public beta release candidate baseline

## Identity and status

- **Public version:** `0.2.0-beta.1`
- **Branch:** `develop/v1.2`
- **Packaged source HEAD:** recorded in the release-evidence addendum below after the
  package build commits this baseline.
- **Status:** `AUTOMATED BETA RC READY — HUMAN SIGN-OFF REQUIRED`
- **Platform:** Linux x86_64; Spanish interface.
- **Project schema:** unchanged at v6. Loading compatibility remains v1–v6.

This is an automated release-candidate baseline, not a claim of physical-desktop
validation or publication. Audio detach already has a real human pass. Viewer
manipulation and the complete packaged workflow still require Fran's physical sign-off.

## Product baseline

The supported beta workflow includes video/image/audio import, cutting and arranging,
multiple visual and audio tracks, direct Viewer move/scale, picture-in-picture, static
titles, cross dissolve, embedded-audio detach, filled asynchronous waveforms,
gain/mute/solo, undo/redo, project save/reopen, and H.264/AAC MP4 export.

The Viewer has display-only **Ajustar** and **100%** modes. Its 16:9 project canvas and
the draggable Viewer/Timeline split do not mutate project bytes or visual transforms.
Timeline drag/autoscroll has one bounded owner and stops on exit, release, cancellation,
or window deactivation.

## Viewer preview root cause and correction

`TimelinePlaybackCoordinator::uses_composition()` previously selected composition only
when more than one visual track existed. A transformed VIDEO 1 clip therefore updated
the domain model, Inspector, and selection bounds while its pixels still came from the
identity-only direct decoder path. The transform was neither lost by `CompositionPlan`
nor ignored by the CPU compositor; the authoritative compositor was never selected.

Any non-default `VisualTransform`, visual stack, or transition now routes through the
shared composition renderer. Preview and export consume the same immutable project
snapshot, composition plan, transformed geometry, and CPU compositor. Viewer selection
bounds and raster composition derive from the shared `visual_layer_geometry()` helper.

Async preview uses deterministic identities:

```text
ProjectSnapshot.document_generation
  -> preview generation
  -> request id
  -> CompositionPreviewResult
  -> DesktopController revision acceptance
```

There is one worker, at most one pending request, and one ready-result mailbox. New work
replaces obsolete pending work. Edits invalidate the generation, worker completion
discards stale generations, and the Viewer independently rejects any result whose
project revision no longer matches the active document. No wall-clock ordering is used.
Debug builds log transform mutation, context/plan construction, compositor application,
render completion, rejection/acceptance, and displayed revision; these logs compile out
under `NDEBUG`.

Pixel-level production tests verify move and scale through `ViewerPane.qml`, changed QML
Viewer pixels, black/background pixels outside the transformed layer, and transformed
pixel bounds matching selection geometry within a 2–3 pixel rounding tolerance. Still
images and the shared VIDEO 1/VIDEO 2 renderer path are covered. Preview/export parity
remains covered by the visual-transform, title-parity, transition, offline-renderer, and
FFmpeg export tests.

## Workspace measurements

Offscreen production-hierarchy measurements in **Ajustar** mode:

| Window | Left | Inspector | Central | Timeline | Viewer area | Canvas |
|---|---:|---:|---:|---:|---:|---:|
| 1280×720 | 243 | 269 | 720 | 202 | 696×292 | 519×292 |
| 1280×800 | 243 | 269 | 720 | 224 | 696×350 | 622×350 |
| 1366×768 | 260 | 287 | 772 | 215 | 748×327 | 581×327 |
| 1680×1050 | 290 | 310 | 1032 | 240 | 1008×584 | 1008×567 |
| 1920×1080 | 290 | 310 | 1272 | 240 | 1248×614 | 1092×614 |

Every measured canvas is exact 16:9 within layout rounding and touches the limiting
available dimension. The splitter drag/refit, reset, minimums, project-byte invariance,
and transform invariance are exercised in the real QML hierarchy.

## Build and runtime baseline

- **Compiler:** GCC 13.3.0
- **CMake:** 3.28.3
- **Qt:** 6.4.2
- **Build-host FFmpeg API:** 6.1.1
- **Packaged FFmpeg:** repository recipe builds FFmpeg 6.1.1 with GPLv3-compatible
  configuration and x264; package details are in `THIRD_PARTY_NOTICES.md`.
- **Host assumptions:** Linux x86_64, glibc 2.39 or newer for the current Ubuntu 24.04
  build, and a supported X11/Wayland graphics stack. AppImage may require FUSE 2 or
  `--appimage-extract-and-run`.

Independent tested Release build:

```sh
cmake -S . -B build-release-rc -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DPULSAR_BUILD_DESKTOP=ON -DPULSAR_BUILD_QT_AUDIO=ON \
  -DPULSAR_BUILD_TESTS=ON -DPULSAR_VERSION_LABEL=beta.1
cmake --build build-release-rc -j2
QT_QPA_PLATFORM=offscreen ctest --test-dir build-release-rc --output-on-failure -j2
```

Supported package build (does not publish):

```sh
./packaging/linux/build-release.sh
```

Expected artifacts:

- `dist/PULSAR-0.2.0-beta.1-linux-x86_64.tar.gz`
- `dist/PULSAR-0.2.0-beta.1-x86_64.AppImage`
- `dist/PULSAR-0.2.0-beta.1-source.tar.gz`
- `dist/PULSAR-0.2.0-beta.1-third-party-source.tar.gz`
- `dist/SHA256SUMS`

The exact packaged source HEAD, sizes, and SHA-256 values are recorded in the addendum
after generating artifacts from the commit containing this baseline.

## Automated quality evidence

| Gate | Result |
|---|---|
| Debug desktop/full suite | 56/56 passed |
| Release desktop/full suite | 56/56 passed |
| ASAN+UBSAN core/full suite | 41/41 passed; leak detection enabled |
| Affected sanitized async/QML/desktop set | 5/5 passed; leak detection enabled |
| FFmpeg regressions | 10/10 passed |
| Pixel transform tests | production Viewer move/scale and still composition passed |
| Persistence | schema v1–v6 compatibility and transformed save/reopen passed |
| Preview/export parity | transformed visuals, titles, transitions, stills and MP4 path passed |
| Package checksum verification | recorded in addendum |
| Tar/AppImage version smoke | recorded in addendum |

No tests were disabled or weakened.

## Performance and bounded-work baseline

Release benchmark, 60 frames at 1920×1080 using `tests/fixtures/e6_video_a.mp4`:

| Composition | Throughput | Peak decoders | Peak retained source frames |
|---|---:|---:|---:|
| Default 1 layer | 62.8 fps | 1 | 2 |
| Transformed 1 layer | 15.5 fps | 1 | 2 |
| Transformed 3 layers | 13.0 fps | 3 | 6 |
| Two clips + cross dissolve | 17.1 fps | 2 | 4 |
| Showcase composition | 10.5 fps | 4 | 8 |

The transformed CPU compositor remains the principal known performance limitation.
This milestone deliberately does not add E6, GPU rendering, or a second compositor.
Preview work remains bounded to one worker, one replaceable pending request, and one
replaceable ready result; waveform and thumbnail work retain their existing bounded
workers/caches.

## Known limitations

- CPU-only transformed/multi-layer composition can produce slow high-resolution preview.
- Titles are static and clip rotation is Inspector-only.
- Cross dissolve is the only transition and requires an incoming source handle.
- MP4/H.264/AAC is the only export family.
- No GPU renderer, E6 Adaptive Engine, proxies, effects catalogue, subtitles, keyframes,
  masks, AI, cloud, collaboration, Windows, or macOS release.
- Physical desktop validation of direct transformed pixels and the final package remains
  outstanding.

## Human sign-off checklist

1. Move a selected visual directly in the Viewer.
2. Scale it from all corner handles.
3. Confirm actual pixels follow the selection bounds with no duplicate/full-canvas layer.
4. Confirm Timeline exit/release/cancel/focus loss never coast.
5. Resize and double-click-reset the Viewer/Timeline splitter.
6. Test **Ajustar** and **100%** without changing project state.
7. Separate embedded audio and confirm no doubled audio.
8. Preview titles, transition, waveform, mute/solo/gain.
9. Save, close, reopen, and compare the project visually.
10. Export MP4 and compare key frames and audio semantics with preview.

Do not begin E6 automatically. Do not publish artifacts automatically.

## Release-evidence addendum

To be completed immediately after building the artifacts from this baseline commit.
