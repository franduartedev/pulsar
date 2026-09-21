# PULSAR Beta Starter Content — Research & Strategy

- Status: research only. No binaries committed. No production code modified. No assets integrated.
- Date verified: 2026-09-21 (all license checks below).
- Method: web research only (primary license pages + repo `OFL.txt`/`METADATA.pb` where noted). Nothing downloaded.
- Parallel tracks: audio gaps + music · fonts · LUTs + overlays/textures · titles + transitions + demo footage + sample project.
- Absolute rule applied throughout: **"free download" ≠ redistributable.** Every source classified
  `APPROVED` / `APPROVED WITH ATTRIBUTION` / `RESEARCH ONLY` / `REJECT`. Anything ambiguous = rejected.

---

## 0. Finding: the "191-sound pack" has no manifest in-repo

The brief describes a prepared audio research pack (~191 sounds, ~41 MB, mostly CC0 + one PD NASA source,
covering UI / Impacts / Whooshes / Foley / Technology / Ambience / Cinematic / Misc). **No manifest file for
it exists in this repo.** The only in-repo traces are contextual references in
`docs/research/video-editor-competitive-study.md:10,223,290` and `docs/architecture/pulsar-risk-register.md`
(R-29: 191 sounds with no index/search/favorites/lazy-peaks). The category list is therefore taken as given.

Consequences:

1. Gap analysis below is against the *described* category coverage, not audited files. Re-audit against real
   files at curation time.
2. Before shipping anything, the pack needs what R-29 already demands: `index.json` (ID, tags/category,
   durations, precomputed peaks), search + favorites + audition-in-place — otherwise 191 files land as a raw
   folder and findability collapses. The metadata schema in §12 is designed to satisfy R-29.
3. Keep per-file provenance (sound ID + page URL + license snapshot) — Freesound/BigSoundBank licenses can
   change on re-upload.

---

## 1. RESULT

**PARTIAL** (by design — this is a research deliverable, not an integration):

- Concrete, evidence-backed Beta 1 starter set defined (§11).
- Every asset family classified BUILD vs DOWNLOAD vs DO NOT SHIP (§10).
- Approved vs rejected sources with license reasoning (§5, §6).
- Package-size budgets estimated (§9).
- Library metadata + legal-manifest structure defined (§12, §13).
- What remains (post-research work): curate/transcode the ~30 gap-fill SFX, subset fonts, implement
  parametric LUTs/titles/transitions/overlays, write the Welcome Project generator. All classified as
  BUILD tasks for later milestones — explicitly **not** done here.

---

## 2. CURRENT AUDIO PACK — coverage and meaningful gaps

Assumed coverage: UI, Impacts, Whooshes, Foley, Technology, Ambience, Cinematic, Misc (~191 files, ~41 MB).

Meaningful gaps (needed-for-beta = yes), i.e. things generic CC0 packs systematically lack and editors
actually reach for:

| Gap | Why it matters | Fill source (all CC0/PD, per-file verification required) |
|---|---|---|
| Camera shutter (2–3 variants) | Snapshot/thumbnail capture, photo inserts | Freesound CC0 filtered search |
| Record/snapshot beeps, countdown blips | Tally/record-start, autofocus confirm | NASA Quindar tones, Sputnik beep (PD) |
| Typing / mouse / keyboard clicks (isolated) | Screencast/tutorial editing = primary beta use | Freesound CC0; BigSoundBank CC0 |
| Notification pings (short OS-style pops) | Generic packs ship long chimes, not <1.5 s pings | Freesound CC0; OpenGameArt CC0 |
| Doors (open/close/slam/creak/latch) | Foundational foley, chronically under-represented | USC/Sunset "Red Library: Doors" via Internet Archive (CC0, ~40 cues) |
| Footsteps (loopable surface set, ≥4 steps) | Generic packs ship 1–2 one-shots | Freesound CC0 footstep sets (concrete/wood/grass/gravel) |
| Room tone (seamless 30–60 s neutral) | Typically MISSING; needed for dialogue gap-filling. Best recorded fresh | Record fresh, else BigSoundBank/Freesound CC0 |
| Wind / rain / thunder (gusts + cracks, not one rain loop) | Most-requested nature bed | USC "GOLD TAPE 46 Thunderstorm & Rain" via IA (CC0, 12 cues) |
| Vehicles (pass-by, idle, city bed — 2–3 only) | B-roll staple | BigSoundBank CC0; Freesound CC0 |
| Broadcast/radio static, tuning sweep, censor beep | Classic transition/mask device | NASA radio/plasma static (PD — use NASA originals, not the CC BY 3.0 space-audio.org mirror) |
| Glitches (3–5 ultra-short digital errors) | Modern staple; "Technology" packs skew to modem sounds | Freesound CC0; OpenGameArt CC0 |
| Subtle transitions (0.3–0.8 s ticks/soft swooshes) | Highest value-per-byte gap; packs ship big cinematic whooshes only | Freesound CC0; or synthesize |
| Short risers/drops/stingers (1–3 s, 3–5 total) | Packs skew to 5–15 s trailers; beta needs logo/button stings | Freesound CC0; OpenGameArt CC0; 1 NASA Saturn-radio swell (PD) |

