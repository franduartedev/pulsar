# PULSAR identity (0.1.0-beta)

Official mark: **Pulse Arc** — a wave crossed by an editing line, electric blue to teal.
Palette from the brand sheet: electric blue `#2563EB`, cyan/teal `#06D6A0`, graphite `#0A0E13`,
slate `#1E293B`, cold white `#F8FAFC`. The interface stays graphite with a blue accent; teal is
reserved for audio and the mark.

## Where it lives

| Use | File |
|---|---|
| Header, About box (`BrandMark.qml`, one component) | `apps/pulsar-desktop/qml/assets/pulse_arc_mark.png` (RGBA, transparent background) |
| Application / AppImage / launcher icon | `packaging/linux/pulsar-256.png`, `packaging/linux/pulsar.svg` (the PNG embedded) |

## Provenance and limits

Both files were cut from the supplied brand sheet **without redrawing**: the mark was lifted off the
dark sheet background with a colour-to-alpha pass (so it sits cleanly on any graphite surface) and the
icon is the sheet's rounded tile. The source is a 1672 px raster, so the mark is ~290 px wide: good
for the header, About box and a 256 px icon, **not** for large print or a hi-dpi splash. Replace the two
files (same names) with the vector master when it exists; nothing else needs to change.
