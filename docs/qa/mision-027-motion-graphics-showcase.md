# MISIÓN 027 — Motion Graphics Showcase

The `pulsar.desktop.motion_showcase` integration test creates the reproducible
showcase project and its evidence directory. It uses only labelled repository
fixtures (`e6_video_a.mp4`, `e6_video_b.mp4`, and `e6_video_rational.mp4`). No
unverified ZONALERO footage is represented as real game footage.

## Coverage

- Classic, Hero, Lower Third, Stat Card and Chapter/Location presets.
- Timeline-time entry/settled preview captures for each preset.
- Crossfade and Dip to Black in the same project.
- Save, reopen and editable preset identity verification.
- 16:9 and 9:16 MP4 export; portrait dimensions are probed.
- SHA-256 report for the project and both exports.

CI uploads the resulting directory as `pulsar-<platform>-motion-titles-qa`.
It contains the `.pulsar`, both MP4s, ten preview PNGs, and `QA-REPORT.txt`.

## Known limits

The fixtures are short technical clips, not a published ZONALERO devlog. A
future QA pass with approved source footage can rerun the same project recipe
without changing the renderer or title model.
