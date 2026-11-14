# Agent Bridge MVP

The bridge is a local, line-oriented JSON interface. It is disabled by default
and does not expose a shell, network listener, or arbitrary process execution.
Every path is relative to the directory passed with `--root`.

Build and run it with the normal CMake build:

```text
cmake -S . -B build -DPULSAR_BUILD_AGENT_BRIDGE=ON
cmake --build build --target pulsar_agent_bridge_cli
pulsar_agent_bridge_cli --root ./agent-workspace
```

Send one JSON object per line and read one JSON result per line. A successful
result contains `"ok":true`; failures contain `error` and `message`.

Supported commands:

- `create_project {"name":"project.pulsar"}`
- `create_test_media {"path":"fixture.test"}`
- `import_test_media {"path":"fixture.test","duration":10,"audio":true}`
- `import_media` (same fields, for an existing local fixture)
- `add_clip {"media_id":1,"source_start":0,"source_end":10,"timeline_start":0}`
- `split_clip {"clip_id":1,"position":4}`
- `timeline_state`
- `save_project {"path":"project.pulsar"}`
- `validate_round_trip`
- `close_project`
- `open_project {"path":"project.pulsar"}`

The test media probe is deterministic and intentionally does not claim to be a
real decoder. It makes project/timeline/persistence automation reproducible;
real media probing and playback control are subsequent extensions.

The bridge compares canonical `ProjectCodec` bytes during
`validate_round_trip`, so a changed clip ID, range, asset, or allocator state
fails instead of being reported as a successful reopen.

## Example

```text
type examples/agent-bridge-round-trip.jsonl | pulsar_agent_bridge_cli --root .\\agent-workspace
```

The executable must not be enabled in normal desktop sessions. The next stage
can add an explicit playback adapter and screenshot capability while keeping
headless results separate from graphical validation.
