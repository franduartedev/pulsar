# Agent Bridge

PULSAR has two related automation surfaces:

1. `pulsar_agent_bridge_cli`, the original headless MVP used to test the JSONL
   protocol, persistence and workspace boundary without Qt or FFmpeg.
2. The desktop bridge described below. It controls the real `DesktopController`
   and therefore the same `EditorSession`, `ProjectDocument`, timeline,
   persistence and playback coordinator used by the graphical interface.

## Desktop mode

The bridge is disabled in every normal desktop session. Start it explicitly:

```text
pulsar-desktop --agent-bridge --workspace C:\QA\pulsar-agent
```

The workspace must already exist. PULSAR creates `agent-bridge.json` inside it.
That descriptor contains a random per-process token and the local endpoint.
The descriptor is deleted at normal shutdown.

The transport is `QLocalServer`: a user-restricted named pipe on Windows and a
user-restricted Unix-domain socket on Linux. It never listens on TCP. Only one
client connection is processed at a time, requests run serially on the Qt UI
thread, a request is limited to 1 MiB and `timeout_ms` is bounded to 1-5000 ms.

Use the packaged client to inject the descriptor token and exchange JSONL:

```text
echo {"id":"status-1","command":"status"} | \
  pulsar-agent-client --workspace C:\QA\pulsar-agent
```

For a persistent connection, write multiple JSON objects (one per line) to the
same client process. Each response echoes `id` and contains either `ok:true` or
a structured `error` with `code` and `message`.

Supported desktop commands:

- `status`
- `open_project {"path":"project.pulsar"}`
- `save_project {"path":"saved/project.pulsar"}`
- `timeline_state`
- `split_clip {"clip_id":20,"position_seconds":4.0}`
- `seek {"position_seconds":0.0}`
- `play`
- `pause`
- `capture_screenshot {"path":"evidence/editor.png"}`
- `get_metrics`

Every file path is relative to the authorized workspace. Absolute paths,
`..` escapes, existing symlink escapes and unsupported extensions are rejected
after canonicalization. There is no shell command, process launch, arbitrary
file read/delete, plug-in loading, TCP listener or remote-control endpoint.

`tools/ci/run-desktop-agent-bridge-qa.ps1` is the executable-level acceptance
test. It opens a real project with video and audio, queries the timeline, splits
a clip at 4 s, saves and reopens it, compares the timeline, plays, reads real
metrics and captures the window without mouse input.

PCM captured by that QA path is taken before the audio device. It validates the
real mixer/playback path, but physical audibility still requires loopback.

## Headless MVP

The MVP is a local, line-oriented JSON interface. It is disabled by default
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

The MVP remains useful as a small protocol/persistence test. It does not own or
mirror the desktop session; desktop automation must use the explicit desktop
mode above.