Marginal/defer: phone rings (1 max or defer), cloth/rustle (1–2 or defer — USC Doors item has one),
computer-equipment hums (1 fan + 1 printer max; reject trademarked startup chimes).

**Net: add ~25–35 files.** Transcode everything to Opus (SFX mono 32–64 kbps, beds stereo 64–96 kbps;
a 0.5 s click ≈ 8–15 KB). Whole gap-fill ≈ **1–2 MB**. Normalize loudness (beds −16 LUFS, hits −14 LUFS);
make room tone + rain seamlessly loopable.

**Quality > quantity:** do not add categories merely because they exist elsewhere; the table above is the
bounded list. If the real-file audit shows any row already covered, drop it rather than duplicating.

---

## 3. SHIP WITH BETA (core installer — offline, ~44–48 MB total)

| Item | Count | Size budget | License posture |
|---|---|---|---|
| Curated SFX (existing ~191 + ~30 gap-fill) | ~220 | ~42–43 MB (existing 41 MB + 1–2 MB Opus gap-fill) | CC0 + NASA PD; per-file provenance |
| Fonts (5 OFL families, variable TTF, latin+latin-ext) | 5 | ~1–3 MB (subset further if pressed) | OFL-1.1, ship 5× `OFL.txt` |
| LUTs/looks (8 parametric, generated 16³ `.cube` at build) | 8 | ~1–1.5 MB (or 0 bytes if applied as shader math + export-on-demand) | Original math, zero third-party rights |
| Overlays/textures (procedural GLSL: grain, vignette, scanlines, gradients, stylized leaks/specks) | 7 | ~0 bytes shipped (<30 KB source) | Original code |
| Title presets (built-in JSON+renderer presets) | 8 | ~50–150 KB | Original |
| Native transitions (shader kernels) | 7 | ~10–30 KB | Original |
| Music tracks | 0 | 0 | — (§8) |
| Demo footage | 0 bundled | 0 | — (download-on-first-run, §7) |
| Welcome Project (JSON + generation script + ATTRIBUTION.md) | 1 | ~50 KB | Original + references |

---

## 4. OPTIONAL DOWNLOAD PACK (never inflate the installer)

- **Demo media cache** (user-opt-in, first-run "Fetch demo media"): NASA/USGS/NOAA PD clips + Sintel /
  Tears of Steel excerpts (CC BY 3.0, credit scroll preserved). 50–300 MB, cached outside the installer
  in `~/Pulsar/Media/`, keyed by content-hash + source URL. Project opens without it (synthetic
  placeholders, identical edit points/durations) — network never blocks first launch.
- **Full-i18n font pack**: cyrillic/greek/vietnamese subsets stripped from core.
- **Photographic texture pack (v2+)**: only from Poly Haven / ambientCG (CC0) or per-verified
  Wikimedia-CC0 files, each with a provenance record. Cap 2K, JPEG unless alpha needed.
- **32³ HQ LUT export**: on-demand, not bundled.

---

## 5. BUILD OURSELVES (better created internally — zero license/storage risk)

