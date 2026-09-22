# FD Labs — Software Gap Hunt 2026

**Date:** 2026-09-21
**Author:** research/scouting agent
**Purpose:** map real, evidence-backed software pain that FD Labs could realistically design and build.
**Scope:** desktop, Linux-first/local-first, maker, developer, creator, homelab, small-business, hardware+software.
**Status:** research only — no candidate has been built, no production code touched.

---

## RESULT

Roughly 100 first-hand pain signals were collected across 7 parallel research tracks (~250 distinct
searches/fetches: Reddit threads, Hacker News (Algolia API), GitHub issues, project trackers, forums).
These compress into **20 top opportunities**, **12+ "boring but good" tools**, a mined catalogue of
**private scripts people depend on**, and **25+ tasks that are still terrible in 2026**.

Headline findings:

1. **Backup verification is the loudest unsolved pain with near-zero incumbents.** "Backups aren't real
   until they're restored" appears independently in HN (100+ comments), Reddit, and personal blogs —
   and the only tools that exist have 0–1 stars.
2. **The spreadsheet is the competitor in almost every small-tool market** — admitted publicly by a
   funded founder ("the spreadsheet is my biggest competitor", PartsBox, HN 46268452).
3. **Settings/history amnesia is a universal maker pain**: nobody can answer "which profile produced
   this good print?" or "what calibration values were valid last month?"
4. **Linux desktop pain is chronic, not exotic**: unreclaimable 295 GB Flatpak stores, NVIDIA updates
   that black-screen with no GUI rollback, suspend draining to 0% by morning, mixed-DPI click offsets.
5. **Subscription + forced-account backlash is loud and monetizable**: one-time-purchase, offline tools
   are repeatedly requested in the same threads that complain about $120/yr transcription apps.
6. Several hypotheses were **falsified** (see SURPRISES): raw transcoding, PDF-merge utilities, and API
   clients are far more crowded than they look.

Nothing here should be built yet. Five candidates are recommended for deeper validation first.

---

## SOURCES SEARCHED

**Methods**

- `websearch` (Reddit/forum/blog discovery; returned HTTP 401 after heavy use late in the run)
- Hacker News **Algolia API** (`hn.algolia.com/api/v1/search`, filtered `created_at_i>1704067200` for
  2024+; stories and comments; full thread reads)
- **GitHub Search API** (`api.github.com/search/issues`) and direct issue/README fetches
- Fedora Discuss (`discussion.fedoraproject.org` search JSON), Linux Mint forum, KiCad forum/GitLab,
  Prusa/Bambu/Orca/Arduino/esptool/devcontainer issue trackers, vendor pricing pages (competition facts only)

**Communities mined (representative, not exhaustive)**

- Reddit: r/selfhosted, r/homelab, r/linux, r/linux4noobs, r/linuxquestions, r/linuxmint, r/archlinux,
  r/openSUSE, r/3Dprinting, r/BambuLab, r/3DPrintFarms, r/klippers, r/arduino, r/esp32, r/embedded,
  r/AskElectronics, r/diypedals, r/Machinists, r/CNC, r/hobbycnc, r/Metrology, r/ExperiencedDevs,
  r/backend, r/devops, r/softwaretesting, r/ClaudeAI, r/ClaudeCode, r/AI_Agents, r/podcasting,
  r/VideoEditing, r/DataHoarder, r/aitubers, r/smallbusiness, r/Contractor, r/AskContractors,
  r/freelancing, r/CraftFairs, r/airbnb_hosts, r/homeowners, r/indiehackers, r/commandline, r/SideProject
- HN threads: 27482397 (Tools you have made for yourself — 453 comments), 21373931 (What do you
  automate — 192 comments), 9471000 (favorite custom shell script), 45670052 (Scripts I wrote that I
  use all the time), 43662421 (script that replaces a SaaS), 41999462 (scripts to stand up a new
  server), 49732513 (Backups Aren't Simple — 100+ comments), 45923352 (Why do voice transcription apps
  charge monthly when Whisper runs locally?), 43887073 (Why Flatpak apps use so much disk), 43393822
  (GIMP 3.0 — 1312 points), 43181862 (Telescope logs), 46942160 (screenshot tools on Linux), and ~40 more
- GitHub projects inspected: Spoolman, Part-DB, InvenTree, PartsBox, homelable, homedex, Backrest,
  restorable, Obico, SimplyPrint, SpoolEase, Maker Management Platform, Bruno, Hoppscotch, Yaak,
  Aegisub, Subtitle Edit, XnConvert, Shutter Encoder, esptool, Arduino IDE, YAPP, pcb-enclosure-generator

**Caveat:** Reddit's JSON endpoints returned 403; Reddit evidence is therefore snippet-level from
search results (real threads, real URLs, quotes as surfaced). HN direct page reads hit HTTP 429, so HN
quotes come from the official Algolia API (same content, valid `item?id=` URLs). Threads or hypotheses
with thin evidence are explicitly flagged **WEAK**.

**Evidence grades used below:** STRONG = 3+ independent reporters or 2+ sites with quotes;
MEDIUM = 2 independent sources; WEAK = one source or inferred (flagged).

---

## RECURRING PAINS

Cross-domain patterns — each one showed up independently in at least three tracks:

1. **Backups nobody has ever restored.** "Backups without restore tests are worthless" (Reddit);
   *"Backups isn't real until it's restored"* and *"How can I verify that I really restored my 10k
   photos?"* (HN 49732513). People cope with dice-roll test scripts.
2. **Spreadsheet as database.** IT inventory (`IT Inventory Final v19 USE THIS ONE.xlsx`), filament,
   parts drawers, job tracking, doctor rotas, calibrations, warranty receipts, show P&L — plus
   *"32GB of RAM to edit 500,000 rows"* (HN 46748771). Six independent HN commenters admitted
   running real databases in spreadsheets.
3. **Settings/history amnesia.** "Which profile produced this good print?", calibration values on
   sticky notes taped to printers, multimeter readings written by hand, "I've forgotten what backup
   system was in place on a prior install."
4. **Diagnosis in plain language doesn't exist.** Linux *"nothing in syslog, nothing in dmesg"*
   (HN 46477081); esptool `PermissionError(13)` cycles; devcontainer feature failures; "why is my
   print failing"; users asking Claude Code to fix their NVIDIA drivers (HN 49705739).
