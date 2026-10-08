# Product audit — "professional + creator completeness" pass

Done on the build of `94e3917` (creator-ready beta) before any code was touched, then repeated on the
result. Method: PULSAR was used as a product, not as a test target — a 12-clip shoot imported as a
folder, music, sound effects, three titles, several audio tracks, ripple, drag and drop, save/reopen,
recovery after `kill -9`, 720p export — at 1280×720, 1366×768 and 1920×1080, looking at it as (1) an
editor, (2) a product designer, (3) a first-time user, (4) a creator who wants to ship a video.

## What still made it feel like a technical beta

| # | Finding | Level | Status |
|---|---|---|---|
| 1 | A `.pulsar` passed on the command line / "Open with" was probed as *media* and failed with "format not compatible or damaged". The saved project itself was fine. | **P0** | Fixed: project files open as projects; dropping one on the window asks about unsaved changes first. |
| 2 | Editing shortcuts (`S`, `Delete`, `Space`, `Home`…) stayed live behind a file dialog: typing in the dialog split clips. | **P0** | Fixed: every shortcut is disabled while any dialog or the colour palette is open; single keys also stay off while typing. |
| 2b | Found while driving the Creator Gate 2.0 project: music imported as part of a folder could not be placed at all — audio rows had no *Insertar*, and a drop was refused because no audio lane existed yet. Only importing a single audio file (which auto-places it) worked. | **P0** | Fixed: *Insertar en el cabezal* on audio rows (double-click too) and dropping under the last lane both place audio, creating an audio track when none is free, as one undo step. |
| 3 | No way to bring in a photo or make a title-card background: a creator had to leave for another tool. | **P0 (completeness)** | Image clips and colour cards (below). |
| 4 | No fade for music/effects: every music bed ended abruptly. | **P0 (completeness)** | Clip audio fades (below). |
| 5 | Sound effects could not be auditioned; names were English ("Click Soft") in a Spanish interface; 25 sounds in one flat list. | P1 | Spanish names, ten categories, ▶ audition, +12 sounds. |
| 6 | Clips on a long timeline were labelled `clip_0…`, indistinguishable; thumbnails vanished below ~200 px. | P1 | Names keep their distinguishing end; narrow clips show their picture as a dimmed backdrop. |
| 7 | Title lane blocks, colour of photos vs. footage, fades and still clips had no visual language. | P1 | Photos amber, colour cards in their own colour, fade ramps drawn on the clip. |
| 8 | Library: no way to tell footage from audio at a glance, no filter, no search; the empty state did not say what to do. | P1 | Kind icons, All/Video/Image/Audio chips, search for long bins, clearer empty states. |
| 9 | Inspector kept describing the *last library item* while another clip was selected; a selected title still showed an unrelated "MEDIO" block. | P1 | Follows the selected clip; hidden for titles. |
| 10 | Export: default name `pulsar.mp4` (could overwrite a previous export), a button that just said "Exportar", no word about where the file went. | P1 | "Mi video.mp4" / "(2)" never overwrites by suggestion; "Exportar video"; "Video exportado: name.mp4". |
| 11 | No frame-step; no duplicate; header logo hard-wired in three places. | P1 | Frame step, Duplicar, one `BrandMark` component, About box. |
| 12 | Qt's own file dialog (light theme, English) is off-brand and cannot multi-select with Qt 6.4's non-native fallback. | P1 | **Not fixed** (toolkit). Workarounds exist (folder import, drag and drop). |
| 13 | `Ctrl+Z` inside the title text box undoes the text, not the timeline. | P2 | Left as is (it is the text field's own undo). |
| 14 | Music dropped by path lands at the playhead, not at zero. | P2 | Left as is; drag it to the start or use *Insertar* at 0. |

## Creator completeness: what still forces a creator to leave?

Scores are 1–10. Effort and risk are for *this* codebase; "schema" means a project-file change; "parity" means
a preview/export divergence risk.

| Item | User | Demo | Effort | Risk | Touches | Schema | Parity risk | Beta? |
|---|---|---|---|---|---|---|---|---|
| A. Still images | 10 | 9 | 5 | 4 | probe, decode session, canvas rule | none (derived from the container) | none: same decode path | **Done** |
| B. Colour cards | 8 | 8 | 4 | 3 | decode session (generated frame), locator | none (`pulsar-color:rrggbb`) | none | **Done** |
| C. PNG/logo overlay | 6 | 7 | 7 | 8 | needs a second visual layer in the compositor, an alpha-preserving asset kind (stills are flattened on purpose), position/scale schema | yes | yes | **No → v0.2** (kill rule: new compositor + schema) |
| D/E. Audio fade in/out | 9 | 8 | 5 | 4 | `Clip`, mixer, offline renderer | `CLIP_FADE` suffix, header v4 | shared `ClipFade`, tested identical | **Done** (audio) |
| E. Video fade | 5 | 6 | 6 | 5 | needs an opacity stage in preview + export | yes | yes | No (fade to a colour card is possible now) |
| F. Duplicate | 7 | 6 | 2 | 2 | session only (existing commands) | none | none | **Done** |
| G. Frame step | 6 | 5 | 2 | 2 | controller; exact rational frame grid | none | none | **Done** |
| H. More audio tools (EQ, ducking, normalise) | 6 | 4 | 8 | 7 | mixer graph | yes | yes | No → v0.2 |
| I. SFX library | 7 | 8 | 3 | 2 | generator + manifest | none | none | **Done** (37 original sounds) |
| J. Title styling | 8 | 8 | 3 | 3 | `Title`, rasteriser | `TITLE_LOOK`, header v4 | shared rasteriser | **Done** (box, opacity) |
| K. Export workflow | 8 | 7 | 2 | 1 | dialog text, default name | none | none | **Done** |
| L. Media organisation | 7 | 7 | 3 | 2 | library model | none | none | **Done** (filter, search) |
| Track-level mute/gain/solo | 6 | 5 | 8 | 7 | mixer + schema | yes | yes | No → v0.1.1 |

### Why overlay and video fade were rejected

*Overlay:* the title compositor blends one premultiplied raster per frame, which is exactly the shape of
a logo — but a logo needs the alpha channel, and stills are deliberately flattened over black so a
photo never shows the window behind it. An overlay means a second asset role (alpha kept), a track or lane
for it, position/scale/opacity in the schema, and thumbnails that understand transparency. That is the
"compositor + schema" the kill rule names, so it is a 0.2 feature, not a late addition to this pass.

*Video fade:* every video frame would need an opacity stage in both the preview path (Qt image provider)
and the offline renderer, and the preview is not a compositor today. A fade to or from a colour card
(now possible) covers the common intro/outro case without a new stage.

## Not changed on purpose

Video 2, transitions, colour correction, LUTs, filters, speed ramps, subtitles, chroma key, proxies, GPU
paths, AI — none of them were touched.