- **8 LUT looks** (Warm, Cool, Film Soft, High Contrast, Muted, B&W, Night, Vintage): source of truth =
  ~50 lines of parametric recipe code; `.cube` (open text format, IRIDAS/Adobe spec 2013, no royalties)
  baked at build (16³/17³) and offered as user export. Generic names only — no "Kodachrome"/"Tri-X"/
  "Vision3"/"Alexa" trademarks.
- **7 overlays**: grain (hash noise × luma mask), vignette (radial smoothstep), CRT/scanlines/flicker,
  gradient backgrounds, stylized light-leak blobs, speck-dust — each 5–25 lines of GLSL.
- **8 titles** (§7) and **7 transitions** (§7): native renderer presets / shader mix kernels.
- **Test media**: ffmpeg `testsrc`/`smptebars`/`mandelbrot` + tone, generated locally (~2–5 MB cache,
  deterministic, zero rights) for tests/screenshots/CI.
- **Room tone**: record fresh (highest legal + quality safety).
- **Music**: commission original CC0 loops later with signed waiver, if ever (§8).

---

## 6. LICENSED SOURCES — approved list + summary

| Source | License | Redistribution | Attribution | Verdict | Use for |
|---|---|---|---|---|---|
| Freesound CC0 corpus (per-sound filter) | CC0 1.0 | Yes | No (credit appreciated) | APPROVED (per-sound check mandatory; snapshot ID+license at curation) | All SFX gaps |
| BigSoundBank / LaSonotheque (J. Sardin) | CC0 / PD dedication | Yes | No (appreciated) | APPROVED | Doors, nature, vehicles, phones, clicks |
| USC/Sunset "Red Library: Doors" + "GOLD TAPE 46" via Internet Archive | CC0 1.0 (`Usage: CC0 1.0`) | Yes | No | APPROVED | Doors; rain/thunder (transcode FLAC→Opus) |
| NASA audio (historical sounds / SVS) | U.S. Public Domain (17 USC §105) | Yes | Requested "Courtesy NASA", not required; no endorsement implication; skip third-party-credited frames, logos, astronaut-publicity issues | APPROVED | Beeps, radio static, launch, space beds/stingers |
| OpenGameArt CC0 subset | CC0 1.0 | Yes (CC0 items only) | No | APPROVED (CC0 filter; never bulk-scrape BY/BY-SA/GPL items) | Glitches, UI, stingers |
| Inter · Oswald · JetBrains Mono · Bebas Neue (google/fonts) | OFL-1.1, no RFN | Yes (ship `OFL.txt` per font) | No (license file mandatory, §13) | APPROVED | Sans / condensed-display / mono / distinctive title |
| Source Serif 4 (google/fonts) | OFL-1.1, RFN **`Source`** | Yes verbatim | No | APPROVED WITH RFN CAVEAT (bundle unmodified; any fork must rename) | Serif/cinematic |
| Blender open movies (Sintel, Tears of Steel) | CC BY 3.0 | Yes (with credit scroll + license link) | **Yes** | APPROVED WITH ATTRIBUTION (reference/download-on-demand only — 270 MB–27 GB, never bundle) | Demo/tutorial media |
| NASA/NOAA/USGS imagery & video | U.S. Public Domain | Yes | Courtesy credit requested | APPROVED WITH ATTRIBUTION-style courtesy + caveats | Demo media, space backgrounds |
| Poly Haven · ambientCG | CC0 1.0 | Yes (explicit "even in a product you sell") | No | APPROVED | Future photographic textures (v2+, not v1) |
| OCIO (configs/infra) · ACES (pipeline) | BSD-3 / Apache-2.0 | Code/configs yes | Notices required | APPROVED as infrastructure, NOT as look sources | Color pipeline, not creative LUTs |
| Musopen Chopin via Archive.org CC0-marked item | CC0 1.0 (this item only) | Yes | No | APPROVED but DISCOURAGED (wrong genre, per-recording verification burden) | At most 1 Opus miniature, or nothing |
| Lutify.me free LUTs | CC BY-SA 4.0 | Yes with attribution + ShareAlike | **Yes + ShareAlike** | RESEARCH ONLY (copyleft burden outweighs 5 LUTs) | Taste reference only |
| FreshLUTs | Site claims CC0 (crowdsourced uploads) | Technically yes, provenance risk | No | RESEARCH ONLY (hand-pick ≤2 as re-implementation reference max; never bulk) | Taste reference only |
| G'MIC presets · vkdt | CeCILL/copyleft-adjacent · mixed BSD/GPL | No (per-preset grant unclear) | — | RESEARCH ONLY | Inspiration, not bytes |

