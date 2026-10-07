# PULSAR roadmap (after 0.1.0-beta)

Direction, not a promise: ordering follows what beta testers hit first. Nothing below is
implemented unless it appears in the [CHANGELOG](../../CHANGELOG.md).

| Version | Theme | Main items |
|---|---|---|
| 0.1.x | Beta feedback | Crash/bug fixes from testers; autosave + recovery prompt; relink of missing media; native-looking file dialogs; export loudness safety (limiter); performance counters for large projects. |
| 0.2 | Editing comfort | Mouse-wheel zoom, drag from library, ripple edit, multi-select, faster asset lookup and waveform cache for 100+ clips, memory budget. |
| 0.3–0.5 | Preview and media | Proxies / lower-resolution preview, hardware-adaptive decoding, optional GPU preview and encode, export presets, more input/output formats. |
| 1.0 | Stable editor | A second video track with a compositor, basic transforms, keyframes, transitions, titles, subtitles, simple colour; stable project format with migration; Windows build. |
| 1.x | Platforms | Windows and macOS packaging; installer/AppImage updates. |
| 1.5 | Assisted editing | PULSAR OPERATOR: analyzers feed a planner whose plans are validated, previewed and applied through the same undoable command pipeline. The assistant never edits the project directly. |

## Prerequisites for OPERATOR (hardening list)

Before an assistant may drive edits, the command layer needs: well-defined nested command
groups (RAII scope, one undo step), transaction semantics with guaranteed rollback on failure,
capacity limits for large plans, and atomic preview-then-apply (preview on a copy; apply as
one group).

## Known platform debt (Windows)

`/proc/self/exe` resource lookup, POSIX `open/fsync/rename` in the project writer, path
separators in media locators, `__int128` time arithmetic (GCC/Clang only), and the Linux-only
packaging scripts. See the release notes.
