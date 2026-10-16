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
transformed layers. The final local run (the current machine was variable and measured
54–73 fps identity) was:

| Workload | fps | frame time | peak decoders | retained source frames |
|---|---:|---:|---:|---:|
| 1× identity | 65.3 | 15.3 ms | 1 | 2 |
| 1× transformed | 15.2 | 65.7 ms | 1 | 2 |
| 3× transformed | 12.6 | 79.5 ms | 3 | 6 |
| 2 clips + cross dissolve | 16.5 | 60.5 ms | 2 | 4 |
| showcase visual composition | 10.1 | 98.7 ms | 4 | 8 |

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
