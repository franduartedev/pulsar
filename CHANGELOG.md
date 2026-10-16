# Changelog

Format: [Keep a Changelog](https://keepachangelog.com/). Versions follow `MAJOR.MINOR.PATCH[-label]`;
the version is defined once, in the root `CMakeLists.txt`.

## 0.2.0-beta.1

First public beta release candidate (Linux x86_64).

### Added
- Multiple visual tracks, picture-in-picture and direct Viewer move/scale.
- Shared transformed composition for preview and export, static titles and one cross-dissolve type.
- Draggable Viewer/Timeline split with display-only Ajustar/100% Viewer modes.
- Filled asynchronous waveform envelopes and reliable embedded-audio detach.

### Fixed
- A non-default transform on VIDEO 1 updated selection bounds and Inspector state
  but left the displayed decoder pixels full-canvas. Single-track transforms now
  route through the authoritative composition preview.
- Async preview results carry deterministic project/generation/request identity;
  obsolete edit results cannot replace a newer project revision.
- Timeline pan momentum and drag autoscroll stop on pointer exit, release, cancel
  and window deactivation, with one bounded timer and no orphaned work.
- Viewer selection bounds and raster composition derive from shared visual geometry.

### Known limitations
- See [`docs/release/0.2.0-beta.1.md`](docs/release/0.2.0-beta.1.md#known-limitations).

## 0.1.0-beta

First public beta (Linux x86_64). History before this release is not itemised.

### Fixed (release-candidate human gate)
- **Sound audition (▶ in Sonidos)** was silent on a real desktop. It now plays on its own audio stream (a dedicated `QAudioSink`, negotiated like the timeline's engine, with a short lead-in so a device waking from idle does not swallow a click) instead of `QSoundEffect`. The same ▶ again stops it (the button shows pause while it plays), another sound replaces it, and it never touches the project or the playback engine. If the machine has no usable output the status bar now says so.
- **Import dialog**: *Importar medio*, *Ctrl+I* and *Carpeta…* open a PULSAR-styled browser (places, path field, folder listing, multi-select with checkboxes, *Seleccionar todo*, *Mostrar todos los archivos*, *Importar N archivos* / *Importar esta carpeta*, Cancelar/Esc) instead of the toolkit's light dialog, which could not select several files on Qt 6.4. Validation, folder expansion, duplicate skipping, batch progress and drag and drop are unchanged. *Abrir proyecto* still uses the system dialog.

### Added
- **Licence: GPL-3.0-or-later** (`LICENSE`, `THIRD_PARTY_NOTICES.md`). The packages now bundle **our own FFmpeg 6.1.1**
  (`--enable-gpl --enable-version3`, no `--enable-nonfree`) with static **x264** and **dav1d**, instead of Ubuntu's FFmpeg
  with its ~25 extra codec libraries; the bundle is smaller and no longer contains libzvbi (GPL-2.0-only), x265 or Xvid.
  Corresponding source archives are built with each release. Product behaviour is unchanged; JPEG XL (never an
  import format) is the only thing the old build could decode that the new one cannot.
- **Official identity**: the *Pulse Arc* mark (header, About box, application icon and AppImage/desktop icon) replaces the placeholder bars. The artwork comes from the supplied brand sheet; `docs/release/brand.md` records its provenance.
- **Free title placement**: drag the selected title in the Viewer (centre guides snap at the middle of the frame); one undo step per drag; saved in the project (`TITLE_POS`, format version 5 only when a title is placed); preview and export share the rasteriser, so the title lands on the same pixels. The alignment presets (a 3×3 pad in the Inspector) return a title to the grid and *Restablecer* clears a free position.
- **Inspector redesigned**: a context chip (TÍTULO / VIDEO / FOTO / PLACA / AUDIO), grouped sections (Texto · Posición · Estilo; Clip · Audio · Medio), segmented controls for background and fades, steppers for size and opacity, "Nada seleccionado" empty state.
- Sound category filter is one scrollable line, so the list keeps its room on a 768 px screen.
- **Photos and colour cards as clips** (PNG, JPEG, WebP; *Placa de color* in the library): real stills on VIDEO 1, 5 s by default, trimmable to any length, never stretched (fit with bars), same picture in preview and export, EXIF orientation applied, transparency flattened over black. No temporary movie is made. A still-only project is built around at most 1920×1080.
- **Audio fades**: linear fade-in / fade-out per clip (Inspector presets Sin, ¼, ½, 1, 2 s), drawn on the clip, identical in preview and export. Project format version 4 only when a fade or a title look exists.
- **Titles**: background box (dark / light) and opacity; titles duplicate and keep their look.
- **Duplicar** (*Ctrl+D*, toolbar, Inspector): a copy right after the original, one undo step, refused with a clear message when the room is taken.
- **Frame step** (*←* / *→*, *,* / *.*, buttons beside Play): one frame on the clip's own rational frame grid.
- **Library as a media bin**: kind icons, All / Video / Image / Audio filter, name search, clearer empty states. **Sounds**: Spanish names, ten categories, ▶ audition before inserting, 12 new original sounds (37 in total).
- Thumbnails as a backdrop on clips too narrow for a tile; clip names keep their distinguishing end; fade ramps and photo/colour styling on the timeline.
- Export dialog suggests "Mi video.mp4" (never a name that already exists) and the status bar says which file was written. *Acerca de PULSAR* box; one `BrandMark` component for the logo.
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
- A `.pulsar` opened from the command line, "Open with" or dropped on the window was treated as a media file.
- Editing shortcuts no longer fire behind a file dialog or the colour palette.
- Music and sounds in the library can now be placed: *Insertar en el cabezal* (or double-click), or drop them under the last track; an audio track is created when none is free.
- The Inspector described the last library item instead of the selected clip.
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
