# Changelog

Format: [Keep a Changelog](https://keepachangelog.com/). Versions follow `MAJOR.MINOR.PATCH[-label]`;
the version is defined once, in the root `CMakeLists.txt`.

## 0.1.0-beta

First public beta (Linux x86_64). History before this release is not itemised.

### Added
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
- Export of audio whose source range is not aligned to the sample grid.
- Trim gestures used pointer coordinates offset by the track-header width.
- Audio import failed when the playhead was past the length of a short audio file.
- Small memory leak per audio decode session.
- Built-in sounds were reported as offline after reopening a project from a packaged build.
- User-facing errors no longer expose internal exception text.

### Known limitations
See [`docs/release/0.1.0-beta.md`](docs/release/0.1.0-beta.md#known-limitations).
