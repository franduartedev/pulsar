# PULSAR roadmap (after 0.1.0-beta)

Direction, not a promise: ordering follows what beta testers hit first. Nothing below is
implemented unless it appears in the [CHANGELOG](../../CHANGELOG.md).

| Version | Theme | Main items |
|---|---|---|
| 0.1.x | Beta feedback | Crash/bug fixes from testers; relink of missing media; native-looking file dialogs; look-ahead limiter; title animation and more fonts; performance counters for large projects. |
| 0.2 | Editing comfort | PNG/logo overlay with alpha (needs an alpha-keeping asset role + compositor lane), video fade, track-level gain/mute/solo, multi-select, `.pulsar` MIME registration, native-looking file dialogs, memory budget. |
| 0.3–0.5 | Preview and media | Proxies / lower-resolution preview, hardware-adaptive decoding, optional GPU preview and encode, export presets, more input/output formats. |
| 1.0 | Stable editor | A second video track with a compositor, basic transforms, keyframes, transitions, titles, subtitles, simple colour; stable project format with migration; Windows build. |
| 1.x | Platforms | Windows and macOS packaging; installer/AppImage updates. |
| 1.5 | Assisted editing | PULSAR OPERATOR: analyzers feed a planner whose plans are validated, previewed and applied through the same undoable command pipeline. The assistant never edits the project directly. |

Audio-specific sequencing is tracked in [`docs/product/audio-fx-roadmap.md`](../product/audio-fx-roadmap.md).

The product direction for the major editor milestones is in
[`docs/release/vision-v1.2-v1.3.md`](vision-v1.2-v1.3.md).

## v1.2 cross-platform acceptance gate

The v1.2 editor is not considered finished until the same release candidate is usable on
Linux and Windows. The gate covers import, timeline editing, preview, Audio FX, project
save/reopen, recovery and MP4 export. Each platform must have a repeatable CI build plus a
manual smoke pass on real hardware; platform-specific packaging or path code must not change
the project format or preview/export semantics.

UI/UX improvements follow the existing visual system: current colors, typography, spacing and
component language remain stable while new controls gain clearer state, validation, keyboard
friendliness and visual feedback.

## Prerequisites for OPERATOR (hardening list)

Before an assistant may drive edits, the command layer needs: well-defined nested command
groups (RAII scope, one undo step), transaction semantics with guaranteed rollback on failure,
capacity limits for large plans, and atomic preview-then-apply (preview on a copy; apply as
one group).

## Known platform debt (Windows)

`/proc/self/exe` resource lookup, POSIX `open/fsync/rename` in the project writer, path
separators in media locators, `__int128` time arithmetic (GCC/Clang only), and the Linux-only
packaging scripts. See the release notes.