Direct license URLs (verified 2026-09-21): CC0 <https://creativecommons.org/publicdomain/zero/1.0/>,
CC BY 4.0 <https://creativecommons.org/licenses/by/4.0/>, BY-SA 4.0
<https://creativecommons.org/licenses/by-sa/4.0/>, OFL-1.1 <https://openfontlicense.org>,
OCIO <https://opencolorio.readthedocs.io/en/latest/aswf/license.html>,
Blender <https://www.blender.org/about/license>,
NASA media <https://www.nasa.gov/nasa-brand-center/images-and-media/>,
Poly Haven <https://polyhaven.com/license>, ambientCG <https://docs.ambientcg.com/license>,
BigSoundBank <https://bigsoundbank.com/licenses.html>, Freesound FAQ <https://freesound.org/help/faq>,
Unsplash <https://unsplash.com/license>, Pexels <https://www.pexels.com/license>,
Pixabay <https://pixabay.com/service/license-summary>, Mixkit <https://mixkit.co/license>,
Lutify.me <https://lutify.me/license>, FreshLUTs <https://freshluts.com/termsandconditions>,
textures.com <https://www.textures.com/about/terms-of-use>, Krita <https://krita.org/en/about/license>.

---

## 7. REJECTED SOURCES (+ reason — do not rediscover)

1. **BBC Sound Effects (RemArc)**: NC personal/research only; commercial via paid license; no redistribution.
2. **Pixabay / Pexels (SFX, music, photo, video)**: custom licenses ban standalone/stock redistribution —
   the exact bundling act. (Pre-2019 Pixabay CC0 archaeology per-file: not worth it.)
3. **Mixkit**: use-only license; bans sublicense/resale/stock-aggregation/competitive reuse. Sync-in-video ≠ bundle-in-app.
4. **Uppbeat**: single-use sync, download caps, credit-link; no redistribution.
5. **Epidemic Sound**: subscription sync-only; standalone/cover/remix bans; new-use rights lapse on cancel.
6. **Free Music Archive (bulk)**: dominantly BY-NC/ND + personal-NC FMA license (no redistribution/derivatives).
7. **Incompetech free (CC BY) for beta**: legally redistributable with credit but rejected on product grounds —
   per-copy/per-export attribution burden, overexposure, 2025–26 ContentID/Spotify flag history.
8. **Wikimedia Commons non-CC0 files (BY/BY-SA/GFDL/NC/ND)**: attribution/ShareAlike chains; GFDL impractical
   for SFX. Only CC0/PD-marked files, per-file verified.
9. **"Free LUT pack" blogs/roundups** (PHLEARN, MyLuts, Sony-LUTs, ON1, RocketStock-style, PremiumBeat lists):
   use-licensed, redistribution prohibited or silent. Taste reference only.
10. **Blender Filmic/AgX LUTs+config · darktable/ART/RawTherapee styles · GIMP/Krita presets**: GPL/CeCILL —
    copying bytes = copyleft asset ingestion. Re-implement generic math from textbooks instead.
11. **textures.com / GameTextures / Texturelabs / Plaintextures / locationtextures**: rendered-use OK,
    redistribution-as-pack/bundle explicitly forbidden.
12. **Unsplash / Pexels photos as shippable overlays**: standalone/competing-service bans, takedown
    propagation, API attribution strings. Users may import their own; PULSAR must not ship them.
13. **Motion Array / Envato / Artgrid / Filmora store templates**: proprietary, no redistribution.
14. **Coverr / Mixkit / Pexels / Pixabay video for installer bundling**: end-product use allowed,
    standalone distribution prohibited. Users may import; do not bundle.
