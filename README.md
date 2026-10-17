# PULSAR

Native and lightweight video editor for predictable editing.

> **Public Beta RC — PULSAR 0.2.0-beta.1** · Linux (x86_64) only · interface in Spanish

PULSAR is a native desktop video editor written in C++ with Qt 6 and FFmpeg. It aims
at a small, predictable editing workflow: exact timing, no surprises on undo, and an
export you can trust.

## What it does today

- Multi-track video timeline: import, **Insert** at the playhead, **Add to end** (gapless), arrange and overlay
- Split, trim and move clips, with **magnetic snapping**
- Undo / redo
- Independent audio tracks, **detach audio** from a video clip, and 37 original built-in sound effects
- Per-clip gain and mute, waveforms
- Save and reopen projects (`.pulsar`)
- Export to **H.264 / AAC (MP4)**

- **Titles**: static text on its own track, with an optional background box and opacity, same look in preview and export (bundled Inter Bold)
- **Photos and colour cards** (PNG, JPEG, WebP) as clips, **audio fades**, **duplicate**, **frame step**
- A media bin with filters and search, and 37 original sound effects you can audition
- **Import** several files or a whole folder; **drag** media from the library onto the timeline
- **Autosave and recovery** after a crash
- Export presets **Original / 1080p / 720p** (real scaling, never enlarges)
- Direct Viewer move/scale for video and still clips, picture-in-picture, one cross-dissolve transition

Not in this beta: Windows/macOS, GPU rendering, animated titles, additional transition/effect catalogues,
subtitles, keyframes, masks, color tools or proxies. See the
[release notes](docs/release/0.2.0-beta.1.md) for the full list of known limitations.

## Run it

1. Download `PULSAR-0.2.0-beta.1-linux-x86_64.tar.gz` (or the AppImage) from the release page.
2. `tar xzf PULSAR-0.2.0-beta.1-linux-x86_64.tar.gz && cd PULSAR-0.2.0-beta.1`
3. `./pulsar`

No Qt or FFmpeg installation is needed. It was tested on Ubuntu 24.04; the bundle needs
glibc 2.39 or newer (see the release notes). `./pulsar --version` prints the version.
The AppImage normally requires FUSE 2 (`libfuse2`); when FUSE is unavailable, run it
with `--appimage-extract-and-run`.

## Report a bug

Open an issue using the **Bug report** template and include your PULSAR version
(`./pulsar --version`), distro, and the steps to reproduce.

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
