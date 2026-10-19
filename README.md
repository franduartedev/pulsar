# PULSAR

Native and lightweight video editor for predictable editing.

> **First Public Beta — PULSAR 0.2.0-beta.1** · Linux x86_64 only · interface in Spanish · GPL-3.0-or-later
>
> This is a beta: expect rough edges, and keep backups of your media and projects.

PULSAR is a native desktop video editor written in C++ with Qt 6 and FFmpeg. It aims
at a small, predictable editing workflow: exact timing, no surprises on undo, and an
export you can trust.

> *Interfaz en español.* PULSAR es un editor de video de escritorio, liviano y nativo
> (C++ / Qt 6 / FFmpeg), en beta pública para Linux x86_64. La documentación del proyecto
> está en inglés; los reportes y comentarios en español son bienvenidos.

## What it does today

- **Timeline:** multiple visual tracks and independent audio tracks; import, **Insert** at the playhead, **Add to end** (gapless), split, trim, move (with **magnetic snapping**), duplicate, delete, undo / redo, frame step
- **Viewer:** direct move/scale of clips, picture-in-picture, display-only *Ajustar* / *100%* modes
- **Titles:** static text on its own track with optional background box and opacity, same look in preview and export (bundled Inter Bold)
- **Images** (PNG, JPEG, WebP) and colour cards as clips; one cross-dissolve transition
- **Audio:** detach audio from a video clip, per-clip and per-track gain / mute / solo, audio fades, waveforms, and 37 original built-in sound effects you can audition before inserting
- **Media bin** with filters and search; import several files or a whole folder; drag media from the library onto the timeline
- **Projects:** save and reopen (`.pulsar`), autosave and recovery after a crash
- **Export** to **H.264 / AAC (MP4)** with presets Original / 1080p / 720p (never enlarges); export refuses to overwrite media files the project uses
- Preview and export share the same composition plan, so what you see is what you export

## Known limitations

- **Linux x86_64 only.** No Windows, macOS or mobile builds.
- **Interface is Spanish only**, and the file picker for *Abrir proyecto* is the toolkit's own (light, English).
- **Preview can be slow** with transformed or multi-layer composition (CPU only, no GPU rendering or proxies). Export is authoritative, and slower on heavy projects. Not tested with 4K / HEVC / long footage.
- No animated titles, subtitles, keyframes, masks, colour tools or speed control; cross dissolve is the only transition.
- No audio limiter or meters: summing several loud tracks can clip.
- No relink UI for missing media, no recent-projects list, no multi-selection.
- Export is MP4 / H.264 / AAC only; there are no vertical (9:16) canvas presets, the canvas follows the first clip.
- Images: PNG, JPEG and WebP only (no SVG, RAW, animated GIF/WebP).
- A project with only audio cannot be exported.

**To be confirmed (feedback welcome):** two behaviours depend on your footage and are only
checked on the maintainer's test material, so treat them as *a confirmar* rather than guaranteed:

- Transparent PNG overlays (alpha) worked in the maintainer's preview/export checks.
- Vertical phone video showed with the correct orientation in the maintainer's checks, but container / codec / rotation-metadata combinations vary by device.

If either misbehaves for you, please [open an issue](../../issues/new/choose) with the source file details.

The full list is in the [release notes](docs/release/0.2.0-beta.1.md); plans are in
[`docs/release/roadmap.md`](docs/release/roadmap.md).

## Requirements

- Linux **x86_64** with **glibc 2.39 or newer** (built and tested on Ubuntu 24.04; Linux Mint 22 and Fedora recent releases should qualify). **Ubuntu 22.04, Linux Mint 21 and Debian 12 are too old** and will fail with a `GLIBC_2.39 not found` error.
- An X11 or Wayland desktop session with working graphics drivers and audio output.
- The **AppImage** needs FUSE 2 (`libfuse2`; on Ubuntu 24.04 the package is `libfuse2t64`). Without it, run with `--appimage-extract-and-run`.
- No Qt or FFmpeg installation is needed; both are bundled.

## Run it

1. Download `PULSAR-0.2.0-beta.1-linux-x86_64.tar.gz` (or the AppImage) and `SHA256SUMS` from the [Releases page](../../releases), and verify with `sha256sum -c SHA256SUMS`.
2. **tar.gz (recommended):**
   ```sh
   tar xzf PULSAR-0.2.0-beta.1-linux-x86_64.tar.gz
   ./PULSAR-0.2.0-beta.1/pulsar
   ```
3. **AppImage:**
   ```sh
   chmod +x PULSAR-0.2.0-beta.1-x86_64.AppImage
   ./PULSAR-0.2.0-beta.1-x86_64.AppImage        # or add --appimage-extract-and-run without FUSE
   ```

`--version` prints `PULSAR 0.2.0-beta.1`. Because the bundle ships its own FFmpeg, avoid opening
media from untrusted sources while in beta.

## Report a bug

Open an issue using the **Bug report** template and include your PULSAR version
(`./pulsar --version`), distro, X11 or Wayland, CPU/GPU/RAM, the media involved and
the steps to reproduce; running from a terminal and pasting its output helps a lot.
Ideas and general beta feedback go in the **Feedback** template.

## Roadmap

Post-beta plans live in [`docs/release/roadmap.md`](docs/release/roadmap.md) and
the changes of each release in [`CHANGELOG.md`](CHANGELOG.md).

## Build from source

Requirements (Ubuntu 24.04 package names): `cmake ninja-build g++ pkg-config`,
Qt 6 (`qt6-base-dev qt6-declarative-dev qt6-multimedia-dev` plus the QML modules
`qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-dialogs qml6-module-qtquick-layouts qml6-module-qtquick-window qml6-module-qtquick-templates qml6-module-qtqml-workerscript`),
and FFmpeg development files (`libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libswresample-dev`).

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DPULSAR_BUILD_DESKTOP=ON
cmake --build build
ctest --test-dir build --output-on-failure          # QT_QPA_PLATFORM=offscreen on a headless machine
./build/apps/pulsar-desktop/pulsar-desktop
```

Core-only (no Qt/FFmpeg, domain + application tests): add `-DPULSAR_BUILD_FFMPEG=OFF` and
leave `PULSAR_BUILD_DESKTOP` off. Sanitizer build: `-DPULSAR_ENABLE_ASAN=ON -DPULSAR_ENABLE_UBSAN=ON`.
To make a release bundle see [`packaging/linux/README.md`](packaging/linux/README.md).

## Architecture

```text
domain  <-  application  <-  adapters (FFmpeg, Qt audio, project files)  <-  desktop app (Qt Quick)
```

`domain` has no external dependencies (exact rational time, timeline model).
`application` owns commands/undo, project documents, playback coordination, and
the export pipeline without depending on Qt or FFmpeg. See [`docs/architecture`](docs/architecture/README.md)
and the ADRs in [`docs/adr`](docs/adr/README.md).

## Contributing

Run the tests before sending changes and keep modules' dependency direction. The
engineering rules are in [`docs/development/engineering.md`](docs/development/engineering.md).

## License

PULSAR is free software, released under the **GNU General Public License v3.0 or later** (`GPL-3.0-or-later`); see
[`LICENSE`](LICENSE). It bundles FFmpeg (built with `--enable-gpl --enable-version3`), x264 and Qt: the components,
their licences and where to get the corresponding source are in [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