15. **Novelty transition packs (spin/zoom/glitch/shake)**: license debt + zero technical need (all are native
    mix kernels) + support cost. Rejected for Beta 1.
16. **Bundling Blender movies or NASA footage in the installer**: legally permitted, size-prohibitive
    (270 MB–27 GB). Reference/download-on-demand instead.
17. **Ubuntu fonts**: Ubuntu Font License (not OFL; Debian/Fedora classify non-free). Do not ship.
18. **dafont/1001fonts mirrors without explicit OFL**; stale Apache Roboto copies; **system-font dependence**
    as a strategy (proprietary EULAs forbid redistribution; nondeterministic rendering breaks portability).
19. **YouTube rips, marketplace previews, "royalty-free" packs without written redistribution grant**:
    all-rights-reserved by default. Treat as NOT redistributable.
20. **NASA logos/insignia, astronaut faces with publicity issues, third-party-credited frames**: never ship;
    PD imagery only with credit + endorsement disclaimer.

---

## 8. FONTS — recommended starter set

| Role | Family (designer) | License / RFN | Format | Size (full TTF → latin subset) |
|---|---|---|---|---|
| Clean sans (default: subtitles, lower thirds, social) | **Inter** (Rasmus Andersson) | OFL-1.1, no RFN | Variable `Inter[opsz,wght].ttf` + Italic | ~700 KB → ~200 KB |
| Condensed/display (space-efficient thirds, headlines) | **Oswald** (V. Adams, K. Gajjar, Cyreal) | OFL-1.1, no RFN | Single VF `Oswald[wght].ttf` | ~200 KB → ~55 KB |
| Serif (cinematic, editorial, credits) | **Source Serif 4** (F. Grießhammer/Adobe) | OFL-1.1, RFN **`Source`** | Variable + Italic (opsz 8–60) | ~1 MB → ~200 KB |
| Monospace (technical/UI-style, timecodes) | **JetBrains Mono** (JetBrains/Nurullin/Bulenkov) | OFL-1.1, no RFN (v2.002+; ignore obsolete Apache-era copies) | Variable + Italic | ~600 KB → ~150 KB |
| Distinctive title (thumbnails, openers) | **Bebas Neue** (R. Tsunekawa/Dharma Type) | OFL-1.1, no RFN (note: "Bebas Neue" is a Dharma Type trademark — bundle unmodified, no endorsement implication) | Single static | ~175 KB → ~50 KB |

**Total: ~2.1–2.9 MB full TTF / ~0.5–0.8 MB latin subsets.** Ship variable TTFs in the installer
(broadest toolkit support); pre-instance 3–4 statics (Regular/Medium/SemiBold/Bold) at build for render
paths with weak VF support; ship latin+latin-ext in core, full-i18n as optional download. CJK via
documented system fallback, never bundled.

Swaps (all OFL, pick one per role, never both): Archivo ↔ Oswald (width axis, larger); Montserrat ↔ Inter
(warmer, worse at small sizes); Roboto Condensed only from `ofl/robotocondensed/` (ex-Apache history);
IBM Plex Mono / Playfair Display carry RFNs (`Plex`/`Playfair`) — usable verbatim, rename any fork.

Compliance: 5× `OFL.txt` with first-line copyright intact (concatenation allowed if no copyright line is
dropped); documents/videos rendered with the fonts carry no OFL obligation (§5); never relicense the fonts
under PULSAR's code license (mere aggregation is explicitly allowed); exported MP4s need no font notice.

---

## 9. LUTS — strategy: CREATE OURSELVES, ship zero third-party bytes

