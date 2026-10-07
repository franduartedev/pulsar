# Architecture

Dependency direction is inward; nothing in an inner layer knows about an outer one:

```text
domain  <-  application  <-  adapters  <-  apps
```

| Module | Path | Role | May depend on |
|---|---|---|---|
| `pulsar_domain` | `src/domain` | Exact rational time (`MediaTime`), timeline/track/clip model, ids. | C++ standard library only |
| `pulsar_application` | `src/application` | Commands + undo/redo (`CommandProcessor`), `ProjectDocument`, project codec, playback coordination and A/V mix, offline export pipeline (`ExportPlan`, renderers, `ExportJob`), waveform cache. | domain |
| `pulsar_project_file` | `src/project_file` | Filesystem adapters: atomic project save/load, export output publishing. | application |
| `pulsar_media_ffmpeg` | `src/media/ffmpeg` | FFmpeg adapters: probe, video/audio decode, MP4 H.264/AAC writer. | application, FFmpeg |
| `pulsar_media_qt_audio` | `src/media/qt_audio` | Preview audio output (Qt Multimedia). | application, Qt |
| `pulsar-desktop` | `apps/pulsar-desktop` | Qt Quick app: `EditorSession` (Qt-free editing/snapping logic), `DesktopController` (QML bridge), QML UI. | everything above |
| `pulsar-engine-lab`, `audio-smoke` | `apps/` | Developer tools for exercising the engine without the UI. | adapters |

Principles that shape the code: exact time (no floating-point timeline math), explicit
ownership and failure states, bounded buffers, one source of truth per fact, and
immutable snapshots for anything that runs off the UI thread (export, waveforms).

Decisions and their reasoning are recorded as ADRs in [`../adr`](../adr/README.md).
Performance budgets and measurements are in [`../performance`](../performance/README.md).
