# ADR 0018 — M9 Project Persistence

- Status: accepted
- Date: 2026-09-17

## Decision

M9 project files use a strict, versioned, line-oriented UTF-8/ASCII-safe
`PULSAR_PROJECT 1` format with `.pulsar` extension. `ProjectCodec` is pure
memory-only application code. It serializes editorial state, normalized
Rational-backed MediaTime fields, explicit TrackId clip ownership, source
locators, immutable media metadata, and allocator high-water marks.

History identities, undo/redo, document generation, playback state, UI state,
cache data, and runtime MediaAvailability are not persisted.

`ProjectRestoreState` is a neutral decoded representation. A validated
`ProjectDocument::restore` factory constructs a complete candidate using real
domain constructors and rejects duplicate IDs, dangling references, invalid
ranges, overlaps, wrong tracks, and invalid allocator high-water marks.
Loaded documents start clean with fresh history identity and generation zero;
a fresh CommandProcessor must be constructed for them.

The outer `FileProjectRepository` owns filesystem access. Relative source
locators are checked relative to the project file directory while the authored
locator remains unchanged. Missing/unreadable media marks the restored asset
Unavailable without removing clips or changing identity; no FFmpeg probing is
performed during load.

Saves encode completely in memory, create a uniquely retried temporary file in
the destination directory, write/fsync/close it, atomically rename it, and
fsync the parent directory. A post-rename directory fsync failure returns
`CommittedDurabilityUncertain`: the new bytes are installed, but durability is
not confirmed. Pre-rename failures leave the destination untouched.

The parser bounds line size, quoted string size, project record count, and
integer syntax. Unknown versions, malformed escapes, extra records, missing
sections, and trailing data are rejected. No SQLite, Qt, FFmpeg, or POSIX I/O
is present in application persistence code.