5. **Subscription + account fatigue.** Transcription apps at $120–144/yr while Whisper runs locally
   (HN 45923352); 1Password +33% (150-reply forum thread); Figma +33%; Adobe FTC action; Evernote
   "900% price creep"; 640+ offline-first posts in six months (analysis of 9,300 "I wish there was an
   app" posts, r/SaaS 1q5lfur).
6. **Personal scripts reinvented by everyone.** Backup stacks, log viewers, release-note writers,
   quoting sheets, GPS geotaggers, accounting automation — the same utilities rebuilt across HN's
   four big "scripts I made" threads.
7. **Workflows scattered across 4–5 tools.** spreadsheet + browser + terminal + notes + scripts is the
   documented default for makers, homelabbers, and small shops alike.
8. **Docs and config drift.** README/ADR rot, homelab wikis going stale until the failure moment, and
   (new in 2025–26) five drifting `CLAUDE.md`/`AGENTS.md` files per repo that "drift apart instantly."
9. **Platform kills are sudden and total.** Pocket (data deleted Oct 2025), uBlock Origin (MV3),
   Authy desktop, Google Earth desktop, Nitter C&D — each triggering "local files or nothing" demand.
10. **Offline is a functional requirement, not a preference.** Fairgrounds with no signal, field crews,
    air-gapped shops, privacy-sensitive dictation, bench work away from the network.

---

## TOP 20 SOFTWARE GAPS

Ordered roughly by (evidence × FD Labs fit × feasibility). Every card uses the same fields.
Difficulty: S = MVP in days/weeks · M = serious product in 1–3 months · L = large but realistic ·
XL = infra/team-heavy (kept out of the top 20 by design).

---

### 1. Verified Restore — backup drill tool

- **Problem:** People run Borg/Restic/Kopia/Timeshift but never prove a restore works. No tool runs
  scheduled, sandboxed, *app-level* restore drills across heterogeneous backups (Docker volumes, DB
  dumps, photo libraries, bare-metal images) and reports "last verified".
- **User:** Self-hosters whose lab became accidentally production; Linux desktops; creators with years
  of masters.
- **Evidence (STRONG, three tracks):** HN 49732513 "Backups Aren't Simple" — *"How can I verify that I
  really restored my 10k photos without losing anything?"*, *"Backups isn't real until it's restored,
  don't be like me folks"*, *"I added 'roll a dice for backup and try to restore it' test script"*;
  https://www.reddit.com/r/selfhosted/comments/1ojd44y/ *"Backups without restore tests are worthless"*;
  https://www.reddit.com/r/linuxsucks/comments/1pgxhxq/ *"why is there still no Linux personal backup?"*;
  Fedora t/84815 *"Couldn't restore my timeshift backup… I give up and I'm reinstalling fedora"*;
  https://news.ycombinator.com/item?id=40878812 (user built and ran his own Time-Machine-style tool 5 years).
- **Current workaround:** dice-roll scripts, healthchecks.io dead-man switches, "restore one file a
  month", hope.
- **Existing alternatives:** Backrest (OSS, schedules jobs — no drills), `restic check`/`kopia validate`
  (repo integrity ≠ usable restore), `restorable-dev/restorable` (MIT, **0 stars**), DockBack (**1 star**),
  Databasus (Postgres-only, paid), CloudCASA (Kubernetes), Timeshift/Déjà Dup/Kup (system/personal split).
- **Why alternatives fail:** integrity checks are not recovery proof; nothing orchestrates "restore into
  a disposable container/VM → run smoke test → report" across tools; nothing owns the per-lab view.
- **FD Labs angle:** local-first, read-only, trust-first product; a desktop "backup report card" that
  speaks plain English — fits the fast/simple/own-your-data thesis exactly. Reusable verification core
  across desktop + homelab + creator editions.
- **MVP:** agent that watches existing Restic/Borg/Kopia/Proxmox repos → weekly disposable restore of
  the newest snapshot → user-defined smoke checks (HTTP 200, DB rowcount, file-manifest diff, play a
  random video) → one "last verified: 6 days ago" badge + alert.
- **Difficulty:** M (L for app-aware checks)
- **Monetization:** free core + paid drill templates / multi-host; one-time desktop license plausible.
- **Risks:** a bad drill must never touch primary data (sandbox correctness is the whole brand);
  restic/kopia ecosystem may ship this; storage churn on large libraries.

---

### 2. Linux "why is my machine broken?" — plain-language diagnosis

- **Problem:** Logs are scattered across journal/dmesg/Xorg/vendor logs with no correlation; users get
  *"Nothing in syslog, nothing in dmesg, nothing in X.org.log"* and either reinstall or paste logs into
  forums/LLMs.
- **User:** Non-sysadmin desktop users, new Linux converts, friends-and-family supporters.
- **Evidence (STRONG):** HN 46477081 (T14s lockups, quoted above → switched distros);
  HN 49705739 *"It is very hard to debug and fix stuff on Linux… my nvidia drivers broke somehow. I
  asked Claude code to take a look"*; https://www.reddit.com/r/linuxquestions/comments/l4qvou/
  *"Is there an app that can run an automated diagnostics on my computer?"*; Fedora t/131279
  (*"'Dummy Output'… suggestions above my pay grade"*); Fedora t/180937 (new user, RTX 5060Ti → blue screen).
- **Current workaround:** copy logs into forums/Reddit, ask ChatGPT/Claude, random Stack Overflow
  incantations, reinstall.
- **Existing alternatives:** Mission Center/Stacer/hardinfo2 (show counters, not root cause),
  `inxi`/`journalctl` CLIs, vendor Support Assistant (own hardware only), sOS-style support sessions (paid).
- **Why alternatives fail:** nobody bundles hardware + kernel + driver + service *timeline* into one
  ranked verdict with evidence lines and safe suggested fixes; nothing works fully offline.
- **FD Labs angle:** offline, read-only "system health report" is a privacy story *and* a Linux-first
  story; explain-don't-touch default avoids bricking machines.
- **MVP:** collect journal + hw + driver deltas since last boot/update → ranked probable causes, each
  with the evidence lines that produced it → safe fix instructions (never auto-execute by default).
- **Difficulty:** M–L (the knowledge base is the product)
- **Monetization:** free report + paid one-click "fix packs", or $29 one-time.
- **Risks:** false positives destroy trust; must integrate GNOME+KDE+distro differences; upstream
  projects could absorb it.

---

### 3. Print Outcome Log + Calibration Ledger (maker)

- **Problem:** Good prints aren't reproducible: slicer profile + firmware + nozzle + spool + calibration
  changed together and nothing records which combination produced the result. Calibration values live
  on sticky notes taped to each printer and die on firmware flash.
- **User:** FDM hobbyists (1–5 machines), Printables/MakerWorld publishers, small print sellers.
- **Evidence (STRONG):** PrusaSlicer issue 11914 (**246 comments**) *"I can't tell if it's an issue with
  PrusaSlicer, my Prusa Mini+ firmware or the printer settings as all 3 of these were updated at once"*;
  https://www.reddit.com/r/3Dprinting/comments/1ttd6dy/ *"Hand written notes… a note taped to the
  [printer]"*; https://www.reddit.com/r/BambuLab/comments/1peqyi0/ *"Why is there no option to set the
  K factor in filament presets? Why are calibration options so limited?"*; https://www.reddit.com/r/3Dprinting/comments/1d2d70w/
  (user-built "master spreadsheet"); BambuStudio #9153 (shrinkage tracked in a spreadsheet).
- **Current workaround:** Google Sheets, screenshots of LCDs, sticky notes, old 3MF folders.
- **Existing alternatives:** slicer profile trees (settings without outcomes), OctoPrint history plugin
  (OctoPrint-only), Teaching Tech calibration wiki (how-to, not a record), FlowFuse DIY dashboard (2026,
  generic), self-built tools (3mf-settings-analyzer, OrcaDiff, Kobra-shaper visualizer).
- **Why alternatives fail:** they diff *files*; nothing records *outcomes* — no photo + gcode header +
  firmware + calibration snapshot per print, searchable per machine/filament.
- **FD Labs angle:** local-first desktop app that imports 3MF/gcode headers automatically; zero server;
  the data is a maker's private knowledge asset.
- **MVP:** "print card": auto-import slicer/profile/filament/temps from the exported file → attach
  photo + pass/fail + calibration snapshot → search "what worked for PETG-CF on MK4".
- **Difficulty:** S–M
- **Monetization:** free local + $25–40/yr optional sync/backup (or one-time).
- **Risks:** slicer header formats vary; Bambu/Prusa could ship it natively; must import cleanly from
  the big three slicers or it feels broken.

---

### 4. Local-first parts-drawer inventory + BOM (electronics makers)

- **Problem:** Thousands of components across drawers/bags/reels; BOMs in Excel; people buy duplicates
  because nobody knows what's in which drawer. A 15-year-old pain with a public confession from the
  category's funded founder.
- **User:** Arduino/ESP32 hobbyists, PCB designers, small repair/electronics workshops.
- **Evidence (STRONG):** HN 46268452 — PartsBox founder: *"people use spreadsheets to manage
  electronic parts (please don't)… The spreadsheet is my biggest competitor"*; r/embedded 1s1agqo (2026)
  — *"Why is BOM management still stuck in Excel in 2026?"*; r/AskElectronics n1ujpd — *"Right now I'm
  using Excel sheets and lot of digging into pile of stuff"*; r/AskElectronics u4q1hn — *"looking to
  build my own inventory system as no suitable solution seems to exist"*, *"I tried a spreadsheet for
  awhile but it is a pain to manage"*, *"I may just code my own inventory management"* (NFC stickers);
  electronics.SE 3893 (thread running since 2010 with hand-rolled location codes).
- **Current workaround:** spreadsheet + paper labels + memory + Digikey order history.
- **Existing alternatives:** PartsBox (paid SaaS, ~$240/yr class), Part-DB (OSS, self-hosted web),
  InvenTree (OSS, BOM/enterprise-shaped), Binner, Copiol (new, cloud), Snipe-IT (IT-asset-shaped).
- **Why alternatives fail:** web/self-host/server-centric; hobbyists want drawer-location-first entry
  (photo of box → tap), QR/NFC labels, CSV import from a Digikey cart, and offline use at the bench.
- **FD Labs angle:** desktop-first, local SQLite, printable labels, transparent export; "the drawer you
  can search."
- **MVP:** drawer→compartment addresses, parametric search (value/package/part-number), one-shot
  distributor-cart CSV import, printable QR labels, shortage list per project BOM.
- **Difficulty:** M (data import polish is the whole product)
- **Monetization:** $20–30/yr or $60 lifetime.
- **Risks:** PartsBox loyalty in the pro segment; active OSS (Part-DB/InvenTree); low urgency → long
  purchase cycle.

---

### 5. Subtitle timing repair + translation QA (local-first)

- **Problem:** Auto-captions produce broken timestamps and garbage lines outside English; fixing them
  line-by-line is "Neanderthal" work; translated-subtitle fetching breaks at scale.
- **User:** Non-English/bilingual YouTubers, hobby subtitlers, course producers.
- **Evidence (STRONG):** Aegisub issue #78 *"Now I have to modify each line and it's really Neanderthal"*;
  r/aitubers 1w7s7ve — *"auto captions are completely useless once you step outside english… fixing
  broken timestamps"*; yt-dlp #13831 (auto-translated subs → HTTP 429); r/antiai 1v12p0j (voice-to-subtitle
  quality complaints).
- **Current workaround:** Aegisub/Subtitle Edit line-by-line surgery; regenerate with Whisper and
  hand-align; YouTube Studio (online only).
- **Existing alternatives:** Aegisub (free OSS, dated UX), Subtitle Edit (free OSS, Windows-first),
  SubtitleWise (free web), Kapwing/VEED ($10–30/mo, upload your content), YouTube Studio.
- **Why alternatives fail:** none combine local Whisper re-timing + bulk translation QA + a diff/preview
  of what changed; SaaS meters usage and requires upload.
- **FD Labs angle:** direct adjacency to PULSAR (subtitle pipeline becomes a reusable FD Labs
  component); 100% offline = the privacy pitch for pre-release material.
- **MVP:** import video/SRT → re-time from Whisper word timestamps → flag suspect lines (overlap,
  >20 CPS, translation mismatch) → one-key fix + batch shift/merge → export SRT/VTT.
- **Difficulty:** M
- **Monetization:** one-time $20–40 or freemium.
- **Risks:** Whisper quality varies by language; Aegisub/Subtitle Edit are entrenched free incumbents;
  model downloads must ship or be bundled for offline use.

---

### 6. Loudness normalization wizard (EBU R128 without the physics degree)

- **Problem:** Creators must hit −16 LUFS (podcast) / −14 (YouTube) / −23 (broadcast) but tools expose
  raw LUFS/LRA/true-peak knobs or nothing.
- **User:** Podcasters, streamers, audiobook makers.
- **Evidence (STRONG):** r/podcasting 1snjpxo, 1na6h4t, 11tacaa (repeated loudness confusion threads);
  github topic `ebu-r128` is a graveyard of raw FFmpeg loudnorm GUI wrappers; HN 45923352 thread culture
  (local tools wanted over subscriptions).
- **Current workaround:** copy-pasted two-pass `ffmpeg loudnorm` recipes, Auphonic upload, guessing in
  Audacity.
- **Existing alternatives:** Auphonic (free tier then ~$11+/mo, web, not Linux), YouLean meter (free,
  Win/mac — measures only), ffmpeg (CLI), Levelator (dead), hobby GUIs.
- **Why alternatives fail:** everyone either *measures* or exposes raw knobs; nobody owns "drop 40 files
  → done at each platform's target, with before/after A/B and a compliance report."
- **FD Labs angle:** smallest, fastest MVP in this report; FFmpeg already in PULSAR's stack.
- **MVP:** drop folder → pick target (YouTube/Spotify/Apple/broadcast) → two-pass R128 + true-peak
  limiter → sidecar report per file + one-click A/B.
- **Difficulty:** S
- **Monetization:** $15–25 one-time (or free funnel into the creator suite).
- **Risks:** thin moat (ffmpeg does it); Auphonic's free tier absorbs casual users — must win on
  batch/offline/no-account.

---

### 7. Wayland recording companion (screen + cam + mic, recoverable takes)

- **Problem:** On Wayland, OBS is fragile (portal/PipeWire permissions, black screens, desync) and
  lightweight recorders drop audio tracks or can't do cam overlays; lost takes aren't recoverable.
- **User:** Linux creators, educators, demo makers, streamers.
- **Evidence (STRONG):** HN 46942160 *"every screenshot tool I've tried falls apart on Linux. The whole
  xdg-desktop-portal + PipeWire dance… is a different world from X11"*; Fedora t/179819 (*"since
  updating to Fedora43 I cannot share my screen any more, it's just black"*); r/linux_gaming 1echnyi /
  1ck2e4v / 17ci7ll (recurring OBS/Wayland pain); r/hyprland 1jagtvu (wf-recorder/Kooha limits).
- **Nested OBS on XWayland; wf-recorder CLI; manual track sync.**
- **Existing alternatives:** OBS (free OSS, heavy/config-hostile under Wayland), Kooha, wf-recorder,
  SimpleScreenRecorder (X11), Kazam (stale).
- **Why alternatives fail:** gap is exactly "OBS power, recorder simplicity, Wayland-native" — separate
  tracks, pause/resume, crash-safe append do not coexist anywhere.
- **FD Labs angle:** Linux-first identity piece; pairs with PULSAR (record → edit).
- **MVP:** one hotkey → screen + cam PiP + 2 audio tracks to separate files, pause/resume, crash-safe
  append, instant-replay folder.
- **Difficulty:** M
- **Monetization:** free core + $15 pro (overlays, replay, device routing).
- **Risks:** portal/wlroots protocol churn per compositor; OBS is a very strong free anchor.

---

### 8. Creator asset library with local search (b-roll / SFX / music)

- **Problem:** Footage, sound effects, and music live in chaotic folders with meaningless filenames;
  search means scrolling. No Linux-native, video/audio-first, local-first media DAM exists.
- **User:** YouTubers, editors, streamers, podcasters.
- **Evidence (MEDIUM–STRONG):** r/VideoEditing 1bb14um (folder chaos), r/NewTubers ri5yq0,
  r/SmallYTChannel 1mgm9pq; HN 46853044 — Zush *"renames them from IMG_8374.jpg to a description of
  what's in the photo… so search actually works"* (demand proof; macOS/images only).
- **Folder conventions + spreadsheets; Eagle/PureRef on other OSes; renaming scripts.**
- **Existing alternatives:** Eagle ($35–50, Win/mac — no Linux), PureRef (refs only), Adobe Bridge
  (Win/mac), digiKam (photos only), Zush ($10, macOS, images), Hazaruma-style self-builds.
- **Why alternatives fail:** nothing is Linux-native + local + audio/video-first with content indexing
  (speech transcripts, waveform/scene fingerprints).
- **FD Labs angle:** highest synergy with PULSAR — the index, proxy, waveform, and transcript layers are
  reusable editor components; natural future import target for the editor.
- **MVP:** point at folders → thumbnails/waveforms + local Whisper transcripts + filename/EXIF parse →
  instant search ("drone shot golden hour", "whoosh SFX") → smart collections + duplicate detection.
- **Difficulty:** L
- **Monetization:** $30–50 one-time.
- **Risks:** indexing huge libraries is heavy (must stay modest-hardware friendly); Eagle's community
  templates create switching costs; AI tagging must run fully offline to fit FD Labs' ethos.

---

### 9. Package disk bloat forensics (Flatpak / Snap / ostree / Proton caches)

- **Problem:** Disk fills with runtimes, duplicate runtimes, old Snap revisions, opaque ostree objects;
  the CLIs demonstrably fail to reclaim; no GUI explains what is safe to remove.
- **User:** Any Linux user with >3 flatpaks/snaps; small-SSD laptops.
- **Evidence (STRONG):** flatpak/flatpak #5904 — *"du returns 296G… I have attempted flatpak remove
  --unused and flatpak repair with no change"* (links #3894, #4270, #2147, #3542); HN 43887073 (78 pts,
  144 comments) *"Why Flatpak apps use so much disk space"*; HN 45208220 (*"two apps… easily end up
  with 8 flatpaks including runtimes"*); r/Ubuntu 1hp2510 *"how can I stop Snap from turning my
  computer into a digital landfill?"*; r/linux4noobs 16qu97t (1 GB updates for Telegram).
- **`flatpak uninstall --unused`, `flatpak repair`, disabling old snap revisions by hand, ncdu.**
- **Existing alternatives:** Baobab/ncdu/BleachBit/Stacer (show directories, not package-level
  attribution), flatpak/snap CLIs.
- **Why alternatives fail:** ostree object store is opaque; `repair` demonstrably doesn't reclaim;
  Snap old-revision cleanup has no GUI; nobody shows per-app *true* size including shared runtimes with
  a safe reclaim button.
- **FD Labs angle:** classic "boring but good" diagnostic; pure local metadata work; instant word-of-mouth
  in Linux communities.
- **MVP:** unified package disk map (apt/rpm + flatpak + snap + Steam/Proton caches) → per-app true
  size → one-click conservative reclaim with warnings before breaking dedup.
- **Difficulty:** M
- **Monetization:** FOSS core + paid pro (scheduled reclaim / storage-sense) one-time $12–29.
- **Risks:** destructive reclamation could brick apps — must be conservative; distro politics.

---

### 10. Update bodyguard — driver/kernel rollback without a terminal

- **Problem:** NVIDIA/kernel/driver updates leave black screens; recovery needs `dnf downgrade`, chroot,
  snapper from a live USB, or Secure Boot re-signing — none of it GUI, none of it explained.
- **User:** NVIDIA desktop/gaming users, rolling-release users, new Linux converts.
- **Evidence (STRONG):** r/openSUSE 1m3rm48 (*"Nvidia, fuck you. You broke my system AGAIN!"* — top
  answer: snapper rollback + driver lock); r/archlinux 1mqixhx / 1jbr97x (rollback attempts failing /
  how-to requests); r/Nobara 1outo06 (*"Rolled back driver update. Now I can't boot"*); Fedora t/172775
  (kernel update broke 21:9 monitors); HN 46096827 (*"a bogus update was pushed to Ubuntu 24 which
  completely broke Nvidia"*); HN 41701201 (*"I have to manage package updates manually instead of
  taking the System Updates through the UI"*).
- **Package pinning by hand, Timeshift/snapper snapshots (that usually don't exist when breakage
  happens), 3-kernel keep, live-USB rescue.**
- **Existing alternatives:** Timeshift/snapper/btrfs (FOSS, snapshots but no guided driver rollback),
  Mint's kernel pinning, terminal downgrade commands.
- **Why alternatives fail:** snapshots are opt-in and rarely present at the moment of breakage;
  *userspace driver library* rollback (not just kernels) has no GUI; Secure Boot/MOK/initramfs steps are
  unexplained.
- **FD Labs angle:** "explain + safe rollback" product with a rescue narrative — high word-of-mouth if
  it saves one machine; can ship as the fix-pack companion to card #2.
- **MVP:** pre-update snapshot hook, kernel↔driver-pair consistency check, one-button rollback entry in
  the boot menu, plain-language "what changed and why you're black-screened."
- **Difficulty:** M
- **Monetization:** FOSS core + $19 one-time "rescue" tier / paid support.
- **Risks:** touching bootloaders/Secure Boot carries brick risk; distro upstreams may resist; needs
  ruthless safety engineering.

---

### 11. Laptop power forensics — "what drained my battery / why did suspend fail"

- **Problem:** Suspend drains to 0% overnight; dGPU never sleeps; no GUI answers *what* consumed power
  or *why*.
- **User:** Linux laptop users, especially NVIDIA/optimus and ThinkPad cohorts.
- **Evidence (STRONG):** HN 46475023 *"I don't think I've ever seen sleep suspend working reliably on a
  Linux laptop"*; HN 47259947 *"Big GPUs are the #1 reason battery doesn't last on Linux laptops"*;
  HN 44701101 (*"Poor power management has always been the last bastion keeping me from using Linux"*),
  HN 44316518 (9W idle); Fedora t/131096 (two ThinkPads *"battery is drained to 0% the next morning"*).
- **TLP/auto-cpufreq configs, `powertop --auto-tune`, envy-control/bbswitch, disabling suspend entirely.**
- **Existing alternatives:** TLP, power-profiles-daemon, auto-cpufreq, thermald, powertop — all
  config/CLI-first, and TLP vs p-p-d conflict each other.
- **Why alternatives fail:** none answer "what drained my battery while suspended" or "which process is
  eating 10W"; no profile conflict detection; no wake-source attribution.
- **FD Labs angle:** pairs with card #2 (diagnosis) as the battery chapter; plain-language + one-click
  profiles with conflict detection is genuinely unowned.
- **MVP:** W/h drain graph, per-device/per-process attribution, suspend-drain test with wake-source
  attribution, quiet/battery/perf profiles with conflict detection.
- **Difficulty:** M
- **Monetization:** free core + laptop-specific profile packs / $15 one-time.
- **Risks:** kernel/firmware bugs are outside your control; huge hardware matrix → must degrade gracefully.

---

### 12. Display profile doctor — mixed DPI, mixed refresh, "apply in 30s"

- **Problem:** Fractional scaling causes mis-clicks on XWayland apps; second monitors cap at 50/60 Hz;
  every fix is per-DE hand tuning with no preview and no revert timer.
- **User:** Multi-monitor Linux users, 4K+1080p combos, designers/gamers.
- **Evidence (STRONG):** Fedora t/179748 (2026) — *"mouse pointer clicks are registered at incorrect
  positions… consistently in XWayland applications"*; t/185189 — *"my second monitor (max100Hz) showed
  only 50Hz"*; t/139330 (*"144Hz external monitor, but everything except the cursor looks like it's
  60Hz"*); r/linux4noobs 1kf5m5c *"Is there any distro or any desktop environment that handles
  resolution scaling properly?"*; r/linuxmint 1j2md4t (*"On Windows, this works seamlessly… on Linux,
  it's really frustrating"*); HN 46942160 (multi-monitor DPI breaks every screenshot tool).
- **xrandr/wlr-randr scripts, kanshi profiles, disabling fractional scaling, falling back to X11.**
- **Existing alternatives:** KDE KScreen/GNOME Settings, kanshi (FOSS), arandr, nvidia-settings.
- **Why alternatives fail:** no preview/"apply in 30 seconds or revert" wizard, no plain-text diagnosis
  of *what* caps the refresh, XWayland misalignment is never explained.
- **FD Labs angle:** small, self-contained, extremely visible utility — "display doctor" as a trust-builder.
- **MVP:** detect mismatched scale/refresh → explain what's capping Hz → apply+revert presets per
  monitor combo → flag XWayland-misaligned apps.
- **Difficulty:** M (UI easy, correctness hard)
- **Monetization:** free + paid "display doctor" one-time $15–29 (or fully FOSS as a community entry point).
- **Risks:** compositor/driver bugs may be unfixable → must ship as "explain + workaround," not "fix."

---

### 13. Compose drift scanner (read-only, multi-host)

- **Problem:** Compose files scattered across hosts/NFS; nothing shows every host, every project, and
  every running-vs-declared delta, or what changed last month.
- **User:** Homelabbers and small teams running Docker across 2–10 machines.
- **Evidence (STRONG):** r/selfhosted 1qsdk9e *"They all live in different [places]"*; r/selfhosted
  1fhxvvh (hand-parsing compose files to list ports); HN 48719573 `stego-tech` (*"I still need to setup
  some sort of GitOps so I don't have to ssh into the box and manually bootstrap whatever compose file
  I've thrown on there"*); HN comment on distr.sh (2026) — *"Since all the tools… don't do these best
  practices, I made my own"* (yoink.is, health checks + drift detection).
- **git repo per host, NFS-shared folders, `docker compose config` diffs by hand, Ansible.**
- **Existing alternatives:** Komodo/Dockge/Portainer (deployment/editing, weak cross-host drift),
  Yoink.is (new, paid), Uncloud (OSS deploy), NixOS (replaces the problem).
- **Why alternatives fail:** they manage deployment; none answer the *audit* question read-only.
- **FD Labs angle:** S-difficulty, read-only, safe — an ideal first homelab product; natural companion
  to card #14 (inventory) and #1 (restore drills).
- **MVP:** read-only scanner per host → fleet table (projects/images/ports/env diffs) → red rows for
  drift (running container not in file, moved image tag, env mismatch) → JSON/markdown export.
- **Difficulty:** S–M
- **Monetization:** free single-host / paid fleet features.
- **Risks:** overlaps Portainer/Komodo gravity; must stay read-only or it becomes "yet another orchestrator."

---

### 14. Homelab inventory with history (auto-discovered, exportable)

- **Problem:** "What runs where" lives in rotting spreadsheets/draw.io diagrams; discovery is manual;
  nobody keeps *history* ("what changed and when").
- **User:** 3+ host homelabbers; small IT generalists.
- **Evidence (STRONG):** r/homelab 1ec2gdj (*"I keep a spreadsheet populated with the output from my
  Ubiquiti security gateway"*); r/homelab 1c0yh2l (tracking service IPs); r/docker 179hfzg (containers
  across VMs); HN 47973693 `ytjohn` (his self-built harness's first task: *"go find everything that's
  running on these hosts… Builds an inventory, memory, todos"*).
- **Spreadsheets, draw.io, Snipe-IT, NetBox (overkill), manual READMEs.**
- **Existing alternatives:** **homelable** (OSS, **3,755★**, Mar 2026 — racks/patch cables/live status),
  **homedex** (53★, auto-discovery of services/ports/routes), NetBox (free, heavy), Snipe-IT (free/paid),
  phpIPAM, almanaut/Rackbase/tinycmdb (≤3★).
- **Why alternatives fail:** NetBox/Snipe-IT are IT-department shaped (manual forms); homelable/homedex
  are six months old and neither keeps *history*, reconciles intent vs. reality, or offers portable export.
- **FD Labs angle:** differentiate on history/drift/export — not on drawing. Data ownership (your
  inventory file, exportable forever) vs. hobby projects with no export story.
- **MVP:** read-only agent → auto-map Docker/Proxmox/LXC + reverse-proxy routes → diagram **and**
  markdown/JSON export → diffs over time.
- **Difficulty:** M
- **Monetization:** free self-hosted + paid multi-site / inventory backup.
- **Risks:** homelable momentum is real; discovery-only value erodes fast.

---

### 15. Agent context-file staleness doctor (CLAUDE.md / AGENTS.md / README claims vs reality)

- **Problem:** Every repo now carries 3–5 agent "read me" files plus README/ADRs; they drift apart
  instantly and nobody verifies that documented commands, paths, and flags still exist.
- **User:** Teams using Claude Code/Copilot/Cursor; any project with public README + runbooks.
- **Evidence (STRONG for a 2025–26 problem):** r/ClaudeAI 1rz20cb *"How do you keep CLAUDE.md and
  context files from going stale?"*; r/ClaudeCode 1pjysfa *"Context Hell… 5 different 'read me'
  files… they drift apart instantly"*; r/VibeCodersNest 1rort8p (config sprawl, no standard name);
  r/devops 1whh2k6 (runbooks rot first); r/node 1mr5lwd *"I got tired of keeping API docs in sync."*
- **Manual review, occasional PRs, hope.**
- **Existing alternatives:** Readme-Sync (**0 stars**), doc-drift-detector (tiny), adr-tools (stores
  decisions, never checks them), GitBook/Architect (paid SaaS), docsalot/vibecoder blog workarounds.
- **Why alternatives fail:** nothing *verifies* documented claims against the tree, and nothing spans
  multiple agent config formats.
- **FD Labs angle:** cheap, fast, CI-friendly; a "lint for documentation truth" is a perfect S/M wedge
  with clear free/paid split.
- **MVP:** extract every path/command/flag claim from CLAUDE.md/AGENTS.md/`.cursor`/
  `.github/copilot-instructions.md`/README → run/check against the tree → pre-commit/CI gate + patched
  suggestions.
- **Difficulty:** M
- **Monetization:** free local CLI + paid team sync dashboard.
- **Risks:** AGENTS.md standardization could absorb it; false positives erode trust fast.

---

### 16. Multi-agent diff review & merge desk

- **Problem:** Devs run 3–10 parallel coding agents in worktrees; overlapping diffs collide, there is no
  shared state layer, and merging agent output is manual cherry-pick hell.
- **User:** AI-heavy solo devs and 2–10 person teams.
- **Evidence (MEDIUM–STRONG):** r/AI_Agents 1scca4z — *"File conflicts – the #1 problem… state
  synchronization and context drift… lack of a shared state layer"*; github
  johannesjo/parallel-code#86 (merge/rebase UX for overlapping agent diffs); HN 47218318 (*"Parallel
  coding agents with tmux and Markdown Specs"*); category surveys listing 10 tools with none solving overlap.
- **tmux + `git worktree` + hand-merging branches.**
- **Existing alternatives:** Vibe Kanban, Conductor, Claude Squad (free/OSS, active), Warp workflows,
  raw git worktree.
- **Why alternatives fail:** they *schedule* agents; none *reconcile* overlapping diffs or hold shared
  state — each is bespoke glue.
- **FD Labs angle:** highest-ceiling dev tool here; local-first by nature (your worktrees never leave
  the machine).
- **MVP:** watch worktrees → auto-rebase non-overlapping hunks → 3-way review UI for true conflicts →
  per-agent context files.
- **Difficulty:** L
- **Monetization:** one-time $49–99 desktop + free CLI core.
- **Risks:** fast-moving space; Cursor/Warp could bundle it; merge correctness is genuinely hard.

---

### 17. ESP32 flash & port session manager ("port detective")

- **Problem:** Flashing fails on ports/permissions/drivers/boot buttons; "what firmware did I flash to
  this board?" lives in shell history; diagnosis is re-Google every time.
- **User:** Arduino/ESP32/ESPHome makers (large Linux subset).
- **Evidence (STRONG):** esptool #682 (`PermissionError(13)`, 18 comments), #1090 (2025, *"uninstalled
  drivers, tried multiple computers"*), #540 (macOS, 94 comments), PR #984 (docs for Linux serial-port
  failures), official "Bootloader Won't Respond" troubleshooting page; HN 46272399 (*"Adafruit Ampy…
  esptool… picocom… it did take a bit to figure out the syntax… saved in my CLI history"*);
  platformio/platform-espressif32#11 (partition-table offsets).
- **Ctrl+R in shell history, holding BOOT, sudo, replug, reinstall drivers, copying forum commands.**
- **Existing alternatives:** esptool (OSS CLI), ESP Web Flasher (web), PlatformIO/Arduino IDE upload,
  esptool-js, VS Code extensions; self-built helpers (esp32-flashmate, fix_partitions_offset.py).
- **Why alternatives fail:** each is one step of the loop; none *diagnose* (who holds the port? udev?
  wrong boot mode?) or remember per-board history (chip, MAC, firmware, partition table, settings).
- **FD Labs angle:** "port detective" is a genuinely unowned diagnostic; pairs naturally with #18.
- **MVP:** port diagnosis (holders, udev rules, driver state, chip detect) + guided flash + per-board
  ledger (chip/MAC/firmware/date/partition/settings) with backup/restore of NVS + partition table.
- **Difficulty:** M
- **Monetization:** free; paid "workbench suite" or one-time.
- **Risks:** Espressif could improve esptool UX; scope creep into a full IDE.

---

### 18. Maker serial console (multi-port, filters, session capture)

- **Problem:** Serial monitors lack log-level filters, colors, timestamps, multi-port view, and session
  archives; makers rewrite their own in Python within days.
- **User:** Every embedded maker; ESP32/Arduino/RP2040 developers.
- **Evidence (STRONG):** r/embedded 1vb828h *"serial monitors are trash… filter by log level, like
  just show me ERRORS. - colors. please. just colors."*; r/embedded 1bzcwy7 *"I ended up recreating
  something very similar last week in python, I wish I had this to work with"*; Arduino IDE #812
  (select-all, open since 2022, still unmerged PR #2936 as of Jul 2026), #2098/#1250 (monitor limits).
- **`screen`/`picocom`/PuTTY, Arduino IDE monitor, ad-hoc Python scripts, copy-paste into notes.**
- **Existing alternatives:** CoolTerm, RealTerm, PlatformIO device monitor, moserial, Serial Plotter.
- **Why alternatives fail:** single-port, no level filtering/parsing, no session archive tied to the
  project; Linux support uneven; the flagship IDE still can't select-all after four years.
- **FD Labs angle:** daily-use tool with word-of-mouth potential; free tier feeds #17; local by design.
- **MVP:** N ports side-by-side, regex/level filters, timestamps, ANSI colors, hex toggle, "save capture
  to current project."
- **Difficulty:** M
- **Monetization:** free + $15 pro (hex/plot/decode).
- **Risks:** free incumbents; low ARPU — best as part of a workbench bundle.

---

### 19. Small-shop job tracker — the gap between a spreadsheet and Jobber

- **Problem:** 1–3 person repair/service shops need job #, status, customer, parts, due date — nothing
  more. Field-service suites are 10× too big and too expensive; the fallback is paper or Excel.
- **User:** appliance/bike/phone repair, handyman, stone fab, 1–3 person trades.
- **Evidence (STRONG):** r/smallbusiness zm13jr — *"keeping every work we do on paper… All i can come
  up with excel"*; reply: *"There is inventory and job tracking software but it tends to be
  subscription or overpriced"*; r/CRMSoftware 1tsqq46 — *"Is there a gap between spreadsheets and full
  field service CRMs for one person shops?"*; r/Contractor 1micdy5 (*"quotes in QuickBooks, jobs
  scattered across emails and Excel sheets, no real shared calendar or job number system"*);
  r/smallbusiness 1stlnin (*"spent 3 months on jobber and my guys never used it"*; quote of $4k for a
  custom tool); r/AskContractors 1rqe9ux (Jobber ~$150/mo at their seat count).
- **Paper repair orders, Google Sheets RO templates, kanban hacks, memory.**
- **Existing alternatives:** Jobber ($99–150/mo), Housecall Pro (~$200/mo), Workiz, ServiceTitan
  ($300–500/tech/mo), ServiceFusion ($192/mo), RepairShopr/RepairDesk (~$50–75/mo); **no credible
  local-first OSS job tracker found.**
- **Why alternatives fail:** price, seats, and adoption — top reply on the Jobber thread: *"this is not
  a software problem."* Full-suite UX overwhelms a 2-person shop.
- **FD Labs angle:** local-first, no seats, no account, works offline in a garage; ship an importable
  spreadsheet template alongside the app (the distribution pattern that already works in this space).
- **MVP:** job list → job detail (customer, photos, parts, status, hours) → print/PDF work order →
  quote→invoice conversion.
- **Difficulty:** M
- **Monetization:** one-time or $8–15/mo, no seats (one-time preferred per FD Labs thesis).
- **Risks:** crowded low end; **adoption, not features, kills these tools**; mobile capture matters as
  much as desktop.

---

### 20. Chat follow-up ledger (WhatsApp/Instagram leads, no message sync)

- **Problem:** Leads arrive as chat messages, get buried, and follow-up is memory. Users explicitly
  reject full CRMs as "overkill and too expensive."
- **User:** Solo service businesses, trades, agencies, India/SEA/global small biz.
- **Evidence (STRONG):** r/smallbusiness 1tq2jok — *"I've already lost a couple of warm leads this
  week because their chats got buried"*; CRMs *"overkill (and are too expensive)"*; r/B2CSaaS 1r8cgvy
  — *"the moment you have 10+ conversations per day your brain becomes the CRM… there is no state
  attached to a conversation"*; r/IndiaBusiness 1ry8o7f (follow-ups "mostly based on memory");
  r/smallbusiness 1q2ruwy (friend lost a $200k lead; author wrote a ranking script); r/IndiaBusiness
  1rjz8p3 (*"losing 30–40% of leads because… first reply came too late"*).
- **Memory, Excel copy-paste, notebooks, sticky notes.**
- **Existing alternatives:** Zoho/HubSpot CRMs, Zenzap, Zoko, WATI/Gupshup (all cloud, most need the
  WhatsApp Business API + monthly fees), plus in-house scripts.
- **Why alternatives fail:** API tools require Business API + fees; CRMs require data entry nobody does.
- **FD Labs angle:** S-difficulty wedge: paste/export a conversation → extract person + intent + next
  action + reminder. **Deliberately no message sync** — avoids ToS/anti-scraping risk entirely.
- **MVP:** local "follow-up ledger": paste transcript → structured contact + next action + due date →
  daily due list + reminders; export your own data anytime.
- **Difficulty:** S–M
- **Monetization:** $5–10/mo or one-time.
- **Risks:** WhatsApp ToS/anti-scraping if anyone pushes toward sync; constant pull toward becoming a
  generic CRM; extraction quality on messy chats.

---

## SCORING

No fake composite numbers — nine axes, each rated LOW / MEDIUM / HIGH.

| # | Opportunity | Evidence | Pain freq. | Pain severity | Build feasibility | Differentiation | Distribution | Maintenance | Monetization | FD fit |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | Verified restore / backup drills | HIGH | HIGH | HIGH | MEDIUM | HIGH | HIGH | MEDIUM | MEDIUM | HIGH |
| 2 | Linux plain-language diagnosis | HIGH | HIGH | HIGH | LOW-MED | MEDIUM | HIGH | MEDIUM | MEDIUM | HIGH |
| 3 | Print outcome + calibration log | HIGH | HIGH | MED-HIGH | HIGH | HIGH | HIGH | MEDIUM | MEDIUM | HIGH |
| 4 | Local parts-drawer inventory + BOM | HIGH | MED-HIGH | MED-HIGH | MEDIUM | MEDIUM | MEDIUM | MEDIUM | MEDIUM | HIGH |
| 5 | Subtitle timing + translation QA | HIGH | MED | HIGH | MEDIUM | MEDIUM | MEDIUM | MEDIUM | MEDIUM | HIGH |
| 6 | Loudness wizard | HIGH | MED | MED | HIGH | LOW-MED | HIGH | LOW | MEDIUM | HIGH |
| 7 | Wayland recording companion | HIGH | MED-HIGH | HIGH | MEDIUM | MEDIUM | HIGH | MEDIUM | MEDIUM | HIGH |
| 8 | Creator asset library + local search | MEDIUM | MED | MED-HIGH | LOW-MED | MEDIUM | MEDIUM | MEDIUM | MEDIUM | HIGH |
| 9 | Package disk bloat forensics | HIGH | MED-HIGH | MED | MEDIUM | HIGH | HIGH | MEDIUM | MEDIUM | HIGH |
| 10 | Update/driver rollback bodyguard | HIGH | MED (spikes to HIGH) | HIGH | MEDIUM | HIGH | HIGH | MEDIUM | MEDIUM | HIGH |
| 11 | Power/suspend forensics | HIGH | MED-HIGH | MED-HIGH | MEDIUM | MEDIUM | HIGH | MEDIUM | MEDIUM | HIGH |
| 12 | Display profile doctor | HIGH | MED-HIGH | MED | MEDIUM | MEDIUM | HIGH | MEDIUM | LOW-MED | HIGH |
| 13 | Compose drift scanner | HIGH | MED | MED | HIGH | MEDIUM | MEDIUM | LOW-MED | MEDIUM | HIGH |
| 14 | Homelab inventory + history | HIGH | MED | MED | MEDIUM | MEDIUM (vs homelable: LOW) | MEDIUM | MEDIUM | MEDIUM | HIGH |
| 15 | Agent context-file staleness doctor | MED-HIGH | MED | MED | HIGH | MEDIUM | HIGH | LOW-MED | MEDIUM | MED-HIGH |
| 16 | Multi-agent diff merge desk | MEDIUM | MED | HIGH | LOW-MED | HIGH | MEDIUM | HIGH | MEDIUM | MED-HIGH |
| 17 | ESP32 flash/port session manager | HIGH | MED-HIGH | MED-HIGH | MEDIUM | HIGH | HIGH | MEDIUM | LOW-MED | HIGH |
| 18 | Maker serial console | HIGH | HIGH | MED | MEDIUM | LOW-MED | HIGH | LOW | LOW-MED | HIGH |
| 19 | Small-shop job tracker | HIGH | HIGH | HIGH | MEDIUM | MEDIUM | LOW-MED | MEDIUM | MEDIUM | MEDIUM-HIGH |
| 20 | Chat follow-up ledger | HIGH | HIGH | HIGH | HIGH | MEDIUM | MEDIUM | MEDIUM | MEDIUM | MEDIUM |

Notes: LOW build feasibility = genuinely hard; LOW distribution = crowded or hard-to-reach audience;
HIGH maintenance = fast-moving external surface (agent tooling, Wayland, printer APIs).

---

## BORING BUT GOOD

At least twelve — deliberately unglamorous, and several are stronger products than flashy AI ideas.

1. **Backup verifier / restore drill reporter** (card #1) — the flagship of this category.
2. **Package disk bloat forensics** (card #9) — "why is 296 GB gone?"
3. **System diagnosis report in plain language** (card #2).
4. **Certificate + domain + DNS-token expiry board** — r/selfhosted 1ubnhis (*"application monitoring is
   relatively easy compared to DNS/SSL"*); 10+ hobby projects ≤6★ = no trusted winner; S–M.
5. **Compose drift scanner** (card #13).
6. **Household/small-office asset, warranty & receipt vault (local)** — r/homeowners 1n0ahov (label
   makers, receipt ziplock bags), 1o9c9hj, 16xc6n2, 18rbs85; HomeBox precedent; S–M, one-time $15–30.
7. **Calibration & certificate log for small shops** — r/Metrology 13f5auj/1milwdv, r/QualityAssurance
   1rueuoc (Excel/paper/lab-notebook answers); enterprise CMMS is the only incumbent; S–M, best B2B ARPU.
8. **ESP32 "port detective"** (card #17) — hardware diagnosis without hardware.
9. **Maker serial console** (card #18).
10. **Power/outage journal + post-outage health sweep** — NUT gives state, not history; self-built
    notifier (mcguirev10) proves the gap; S–M, best shipped as a module of the health digest.
11. **Job photo documentation, local-first** — r/Contractor 1k2bccs *"We need a job photo organization
    software"*; CompanyCam is per-seat cloud; no local option exists; M (flagged MEDIUM evidence).
12. **Receipt → job costing ("Book of Lies" fix)** — r/Machinists 15rv9ix *"Our old-head calls it 'The
    Book of Lies' because everyone just bullshits it"*; r/smallbusiness 1t3f672, r/Contractor 1rli1ka;
    M.
13. **Host rebuild checklist (guided reinstall)** — HN 41999462 *"I always forget the workflow"*;
    config engines (Ansible/Nix) assume you already know the steps; M.
14. **Batch file/PDF/rename utility cluster** — see "why is this still hard"; huge demand, but hard to
    monetize directly (see SATURATED).

---

## I BUILT THIS FOR MYSELF

The strongest signal category: private tools people depend on daily. Primary sources: HN 27482397
(453 comments), HN 21373931 (192), HN 9471000, HN 45670052, HN 43662421, HN 41999462, plus GitHub
search: 498 issues containing "I wrote a script because".

**Curated examples (URL + underlying repeated problem):**

1. **Personal accounting automation stack** — HN 27482397 `jgimenez`: scraped bank site → Asana tasks →
   invoice extraction from Drive → *"an amorphous collection of scripts… tailor-made for me"*. Problem:
   SMB invoice/bank-entry reconciliation.
2. **20-year self-made invoicing program** — HN 27482397 `alienmaster` (Perl → Python port). Problem:
   solo invoicing without SaaS.
3. **Hardlink Time-Machine-style backup with TCL/TK UI** — same thread + HN 40878812 (ran 5 years).
   Problem: no satisfying GUI backup on Linux.
4. **Calendar union tool → product** — HN 27482397 `roarkry` (O365 + Google merge, then launched).
   Problem: dual-calendar double-booking.
5. **GPS geotagger for photos** — HN 27482397 `jcubic` (Takeout JSON → Android logger → hardware watch).
   Problem: photo geolocation scattered across devices.
6. **50-LOC screenshot tracker** — HN 27482397 (`mac-screenshot-tracker`): *"annoyed by the seemingly
   over-engineered tools for screen tracking."* Problem: bloated quantified-self tooling.
7. **Dice-roll restore test script** — HN 49732513 `PunchyHamster`. Problem: unverified backups.
8. **Terminal-banner backup nudge** — HN 49732513 `smallerfish` (*"I've also forgotten what backup
   system was in place on a prior install"*). Problem: forgotten setup + dead-man switching.
9. **factotum fleet harness** — HN 47973693 `ytjohn` (SSH + ansible + *"go find everything that's
   running on these hosts"*). Problem: fleet inventory.
10. **Compose health/drift tool (yoink.is)** — HN comment on distr.sh. Problem: compose best practices.
11. **Watchdog mixing bash + Uptime Kuma + Beszel** — HN 48719573 `exiguus`. Problem: glue monitoring
    doesn't auto-heal.
12. **`/routine-maintenance` Claude skill** — HN 48719573 `cheschire` (updates, backup validation,
    SMART checks, email report). Problem: recurring maintenance runs.
13. **Mercury transaction script → ditch QuickBooks** — HN 43272270 (`fangpenlin.com` beancount post):
    *"I don't really know what I am getting for $60/month."* Problem: SMB bookkeeping cost.
14. **Stone-fab quoting sheet (Quote → Deposit → CAD → Fab → Install)** — r/CounterTops 1s2h6ba.
    Problem: quoting in your head + sticky notes.
15. **Till Minder (Square drawer tally)** — r/SquarePOS_Users 1oksita: *"Got tired of waiting and made
    my own! I used a spreadsheet for years."* Problem: POS gap ignored for 5 years.
16. **Offline invoicing app built in 7 days** — r/Femalefounders 1s4qzgl: *"Everything I looked for cost
    so much money… No subscriptions. No cloud."*
17. **Filament inventory spreadsheet → app** — r/BambuLab 1f60u4i (FMS), r/3Dprinting 1apqo9g
    (Spoolstock), r/BambuLab 1igmchi. Problem: spool tracking.
18. **NFC-tagged filament bins on Google Sheets + Appsheet** — r/BambuLab 1igmchi `S0k0n0mi`
    (weigh spool → auto-subtract). Problem: spool weight/location.
19. **LED-lit smart component drawers + iPad app** — r/arduino 12pefko; **BuildBook** (parts + code + 3D
    files together) — r/arduino 1tj2s2h. Problem: finding/keeping parts & project assets.
20. **Print-farm manager for wife's 8 Bambus** — HN 47310678 (pause points, magnets, BOM, scheduling).
    Problem: multi-printer small-business orchestration.
21. **CNC operator-steps tool (F#)** — HN 48400963: *"if I come back in 1 year… I pretty much always
    have to open VCarve and eyeball what the hell all the files did."*
22. **Post-processing script to strip G-code comments** — Rinkhals #453: *"it's a hack… generally ugly
    and inconvenient."*
23. **Local log viewer (Telescope, 172 pts)** — HN 43181862: *"most force a specific log format, are
    tied to ingestion pipelines."*
24. **claude-devtools (69 pts)** — HN 47004712: no quick local way to see what Claude Code sessions
    are doing.
25. **commitlog** — r/node 1spvjuh: *"I got tired of writing release notes."*
26. **erm (local um/uhs stripper)** — doug.sh: filler removal required cloud SaaS.
27. **Zush (image renamer/tagger)** — HN 46853044: *"renames them… so search actually works."*
28. **SyncMyCalendars / Index Rusher / local Reddit scraper** — repeated "built for myself → shipped"
    pattern (HN 27510711; HN 39459268; dev.to local-first Reddit tool: cloud version got blocked in
    5 minutes, desktop app from the user's home IP "just works").

**Meta-insight:** the same ~10 problems (backup confidence, inventory, release notes, log viewing,
invoicing, fleet visibility, media search, calibration memory) are rebuilt by strangers every year.
Products hiding in plain sight: **backup verification, local log correlation, parts/spool inventory,
release-notes-from-intent, media asset search.**

---

## WHY IS THIS STILL HARD?

Twenty-five tasks that should feel trivial in 2026 and don't:

1. **Proving a restore works** — HN 49732513; the only GitHub tools have 0–1 stars.
2. **Reclaiming 295 GB of Flatpak objects** — `flatpak remove --unused` and `repair` do nothing (flatpak #5904).
3. **Sharing a screen on Wayland** — black frames, per-compositor portal failures (Fedora t/179819, HN 46942160).
4. **Clicking in the right place with fractional scaling on a second monitor** — Fedora t/179748 (2026).
5. **Getting the second monitor above 50/60 Hz** — Fedora t/185189, t/139330.
6. **Laptop suspend without 0%-by-morning** — Fedora t/131096, HN 46475023.
7. **Mic + sound from Bluetooth headphones** — Fedora t/155135, HN 46475023, r/kde 1hquokv.
8. **Granting a Flatpak app camera access** — CLI `permission-set` no-ops (Fedora t/181312).
9. **Flashing an ESP32/ESP8266** — `PermissionError(13)`, BOOT-button rituals, 94-comment macOS thread (esptool #682/#540/#1090).
10. **Reading serial output usefully** — r/embedded *"serial monitors are trash"*; Arduino IDE still lacks select-all after 4 years (#812, PR #2936 unmerged).
11. **Comparing two sliced G-code files** — Reddit users ask strangers to diff them; Bambu exports G-code it can't re-import (HN 49478118).
12. **BOM/inventory management** — r/embedded (2026): *"Why is BOM management still stuck in Excel in 2026?"*
13. **Remembering calibration state** — sticky notes taped to printers (r/3Dprinting 1ttd6dy).
14. **Merging/compressing/numbering PDFs** — **5 separate Show HN threads** (43880962 with 791 pts, 46516400, 47059477, 46702843, 49220498).
15. **Batch file renaming** — 4 threads; *"adrenaline levels go up by about two espressos"* (HN 44081850); *"I'd love a way that isn't miserable"* (HN 48858121).
16. **Plain-English media conversion instead of ffmpeg** — HN 46400251 (420 pts, 198 comments).
17. **Scrolling screenshots** — still missing from Windows 11 Snipping Tool in 2026 (screenshotapi.net); r/linux4noobs 10943x5.
18. **Photo library de-duplication** — HN 40371467 (*"A reliably good app that helps me delete duplicates… would be amazing"*), 47363279, 47150083, 45446834.
19. **Android backup/restore** — *"Do Android Backups finally fully work in 2026?"* (r/AndroidQuestions 1ue8zdh).
20. **DOCX → PDF fidelity** — admitted unsolved by builders (r/node 1sdu0mj, docx-to-pdf repo).
21. **Fixing auto-caption timestamps outside English** — r/aitubers 1w7s7ve; Aegisub *"really Neanderthal"* (#78).
22. **Recording screen + mic + cam on Wayland** — r/linux_gaming 1echnyi et al.
23. **Podcast loudness targets without a physics degree** — r/podcasting 1snjpxo, 1na6h4t, 11tacaa.
24. **Digitizing a box of VHS on Linux** — r/DataHoarder 1i65t5f; multi-tool half-documented grind (m6wiq.uk writeup).
25. **Rebuilding a host from memory** — HN 41999462 *"I always forget the workflow"* (54 comments; answers remain bash-vs-Ansible-vs-Nix tribalism).
26. **Mapping patch-panel → switch port** — r/networking 2a0dsw: the top answer is *"use colour coding"* (i.e., avoid documenting it).
27. **Following up on chat leads** — r/smallbusiness 1tq2jok, 1q2ruwy.
28. **Physical file transfer between machines without cloud** — Locansend-style Show HN rebuilds (HN 47933208).

---

## LINUX OPPORTUNITIES

Cards: #2 (diagnosis), #9 (disk bloat), #10 (rollback), #11 (power), #12 (display), #7 (Wayland recording).

Additional candidates (condensed):

- **Wayland capture/portal health checker** — test each capture path (portal/PipeWire/native), report
  which backend works, repair portal services. S–M. Overlaps #7. (MEDIUM)
- **Flatpak/portal permission unifier** — one dashboard over flatpak overrides + xdg portals + device
  nodes; "why is my camera blocked?" (Fedora t/181312, t/66531, t/129103; Flatseal exists but three
  layers still disagree). S–M. (WEAK-MODERATE)
- **Windows→Linux migrator** — scan a mounted Windows drive → propose Linux equivalents → set MIME
  defaults in one click (Fedora t/163348 requires hand-editing `mimeapps.list`; HN 45556153 notes the
  good file-manager for this is abandonware). M. (**WEAK** on acute-pain evidence — mostly "app gaps.")
- **Gaming health panel** — shader-cache churn (HN 44385799: daily re-downloads *"plaguing users for
  years"*), HDR capability report (HN 46458339), controller status (Fedora t/141168, t/98530).
  S–M. (MODERATE; Valve could ship it)
- **Printer/scanner doctor** — queue/driver/permission/discovery in one screen + share-wizard for old
  MFPs (HN 46459521, 49411421, 46800253). S–M. (**WEAK-MODERATE**, counter-evidence: new printers work OOTB)
- **Personal backup + verified restore on the desktop** — folded into #1 (Timeshift excludes $HOME;
  Fedora t/114944 shows hand-rolled rsync + systemd units).

---

## MAKER OPPORTUNITIES

Cards: #3 (print outcome + calibration), #4 (parts/BOM), #17 (ESP32 flash), #18 (serial console).

Additional candidates (condensed):

- **Maker project notebook** — project = folder: markdown + wiring photo/PDF + BOM table + code + G-code
  list, local search across projects. Evidence: r/arduino 1554iaa, 1tj2s2h (self-built BuildBook), HN
  48400963 (CNC project rot). M. (STRONG; notes-market crowding is the risk)
- **G-code preview + semantic diff** — drop 2 files → layer-by-layer overlay diff + header/settings diff.
  Evidence: r/3Dprinting wpd652/rixe6l (users asking strangers to compare), HN 41548823 (*"about to
  break down and buy tkcnc"*), HN 49478118 (Bambu can't re-import its own G-code). L (XL for material-
  removal sim). (STRONG demand, heavy build)
- **Hobby CNC tool-life ledger** — tool records + cut-time log → "tool #7 at 82% of historical life" +
  per-job tool-change sheet. Evidence: r/hobbycnc 1mhkznn (*"Fusion… not intended to calculate feeds
  and speeds or wear and tear"*), r/Machinists 18jqn8z, HN 48400963. M. (MEDIUM)
- **Small-shop calibration/certificate log** — see BORING BUT GOOD #7. S–M, best B2B ARPU.
- **Print-business job cost/schedule tracker** — HN 47310678/48090439 (8-Bambu farm for his wife,
  pause points, magnets, BOM). L. (STRONG niche; crowded edges + fragile vendor APIs)
- **Filament inventory** — STRONG demand but **Spoolman (2.8k★, MIT, active) + AMS auto-tracking own
  the generic case**; only viable as "spool + recipe + drying log," bundled with #3.
- **G-code post-processing pipeline manager** — visual chain of transforms with before/after diff
  (Rinkhals #453 calls current scripts *"ugly and inconvenient"*). M, fold into the G-code differ. (**WEAK-ish**)

---

## DEVELOPER OPPORTUNITIES

Cards: #15 (context-file staleness doctor), #16 (multi-agent diff merge desk).

Additional candidates (condensed):

- **Local multi-source log viewer** — globs → format auto-detect → merge rotated files by time →
  side-by-side tails → filter by trace ID. Evidence: r/linuxadmin 1wg5fme (*"What finally made you stop
  grepping through log files?"*), HN 43181862 (Telescope, 172 pts), pm2-logrotate #146, r/Backend
  1o6dn0h (*"manually query logs of each service and correlate"*). M. (**lnav is free and excellent** —
  must win on multi-source + correlation UX only)
- **Codebase orientation map** — LSP-backed call graph + entry-point walk → auto "orientation report"
  (key paths, data flow, config provenance). Evidence: r/AskProgramming 1jupmxi, r/devops fah0jw
  (*"Sourcegraph is great but not useful for visualization"*), Sourcetrail dead/archived (HN 21567556
  *"There's no good tool out there for reading source code"*). L. (MEDIUM — multi-language is hard)
- **Flaky test triage desk (local)** — cluster failures, compute flake rate, auto-bisect via worktrees.
  Evidence: HN 46967724 (*"retries everywhere, quarantining tests… slowly losing trust in CI"*),
  r/softwaretesting 1tjbkcs. M–L. Trunk/CI vendors are the incumbents.
- **Devcontainer/local-env failure debugger** (`envdoctor`) — build, parse each feature/layer, explain
  the first real failure, diff expected vs actual env. Evidence: devcontainers/features#1494,
  r/ExperiencedDevs x1srkq (*"It transformed the 'get set up' problem into a 'debug and maintain the
  docker container' problem"*), HN 46613997. M. (Microsoft owns the ecosystem — risk)
- **Doc-drift detector (README/ADR/runbook)** — verify commands/flags/routes from Markdown against
  code/tests in CI. r/devops 1whh2k6, r/node 1mr5lwd. M. (Natural sibling of #15 — likely one product.)
- **Local-first secret/.env manager** — encrypted local vault, per-process injection, no account.
  Evidence: 1Password community *"Frustrations with .env File Handling"* (*"no real standard solution"*),
  1Password Environments still **beta** and subscription-gated; direnv/SOPS cover part of it. S–M.
- **Dependency upgrade shrinker** — minimal compatible upgrade batch + codemods + one PR.
  r/ExperiencedDevs 1ksemp6 (*"months long tasks of 'upgrading a stack'"*), dependabot-core #4031,
  renovate #37131 (*"graveyard of stale branches"*). **L–XL** — document only.
- **Release notes from intent (diff + PR bodies + issues)** — S–M, but **partially saturated**
  (git-cliff, release-please, conventional-changelog).
- **WEAK, not recommended:** build-log first-error reporter (pain real, no one asks for the product),
  seed-data/fixture manager (thin demand), SSH/tmux session persistence (power users solved it themselves).

**Explicitly out of scope:** another AI coding chat; another Postman clone (see SATURATED).

---

## CREATOR OPPORTUNITIES

Cards: #5 (subtitle QA), #6 (loudness wizard), #7 (Wayland recording), #8 (asset library).

Additional candidates (condensed):

- **Conservative silence/filler removal with a review pass** — suggested cuts on a timeline, per-category
  thresholds, human approve/reject in one pass, export + EDL. Evidence: doug.sh/erm HN discussion
  (*tools "cut too aggressively"*), r/podcasting 1j4s444 (*"I built an iOS app that auto-removes silence
  & filler words"*), mbleigh.dev HN comment (*"I haven't find a tool that does that well yet"*). M.
  Descript ($16–30/mo) is the incumbent.
- **Deliverable packager** — folder in → per-platform package out (video + SRT + thumbnail + metadata +
  naming + spec validation), queueable/resumable. Evidence: HN 44958528 (*"automate that batch job of
  changing the intro sequence of the thousand video files and generate new thumbnails"*), r/linux 9t7sv,
  r/AV1 uqdkiy. M. **Differentiate on deliverable packaging, not transcoding** (Shutter Encoder ships
  Linux; HandBrake is strong).
- **Creator backup with media-aware restore drills** — dedupe takes, ignore proxies, verify *playable*,
  weekly restore test. r/DataHoarder 1i65t5f/19er5gv/1lhe4lz; HN 40878812 self-build. M. (Fold card #1
  in here for the creator edition.)
- **VHS/DV/tape archival suite for Linux** — device check → capture → scene auto-split → cleanup
  presets → manifest. r/DataHoarder 1i65t5f; m6wiq.uk capture writeup; dvgrab stale/CLI-only. L,
  one-time $30–45. (MEDIUM; hardware-dependent)
- **Chapter/marker writer (one list → MP3 CHAP/MP4/M4B/YouTube/RSS)** — HN 48662245 (*"raw generation
  ended up being maybe 30% of the work"*). S. (**WEAK** — free feature inside another product.)
- **WEAK/not recommended:** generic batch image resizers (XnConvert/ImageMagick cover it), AI thumbnail
  generators (saturated SaaS), anything PULSAR itself already does.

---

## HOMELAB OPPORTUNITIES

Cards: #1 (verified restore), #13 (compose drift), #14 (inventory + history).

Additional candidates (condensed):

- **DR runbook generator + rehearsal** — inventory → ordered rebuild procedure (commands, secrets
  locations, estimates) → monthly "tabletop" check that each step still exists. Evidence: HN 44618687
  `binwiederhier` (*"Backup is not disaster recovery… If you cannot get back up and running in a few
  minutes/hours, your customers will lose your trust"*), unraid-config-guardian (171★, demand signal),
  HN 49732513 `wolfi1`. L.
- **Certificate + domain + DNS-token expiry board** — one YAML list → daily registrar expiry + DNS API
  token validity + every served chain (incl. wildcards) + ACME account status → countdown board.
  r/selfhosted 1ubnhis, 14cr5aq (*"A tool to create my own SSL CA… for all my selfhosted stuff"*).
  S–M. (**10+ hobby projects ≤6★ = no trusted winner**)
- **Host rebuild checklist (guided reinstall)** — each step is a human task or executable snippet with
  pre/post checks; export to Ansible when you outgrow it. HN 41999462 (`JP_Watts`: *"Setting up a new
  Linux server… very repetitive. I always forget the workflow"*; `phendrenad2`: *"I have a text file
  with a series of steps"*). M.
- **Physical topology via LLDP/CDP + SNMP** — auto-suggest patch-panel map, reconcile documented vs
  observed links. r/networking 2a0dsw; homelable covers drawing, not verification. M–L.
- **Plain-language health digest** — SMART + NUT/UPS + backup jobs + cert dates + container health →
  daily verdict (*"everything OK"* widget). HN 48719573 (`exiguus`, `_pdp_`: *"I wrote a small agent…
  that does all the monitoring"*), HN 49732513 (`overtone1000` healthchecks.io blowups), eastkode.in
  (*"the honest gap"*: monitoring from outside). S–M. **Category flagged saturated** — must ship as a
  verdict, not another dashboard.
- **UPS/outage journal + post-outage sweep** — NUT events → outage log, battery-age alerts, "did
  everything come back?" check. r/homelab 1e6iu1s, 18c3t0d; mcguirev10 self-built notifier. S–M.
  (WEAK-MEDIUM alone; module of the digest.)
- **WEAK, validate before building:** homelab change log ("what did I change last month?" — repeated
  HN queries returned zero tool demand; may be a non-problem), access inventory ("who can reach what"),
  license/warranty tracker (Snipe-IT gravity), generic dashboard/startpage (saturated).

---

## SMALL BUSINESS OPPORTUNITIES

Cards: #19 (job tracker), #20 (chat follow-up ledger).

Additional candidates (condensed):

- **Recurring maintenance due-list** — asset/customer + interval + last-serviced → "due this month" →
  one-tap done with photo. r/smallbusiness 1u779tc (*"after a year it's easy to lose track of who is
  due… spreadsheets and calendar reminders, but it's becoming messy"*), r/manufacturing 1q28aib
  (*"we're supposed to log work in an Excel sheet"*). S–M. Attaches to #19.
- **Receipt → job costing** — see BORING BUT GOOD #12. M.
- **Household/small-office asset + warranty vault (local)** — see BORING BUT GOOD #6. S–M.
- **Direct booking for 1–6 rooms/units** — r/airbnb_hosts 1rny9ur (*"fees are starting to feel like a
  second mortgage"*), r/ShortTermRentals 1sorwfe (*"Over $4,200/month… a mortgage payment"*),
  r/airbnb_hosts 1ba6ffc (*"We just now block out the dates… some Google calendar type thing"*),
  r/Wordpress 1rm158q (free plugins paywall iCal sync). M–L (payments + double-booking correctness).
  One-time desktop + cheap hosted booking page.
- **Market stall inventory with no barcodes** — r/CraftFairs 1t29n8q (*"There's no way I can barcode
  each individual item… don't want to keep anyone waiting while I flap through pages in a notebook"*),
  r/Entrepreneur r3mkkc (paper slips in a binder), r/googlesheets 1j6f6kn (*"300-400 [sold] at an
  event… super time consuming"*). S–M, offline-first at fairgrounds is the wedge.
- **Quote → estimate → invoice PDF** — r/cinematography 1o6or1q (Adobe Acrobat $30/mo), r/ausbusiness
  1t39wps (Xero $75/mo for 1 person), r/Entrepreneurs 1u2jn6l (*"Jobber costs $99/month… built for
  10+ person teams"*), r/freelancing 1s0yash (*"the spreadsheet phase… works until about client 5-6"*).
  S–M — **most crowded bucket**: only price/local-first differentiation works.
- **Job photo documentation, local-first** — see BORING BUT GOOD #11. M.
- **Deposits/no-show tooling, photographer admin CRM, habit/task capture** — evidence MEDIUM/WEAK;
  attach as features rather than products. r/SaaS 1q5lfur's 9,300-post analysis says productivity is
  the most crowded request category (1,231 posts) — volume ≠ opportunity.

---

## 5 DEEP-RESEARCH CANDIDATES

Not a single winner — these five deserve a second, harder validation pass (customer interviews,
competitor teardowns, willingness-to-pay probes) before any build decision.

1. **Verified restore / backup drills** (card #1) — loudest cross-domain pain, near-zero incumbents
   (0–1★ alternatives), pure local-first fit, reusable across desktop/homelab/creator editions.
   *Validate:* would users pay for a "report card"? Will restic/kopia ship it first?
2. **Print outcome log + calibration ledger** (card #3) — 246-comment evidence thread, zero credible
   local-first incumbent, S–M difficulty, maker community is reachable and vocal.
   *Validate:* import fidelity across PrusaSlicer/Orca/Bambu; do vendors see it as a feature?
3. **Linux plain-language diagnosis** (card #2) — universal recurring pain, huge word-of-mouth upside,
   offline/privacy story. *Validate:* can a knowledge base be built without false-positive bricking?
   Which first vertical (NVIDIA? audio? suspend?)?
4. **Local parts-drawer inventory + BOM** (card #4) — 15+ years of complaints, the founder of the
   paid incumbent admits the spreadsheet is his biggest competitor; import/label UX is the whole game.
   *Validate:* willingness to pay vs free Part-DB; whether drawer-first beats BOM-first.
5. **Subtitle timing repair + translation QA** (card #5) — sharp recurring pain for a reachable creator
   audience, direct PULSAR component reuse, offline pitch for pre-release material.
   *Validate:* Whisper accuracy per language; whether Aegisub/Subtitle Edit users will pay for UX.

**Runners-up:** compose drift scanner (#13), Wayland recording (#7), agent context staleness (#15),
small-shop job tracker (#19), asset/warranty vault.

---

## 5 WILDCARDS

Strange on purpose — each still tied to a real, evidenced use case.

1. **Bench measurement logger (software + test equipment).** One local app that reads multiple
   instruments (sigrok/pyvisa/serial: multimeters, scopes, power meters) into one timestamped log with
   annotation + CSV export. *Evidence:* pyLabDataLogger README — *"the software is closed-source and
   this makes it very hard to get very different pieces of equipment… saving data into a single file at
   the same time"*; UltraDMM's mission page — *"every multimeter comes with different software (or no
   software at all)"*; Watts-Up logger, multilog (research institute), and dozens of one-off scripts
   prove the reinvention. *Difficulty M.* *Risk:* instrument protocol breadth; vendor Windows tools.
2. **Parametric PCB enclosure generator (software + 3D printing).** Enter board dimensions, keep-outs,
   port cutouts → printable case with 3D preview + OpenSCAD/STL export, desktop app. *Evidence:*
   r/3Dprinting 1hjbyoy — *"Can anyone direct me to a program or website where i can just input some
   dimensions of a pcb to create an enclosure?"*; the answer today is 10+ fragmented OpenSCAD scripts
   (YAPP, Enclosure-CAD, harrydobbs, MakerWorld generators) with known usability complaints (Hackaday
   2022 comment thread: *"the author's familiarity with his own project ≠ anyone else's… magic values"*).
   *Difficulty S–M.* *Risk:* open-source scripts are free; must win on UX + preview + clearance checks.
3. **NFC/QR bin-tagging kit for local inventory (software + cheap hardware).** ~$15 of NFC stickers or
   QR labels + a desktop app: tap/scan a bin → the part/spool/tool record. *Evidence:* r/BambuLab
   1igmchi `S0k0n0mi` (embedded NFC tags in swatches/boxes, weighing spools through Appsheet);
   r/arduino `lolslim` (*"I have a roll of nfc stickers im going to place on each bin… I may just code
   my own inventory management"*); SpoolEase proves the hardware works but is filament+Bambu-specific;
   SimplyPrint charges for NFC features. *FD Labs angle:* hardware+software bundle (Sticker pack + app),
   generic across parts/filament/tools/totes. *Difficulty M.* *Risk:* phone NFC assumed — need a cheap
   USB reader path for desktop.
4. **"Appliance maker" — old laptop/NAS → local AI node, guided (software + old hardware + local AI).**
   A desktop wizard that takes a dusty laptop or NAS and turns it into a local transcription/indexing
   appliance (whisper + file search) with health reporting — not another walled app store. *Evidence:*
   r/selfhosted 1m6z014, r/homelab 1axkvf9, dozens of "how do I convert my old laptop" threads where
   every answer is manual SSH/Samba/lid-close lore; Umbrel complaints — *"app store is small, outdated…
   difficult to use non-approved software"* (r/selfhosted 16qekm6), *"Isn't it a bit counter productive
   to have people create an entire store just to install an extra app?"* (community.umbrel.com 11172);
   CasaOS/Umbrel alternatives thread 1zh4ot3. *Difficulty M–L.* *Risk:* CasaOS/Umbrel exist; must win on
   honesty (read-only inventory, no walled store, plain-English health) and on the AI workload angle.
5. **Outage journal + power-loss probe (software + ESP32 + homelab).** NUT event collector → outage
   log (start/end/duration/runtime used), battery-age alerts, automatic post-outage "did everything come
   back?" sweep; optional $10 ESP32 mains-loss/temperature probe for the closet. *Evidence:* r/homelab
   1e6iu1s (*"I'm looking for a local (non-cloud), free solution…"*), 18c3t0d (*"any *simple* ups
   management tools for Windows that can speak to a NUT daemon?"*), specbranch/ergaster writeups
   (*"once I discovered the hard way that the UPS batteries couldn't hold a charge"*), mcguirev10's
   self-built notifier. *Difficulty S–M.* *Risk:* small market alone — ship as a module of the health
   digest. Evidence grade MEDIUM-WEAK.

---

## SATURATED IDEAS TO AVOID

Having competitors is fine; these are either crowded *and* commoditized, or crowded *and* free:

- **Homelab dashboards/startpages** — Homepage, Homarr, Dashy, Glance, Lab Dash + clones. Saturated-with-complaints but no pricing room.
- **\*arr stack tools, generic uptime pingers** — Uptime Kuma + many clones.
- **General wikis** — BookStack/Outline/Outline-alikes: plain saturated.
- **"Visual homelab map" as a first product** — homelable (3,755★) arrived Mar 2026; homedex follows. Only *history/drift/export* remains.
- **Generic filament inventory apps** — Spoolman (2.8k★, MIT, active) + AMS auto-tracking + SimplyPrint's free tier.
- **3D print-farm dashboards & AI failure detection** — Obico (OSS/free), OctoEverywhere, SimplyPrint, Printago, 3DPrinterOS, FDM Monster, Bambu Farm Manager (free, local), plus a 2026 wave of ≤5★ GitHub clones. OctoFarm died — the niche got *more* crowded.
- **Electronics parts databases (head-on)** — PartsBox (funded), Part-DB, InvenTree, Binner, Kitspace/Octopart integrations. Only the drawer-first, offline, label-printing wedge is open.
- **Postman-style API clients** — Bruno (43.7k★), Hoppscotch (57k★), Yaak (18.6k★), Posting (11.9k★), Insomnia, Thunder Client, Hurl, Testfully, Yaade, Restfox. Postman's 2026 pricing ($19/user/mo Team) annoys people, but the field is jammed.
- **Agent kanban/manager layer** — Vibe Kanban, Conductor, Claude Squad + Warp/Cursor built-ins (10+ tools surveyed). Only *diff reconciliation* looks open.
- **Whisper/transcription wrappers & dictation apps** — dozens of Show-HN clones; one-time-purchase players already exist (MacWhisper, $20 lucidvoice, OpenWhispr). The *pricing anger* is real (HN 45923352) but the supply caught up.
- **Codec transcoders / ffmpeg GUIs** — HandBrake, Shutter Encoder (**ships a Linux .deb**), XnConvert, FFmpeg GUIs.
- **Generic batch image resizers, background removers, AI thumbnail generators** — web-SaaS saturated.
- **Video editors / AI clip slicers** — PULSAR's own category; do not duplicate.
- **Commit-list changelog generators** — git-cliff, release-please, conventional-changelog.
- **Sourcetrail-style call-graph viewers** — repeated dead attempts; only the "orientation report" framing looks unclaimed.
- **SaaS secret managers** — Doppler, Vault, Infisical, 1Password Environments.
- **Photo dedup tools** — several active (SnapSift et al.); crowded and free.
- **PDF merge / batch rename as standalone paid products** — constant rebuild signal (5 and 4 HN threads) but free tools everywhere → treat as features, not businesses.
- **Field-service FSM low end, invoicing, booking/PMS, habit/to-do apps** — Jobber/Housecall/Xero/Fresha/Habitica economics + 1,231 productivity requests in one analysis = volume without moat.
- **Self-hosted "app store OS"** — CasaOS/Umbrel own the shape; complaints are about trust and walled gardens, not a missing product.

---

## IDEAS BLOCKED BY NETWORK EFFECTS / DATA / LEGAL ISSUES

- **WhatsApp/Instagram/Email inbox sync or auto-reply** — WhatsApp ToS + anti-scraping; Business API
  pricing. The safe form (manual paste/export, card #20) deliberately avoids this.
- **Anything depending on Bambu/printer cloud APIs** — proprietary, key-pair auth disputes, AGPL
  controversy (2025); LAN-mode read-only only, and expect breakage.
- **OTA/channel-manager booking (real distribution)** — Airbnb/VRBO API access and iCal scraping limits;
  two-sided trust for guest payments. The narrow form (own-site booking page + Stripe) stays legal.
- **Multi-account social posting/scheduling** — platform ToS + API quotas; distribution blocked.
- **YouTube upload/metadata automation at scale** — API quotas make a hosted product fragile; local
  one-account use is fine.
- **Marketplace for 3D models / print-on-demand** — network effects on both sides + IP licensing.
- **Self-hosted app store / community repo** — needs a packaging community (Umbrel/CasaOS model);
  maintenance treadmill across hundreds of upstream apps.
- **Stock music/SFX search licensing** — legal/licensing minefield; *organizing the user's own library*
  (card #8) is the legal path.
- **Real-time cloud transcription/diarization as the core** — feasible but requires GPU infra (XL) and
  competes with local models; contradicts FD Labs' local-first thesis.
- **Anything requiring per-seat CRM/FSM distribution** — not legal-blocked, but the sales motion
  (outbound, seats, integrations) is a different company than FD Labs.

---

## SURPRISES

1. **The spreadsheet admits it's the competitor** — a funded founder said it out loud (PartsBox, HN
   46268452), and the same confession appears in filament, calibration, IT inventory, and job costing.
2. **Backup verification — universal agreement, zero products.** Four independent HN/Reddit phrases,
   0–1★ GitHub tools. The most obvious white space found.
3. **"Empty markets" filled up mid-research.** Homelab visual mapping went from "nobody does this" to
   homelable at 3,755★ in six months; print-farm dashboards spawned a wave of new 2026 repos. Blue
   oceans here last months, not years.
4. **Hypotheses that died:** HandBrake/FileZilla/WinRAR/WinSCP/OBS/Sublime have *no* genuine 2024–26
   complaint threads; Typora is one-time $14.99, not a subscription; Shutter Encoder **does** ship Linux;
   folder comparison had only demand-side evidence. (Falsified and discarded.)
5. **PDF merge and batch rename get rebuilt constantly on HN** (5 and 4 Show HN threads) — demand is
   real, but free incumbents absorb it: proof that "repeated rebuilds" ≠ "paying market."
6. **API-client churn is a documented migration path** — *"postman → insomnia → back to postman →
   bruno"* (HN 43185836) — churn is real, yet the destination (Bruno, 43.7k★) shows the window closed.
7. **Adoption, not features, kills small-business software** — *"spent 3 months on jobber and my guys
   never used it"*; top reply *"this is not a software problem."* Distribution insight: ship the
   importable spreadsheet template *with* the app.
8. **Local-first has a distribution proof point** — a builder abandoned cloud scraping (VPS blocked in
   5 minutes) for a desktop app that runs from the user's home IP: *"It just… works."* Local-first isn't
   only ethics; it's a technical advantage against rate-limited APIs.
9. **Offline-first is quantified** — 640+ of 9,300 "I wish there was an app" posts (6.9%) explicitly
   asked for offline/privacy tools in a six-month window.
10. **Wayland pain unified three tracks at once** — screenshot tools, screen sharing, and recording all
    fail on the same xdg-desktop-portal/PipeWire seam; one root cause, three product opportunities.

---

## METHOD NOTES & CAVEATS

- ~250 searches/fetches across 7 parallel tracks; `websearch` hit HTTP 401 late in the run and Reddit
  JSON returned 403 throughout — Reddit evidence is snippet-level (URLs verified, quotes as surfaced).
  HN quotes come from the Algolia API after direct pages hit 429.
- Product/Hunt-style listicles were not used as evidence; vendor pages appear only in COMPETITION fields.
- WEAK flags are honest: Windows→Linux migration urgency, homelab change logs, access inventories,
  maker "archive of makes," standalone chapter tools, batch-image resale, build-log rewriter.
- Prices quoted are from user threads (what people *pay*), not vendor pages, unless marked.

---

## FILES CREATED

- `docs/research/fd-labs-software-gap-hunt-2026.md` (this document)

---

## NEXT STEPS

Per mission instructions: **stop here.** No candidate has been built. The decision of which of the
five deep-research candidates to pursue is deliberately left open.
