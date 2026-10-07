# Tools

Developer scripts. None of them is needed to build or run PULSAR.

| Script | Purpose |
|---|---|
| `generate_core_sounds.py` | Regenerates the original, deterministic Core Sounds pack (`assets/sounds/*.wav` and `index.json`). Run from anywhere: `python3 tools/generate_core_sounds.py`. |
| `test_core_sounds.py` | Validates the pack (format, duration, index consistency): `python3 tools/test_core_sounds.py`. |

Release packaging lives in [`../packaging`](../packaging).