- Every commercial "free LUT" license reviewed grants *use of the look* but withholds *redistribution of
  the `.cube`* — bundling = breach. Open-source color projects (Blender, darktable, GIMP, Krita, G'MIC)
  are GPL/CeCILL — copying bytes = copyleft ingestion. OCIO/ACES are pipeline infrastructure (BSD/Apache),
  not creative-look packs.
- **Decision: 8 parametric looks** (Warm, Cool, Film Soft, High Contrast, Muted, B&W, Night, Vintage),
  clean-room math (lift/gamma/gain, S-curves, channel-mixer, split-tone, desat, grain formulas — never
  copied curve points), generic names only. Source of truth = recipe code; `.cube` baked at build
  (16³ ≈ 100–180 KB each → **~1–1.5 MB for 8**; 32³ ≈ 8–10 MB reserved for on-demand HQ export).
  Parametric looks are resolution-independent, slider-friendly (lerp to identity), and testable (ΔE bounds).
- FreshLUTs (CC0-claimed, crowdsourced) / Lutify.me-free (BY-SA) / G'MIC: taste/validation reference only.

---

## 10. VISUAL ASSETS — overlays/textures/demo media

- **Procedural (ship code, ~0 bytes):** film grain, vignette, CRT/scanlines/flicker, gradient backgrounds,
  stylized light leaks + dust specks. Each 5–25 lines GLSL; animatable, resolution-independent.
- **Photographic (defer to v2+, CC0 only):** real paper/dust/leaks from Poly Haven / ambientCG, or
  per-verified Wikimedia-CC0 files (most Commons files are *not* CC0 — per-file tag check mandatory).
  Cap 2K, JPEG unless alpha needed. Never Unsplash/Pexels/textures.com (standalone/bundle bans).
- **Demo footage: bundle nothing.** H.264 1080p ≈ 60–90 MB/min makes even 1–2 bundled clips a ~20 MB
  installer tax for near-zero value. Reference only: Blender CC BY 3.0 excerpts + NASA/NOAA/USGS PD,
  download-on-first-run with auto-generated `ATTRIBUTION.md`; synthetic ffmpeg-generated media for
  tests/screenshots/CI.

---

## 11. TITLES / TRANSITIONS — built-in starter set (all native, ~2–3 MB incl. fonts)

Titles (8, each spec'd with font · animation · safe areas · editable props):

| Preset | Font | Animation | Editable |
|---|---|---|---|
| T1 Basic Title | Inter SemiBold | Fade + slight rise, 12–20 fr | Text, size, tracking, color, position |
| T2 Subtitle | Inter Regular | Cut/fade, broadcast timing | Text, size, bg box opacity, position |
| T3 Lower Third | Inter + Oswald | Slide-in bar + name/title stagger | Two text fields, accent color, duration |
| T4 Centered Minimal | Inter Light, +tracking | Tracking-expand fade | Tracking, size, color |
| T5 Cinematic | Source Serif 4 | Slow tracking + letterbox awareness | Size, tracking, letterbox toggle |
| T6 News/Info | Oswald / JetBrains Mono | Hard cut, side-accent wipe | Kicker + headline, accent, live-bug toggle |
| T7 Credits (+short variant) | Source Serif 4 | Roll (long) / static card (short) | Speed, size; auto-fills attribution in Welcome Project |
| T8 Social Caption | Inter Bold / Bebas Neue | Word-by-word pop, 9:16 center-cut safe | Words-per-card, size, 9:16 preview |

Transitions (7, **all native shader mix kernels** — no asset files, no novelty spam):

| ID | Transition | Native | Params |
|---|---|---|---|
| X1 | Cut | trivial | — (default; gap/override escape per competitive study) |
| X2 | Cross Dissolve | mix() | duration, easing |
| X3 | Dip to Black | mix→black | duration, hold |
| X4 | Dip to White | mix→white | duration, hold |
| X5 | Fade (from/to) | alpha ramp | duration |
| X6 | Simple Wipe | step on uv.x | direction, softness, duration |
| X7 | Push/Slide | offset sampling | direction, easing, duration |

Total installer delta for titles + transitions + font subsets: **~1.5–3.5 MB**.

---

## 12. MUSIC — decision: SHIP NONE (0 tracks) in Beta 1

- Only clean path (Archive.org CC0-marked Musopen recordings) is wrong-genre solo piano + per-recording
  verification burden + oversized masters. CC BY (MacLeod, rare FMA tracks) = per-copy/per-export
  attribution + ContentID support risk. All stock "free music" (Pixabay, Pexels, Uppbeat, Epidemic, FMA
  bulk, Mixkit) prohibits the bundling act itself.
- Size alone kills it: three 3-min tracks in WAV ≈ 70–100 MB; FLAC ≈ 30–50 MB. Opus 64–96 kbps would be
  ~4–6 MB — but there is nothing suitable to encode.
- Cover demo needs with 3–5 short CC0 **stingers/risers** (~100 KB total) + document "import your own
  music." Revisit post-beta via commissioned CC0 loops with signed waiver. If leadership insists on one
  demo track: a single Archive.org CC0 Chopin miniature as 96 kbps Opus (~1.5 MB), labeled
  `demo-classical-PD` with full provenance — never presented as library music.

---

## 13. DEMO PROJECT — "PULSAR Welcome Project" (do NOT build yet)

- 5 clips / 60–90 s: (1) synthetic SMPTE bars + 1 kHz tone (generated at first run — A/V sync, waveforms);
  (2) NASA/USGS PD landscape ~15 s (download-on-first-run); (3) Sintel dialogue excerpt (CC BY 3.0 —
  waveforms, cuts, T2 subtitle); (4) Tears of Steel Amsterdam plate (CC BY 3.0 — T3 lower third + one X2
  dissolve); (5) synthetic gradient outro (T4 + T7 short credits rolling all attributions).
- Exercises: hard cuts throughout, one dissolve, optional wipe/push, T1 opener, T2 subtitle, T3 third,
  T8 caption in 9:16 preview. Sintel excerpt's score (same CC BY, J. Morgenstern) covers music — no music
  bundling needed.
- Architecture: `project.json` references media by **content-hash + source URL**; installer ships only
  JSON + generation script + presets (~50 KB). Graceful offline degradation: opens with synthetic
  placeholders when cache is absent; "Fetch demo media" when online. Auto-generated `ATTRIBUTION.md`
  per project satisfies CC BY credit-scroll + PD courtesy lines.

---

## 14. PACKAGE SIZE — Core / Starter / Full

| Tier | Contents | Estimate |
|---|---|---|
| **CORE** (installer) | ~220 SFX (~43 MB) + 5 font families (~1–3 MB) + 8 LUTs at 16³ (~1–1.5 MB, or 0 if shader-only) + titles/transitions/overlays (~0.2 MB) + Welcome JSON (~0.05 MB) + legal notices (~0.1 MB) | **~44–48 MB** |
| **STARTER** (optional first-run download) | Demo media cache (50–300 MB user-opt-in) + full-i18n font subsets + 32³ HQ LUT export | **50–300 MB** (cached, never installer) |
| **FULL** (later) | Starter + photographic CC0 texture pack (~10–40 MB) + commissioned CC0 music loops | TBD post-beta |

Rule: PULSAR must never become a 2 GB install because of assets. Procedural-first keeps creative payload
near zero; bytes are dominated by SFX + fonts, both justified and bounded.

---

## 15. BUILD VS DOWNLOAD — master verdict table

| Family | Verdict | Rationale |
|---|---|---|
| Gap-fill SFX (~30) | DOWNLOAD / LICENSE (CC0/PD, Opus transcode) | Cannot synthesize authenticity (doors, thunder, shutters) |
| Room tone | CREATE OURSELVES (record fresh) | Legal + loop-quality safety |
| Music | DO NOT SHIP YET (0 tracks) | No clean source at acceptable size/attribution cost |
| Fonts (5) | DOWNLOAD / LICENSE (OFL, verbatim + `OFL.txt`) | Cannot "create" typefaces in scope |
| LUTs (8) | CREATE OURSELVES (parametric) / GENERATE PROCEDURALLY | Zero rights risk, ~1 MB, slider-friendly |
| Overlays (7) | GENERATE PROCEDURALLY (GLSL) | ~0 bytes, animatable |
| Photographic textures | DO NOT SHIP YET (v2+, Poly Haven/ambientCG CC0 only) | Procedural covers v1 needs |
| Titles (8) · Transitions (7) | CREATE OURSELVES (native) | No third-party rights; novelty packs rejected |
| Demo footage | DOWNLOAD ON DEMAND (CC BY 3.0 / PD, never bundled) | Size-prohibitive to bundle |
| Test media | GENERATE PROCEDURALLY (ffmpeg lavfi) | Deterministic, zero rights |
| Sample project | CREATE OURSELVES (generator + JSON, later milestone) | References, not bytes |

---

## 16. LIBRARY METADATA — fields for the future asset browser

Per-item (aligns with competitive-study finding "finding beats quantity" and risk R-29):

```text
id (stable, content-hash-based) · name · category · tags · duration · format ·
channels (audio) / resolution (visual) · favorite · license · license_url ·
creator · source_url · attribution_text (empty unless required) ·
redistribution (y/n) · commercial_ok (y/n) · modification_ok (y/n) ·
date_verified · provenance_note
```

Plus a precomputed sidecar the browser needs at scale (R-29): peaks/waveform summary + loudness so the
browser never decodes 191 files behind one row render; keymapped insert + favorites-star + audition-in-place.

Machine-readable starter manifest: `docs/research/pulsar-beta-starter-assets.csv` (companion to this doc;
provenance subset — full per-file rows get filled at curation time).

---

## 17. LEGAL / ATTRIBUTION — manifest strategy (define now, integrate later)

Future structure:

```text
assets/
  manifests/  sounds.json  fonts.json  visual.json   # full §16 fields per item
THIRD_PARTY_NOTICES.md                                # human-readable credits
```

Rules:

- **CC0/PD**: no legal attribution duty — still record provenance (name/creator/source/license URL/date)
  so PULSAR can answer "where did this come from, can I use it commercially, do I need attribution?"
  with: CC0/PD = commercial yes, attribution no (NASA: courtesy line requested).
- **OFL fonts**: ship each `OFL.txt` with copyright line intact; rendered exports carry no obligation;
  never relicense fonts; respect RFNs (`Source`, and `Plex`/`Playfair` if ever chosen).
- **CC BY 3.0 demo media** (Blender films, download-on-demand only): preserve credit scroll + auto-generate
  per-project `ATTRIBUTION.md` (title, author © Blender Foundation, source URL, license URL, "CC BY 3.0").
- **BY-SA / GPL / NC / ND**: excluded from all bundles by policy (§7) — any future exception needs written
  review + ShareAlike/compliance plan, never silent inclusion.
- Permanently preserved per item: name, creator, source URL, license URL, license, redistribution /
  modification / commercial flags, attribution wording, file type, size, category, date verified, notes.
  If any field cannot be established → not ship-ready.

---

## 18. BETA STARTER PACK — final exact recommendation

- **~220 SFX** (191 existing + ~30 CC0/PD gap-fill, Opus, −16/−14 LUFS, loopable beds) with `index.json`.
- **5 OFL fonts**: Inter (sans) · Oswald (condensed/display) · Source Serif 4 (serif, RFN `Source`) ·
  JetBrains Mono (mono) · Bebas Neue (distinctive title). Variable TTF, latin+latin-ext in core.
- **8 self-generated looks** (Warm, Cool, Film Soft, High Contrast, Muted, B&W, Night, Vintage) at 16³.
- **7 procedural overlays** (grain, vignette, scanlines/CRT, gradients, leaks, specks, matte noise).
- **8 native title presets + 7 native transitions** (Cut default; one dissolve in Welcome Project).
- **0 music tracks**; 3–5 CC0 stingers instead; "import your own music" documented.
- **0 bundled footage**; download-on-first-run CC BY/PD cache + synthetic test media.
- **1 Welcome Project generator** (later milestone): JSON + script + auto-attribution, offline-degrading.
- **Installer delta for all creative content: ~44–48 MB**, dominated by SFX+fonts; everything visual ≈ 2–3 MB.

---

## 19. FILES

- `docs/research/pulsar-beta-starter-content.md` (this report)
- `docs/research/pulsar-beta-starter-assets.csv` (machine-readable source manifest, provenance subset)

## 20. COMMIT

```text
research: design Pulsar beta starter content
```

Push to `origin/chore/beta-starter-assets-research`. Then STOP — no integration.
