# Changelog

Format: [Keep a Changelog](https://keepachangelog.com/). Versions follow `MAJOR.MINOR.PATCH[-label]`;
the version is defined once, in the root `CMakeLists.txt`.

## 0.1.0-beta

First public beta (Linux x86_64). History before this release is not itemised.

### Added
- **Titles V0**: *Título* button or key *T* adds a static text at the playhead on a dedicated title track (text, size, colour, alignment, shadow in the Inspector; move/trim/delete; one undo step per edit; saved in the project, format v3 only when titles exist). Preview and export use the same rasteriser and compositor. Font: Inter Bold, SIL OFL 1.1 (`assets/fonts/`).
- **Batch import**: multi-file import, folder import ("Importar una carpeta entera…" or a folder path), duplicates skipped, progress and cancel; audio in a batch goes to the library only.
- **Drag and drop** from the library onto the timeline with a drop ghost, snapping and a reason when the drop is refused.
- **Autosave and crash recovery**: a snapshot every 30 s (override `PULSAR_AUTOSAVE_MS`) off the UI thread; on the next start PULSAR offers *Recuperar* / *Descartar*. Cleared on save, open and close.
- **Export presets** Original / 1080p / 720p with a real Lanczos scaler (libswscale); the picture is fitted, never enlarged. Mixed-size clips are scaled in the export.
- **Audio output safety**: a shared soft-knee output stage in preview and export, so the mix can no longer exceed full scale or differ between preview and file (not a look-ahead limiter).
- Thumbnails on clips and in the library.
- **Eliminar y cerrar hueco** (ripple delete, right-click menu or *Shift+Supr*): removes the clip and moves every clip, on any track, that starts at or after its end earlier by exactly its length. One undo step; refused (nothing changes) if a later clip would land on one that stays.
- *Q* / *W* trim the selected clip's start / end to the playhead; *Alt+←* / *Alt+→* select the previous / next clip.
- One thumbnail per video clip (decoded off the UI thread by a single worker, bounded cache of about 11 MB).
- Silencing a clip's audio is now obvious: a speaker icon on every clip with audio, an *Silenciar audio / Activar audio* button in the Inspector, key *M*, and a right-click clip menu (Cut, Mute, Detach audio, Delete).
- **Snap** magnet toggle in the timeline toolbar (key *N*); overlap protection stays on when snapping is off.
- Ctrl + mouse wheel zooms the timeline around the pointer; Shift + wheel pans.
- Multi-video timeline on a single video track: import, Insert at playhead, Add to end (exact, gapless).
- Split, trim and move with magnetic snapping (8 px, exact result) and collision stop.
- Undo / redo, including grouped edits (e.g. Detach audio is one step).
- Independent audio tracks, Detach audio from video clips, 25 built-in original SFX.
- Per-clip gain and mute, waveform display, zoom/pan/fit, ruler with seek.
- Project files (`.pulsar`), atomic save, reopen; clips with missing media open as offline.
- H.264 / AAC MP4 export with progress, cancellation and atomic publish.
- Save / Save As / Export use a typed-path prompt (works without native dialogs).
- Unsaved-changes guard when closing the window or opening another project.
- `--version`, application metadata, `.desktop` entry and icon, portable Linux bundle.

### Fixed
- Export did not clamp the mixed audio at all while the preview hard-clipped; both now use the same soft stage.
- A grouped edit larger than the history capacity could lose part of itself from the undo history; whole groups are now kept or dropped together.
- Clips starting before a zoomed view no longer paint over the track headers; four or more tracks stay reachable (compact lanes, scroll indicator).
- Export of audio whose source range is not aligned to the sample grid.
- Trim gestures used pointer coordinates offset by the track-header width.
- Audio import failed when the playhead was past the length of a short audio file.
- Small memory leak per audio decode session.
- Built-in sounds were reported as offline after reopening a project from a packaged build.
- User-facing errors no longer expose internal exception text.

### Known limitations
See [`docs/release/0.1.0-beta.md`](docs/release/0.1.0-beta.md#known-limitations).
