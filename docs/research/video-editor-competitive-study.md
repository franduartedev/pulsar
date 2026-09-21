# PULSAR — Video Editor Competitive Study

- Status: junior competitive-intelligence mission (no production code touched)
- Retrieval date for all web sources: **2026-09-21**
- Scope: depth over count. Core: DaVinci Resolve, Adobe Premiere Pro, Final Cut Pro, CapCut Desktop, Filmora, Kdenlive, Shotcut. Lighter: Olive, Lightworks, VEGAS Pro, Avid Media Composer.
- Method: official docs / release notes / manuals + Reddit / forums / issue trackers / experienced-editor reviews. Every claim is tagged:
  - **[V]** = verified product behavior (official docs, reproducible settings, manuals)
  - **[U]** = user opinion / forum report (repeated pattern required before it becomes a finding; single angry comments ignored)
  - **[A]** = our analysis for PULSAR (inference, clearly separated)
- PULSAR context respected: precise timeline editing, strong audio foundations (M10), waveforms (A8), low-memory design, "PC-patata" support, future adaptive performance (Auto/Lite/Balanced/Pro), optional AI, 191-SFX PULSAR Sounds pack. No proposal here rewrites frozen architecture.

---

## 1. Competitor profiles (required format)

### 1.1 DaVinci Resolve (v19 / v20 era, 2025–2026; Studio $295 perpetual, generous free tier)

**Best thing it does.** All-in-one finishing without round-trips: Edit + Color + Fairlight + Fusion + Deliver in one app, with best-in-class color and a real DAW-grade audio page, at $0 / $295-once. **[V]** Blackmagic product/what's-new pages. **[U]** Consistent "switched for price + color" theme across r/editors, r/Filmmakers.

**Why that works.** It removes the two most hated round-trips (grade, mix) and replaces subscription anxiety with a trust-building price. Constant Blackmagic feature drops reinforce "works FOR me". **[A]**

**Worst recurring friction (with evidence).**
1. Cache/preview model is manual and clip-based (Smart/User cache, Optimized Media, per-effect Render Cache); no simple Premiere-style "Render In-Out". Users report re-caching from timeline start, lost cache on relaunch (EXR), per-clip toggles. Long Blackmagic forum thread (2024–2025) begging for render-preview. **[V+U]**
2. Project frame rate locks at creation from first imported clip (defaults 24 fps, no preset/template); changing it means a new project. Larry Jordan Apr-2025 test of 35 interviews. **[U]**
3. Live Save overwrites the live project on every action; a crash can persist corrupted state unless timed Project Backups are also on. Reddit advice: "disable Live Save, enable Auto Backups". **[U]**
4. Fusion titles: no accented characters in some templates, no easy lower-third/drop-shadow without Fusion; render queue is live-linked to the timeline (editing after queueing mutates queued jobs — no variant queueing). **[U]**

**Timeline philosophy.** Classic unlimited-track NLE. No magnetic timeline. Snapping (N), ripple trim/delete/extend, blade (B). Cut page (fast assembly) + Edit page (precision). Ripple is per-track/lock-state; users call it counterintuitive vs FCP and ask for "elastic ripple where all unlocked tracks move together". **[V+U]**

**Performance philosophy.** GPU-first + user-managed proxies/cache/optimized media/timeline-proxy-resolution. Excellent on discrete GPU / Studio + NVIDIA / Apple Silicon; punishing on iGPU/low-end. No true background render; "read Chapter 8 of the manual" tuning burden. **[V+U]**

**Audio philosophy.** Fairlight = real mixer: strips, buses/aux, meters (incl. YouTube-loudness targeting), Voice Isolation, Dialogue Matcher, AI Audio Assistant (auto level/fades). Weaknesses: no track lock (easy knock-out-of-sync), no audio templates, no multicam multi-track edit in Fairlight, Edit/Fairlight playhead desync, sub-frame de-pop requires disabling frame-lock + deep zoom. **[V+U]**

**Target user.** Aspiring-pro → pro color/finishing editors, YouTubers/freelancers wanting one app, Win/Linux/Mac. Steep curve; pays off "after time in seat".

**What PULSAR should learn.** Generous free tier + perpetual option as trust; integrated capable audio + one-click loudness normalize ("YouTube perfect"); per-effect cache granularity is powerful *if* defaults are sane.

**What PULSAR should NOT copy.** Live-linked render queue; clip-cache without In-Out preview; first-import frame-rate lock; Live Save without versioned backups; forcing simple titles through a node compositor.

### 1.2 Adobe Premiere Pro (v25/26, 2025–2026; subscription)

**Best thing it does.** Ecosystem depth + best speech/text workflow: auto-transcribe (cloud, rated far more accurate than Resolve in Jordan tests), Text-Based Editing (edit by deleting transcript text), filler-word/language detection, Audio Category Tagging, Essential Sound presets, Dynamic Link to AE/Audition/Media Encoder. **[V+U]**

**Why that works.** It attacks the highest-volume repetitive work (interviews, podcasts, captions) directly in the timeline, and leverages switching-cost moats (AE templates/MOGRTs, Audition round-trip). **[A]**

**Worst recurring friction.**
1. Reliability 2024–2026: crash/autosave-loss threads ("crash deleted all project files and auto-saves", "autosave every 2 min crashes", "2025 is broken"). Adobe Community + r/premiere. **[U]**
2. Slowness: "navigating 2025 makes me hate my job", slow render/export vs FCP/Resolve. **[U]**
3. Business: $22.99/mo single-app, ~$69.99/mo CC Pro (Jul-2026 pricing reports), annual early-cancellation fee = 50% remaining, mandatory Creative Cloud login. Constant complaint/legal theme. **[V+U]**
4. Controversial 2026 "dynamic ripple" behavior change without opt-in. **[U]**

**Timeline philosophy.** Classic track + snapping (S), ripple tool, ripple delete (Shift+Delete), Q/W ripple-trim-to-playhead, blade (C), linked selection, work area. Flexible but manual: gaps/collisions allowed, sync easily broken — the opposite of magnetic. **[V]**

**Performance philosophy.** Ingest proxies + reduced playback resolution + Render In-Out previews (fast, selectable — the thing Resolve users envy), then Media Encoder queue for background export while editing. Good editorial responsiveness; export/render slow; Dynamic Link sometimes eats CPU/fails multi-project batches. Tolerable on low-end with proxies, stability drops. **[V+U]**

**Audio philosophy.** Essential Sound Panel = guided repair: tag Dialogue/Music/SFX/Ambience → one-click ducking, loudness, EQ presets. Enhance Speech (AI, cloud-assisted) rescues bad field audio **[V]**; pros call it "worthless with decent audio — use parametric EQ/multiband", artifacts on music **[U]**. Full mixing expects Audition. Approachable waveforms/sliders, not a DAW.

**Target user.** Working pros/teams locked into Adobe pipeline, agencies, template-heavy shops. Beginners can start; density is high.

**What PULSAR should learn.** Text-first editing + filler/silence removal + auto-captions are now table stakes; best-in-class transcription is a moat. One-key guided audio repair beats exposing a full mixer to beginners. Background export queue (edit while exporting) + true Render Selection/In-Out.

**What PULSAR should NOT copy.** Subscription + cancellation fee + mandatory cloud login; fragile autosave that can corrupt/lose queue work; slow export pipeline; unannounced ripple-behavior changes.

### 1.3 Final Cut Pro (11.x → 12.x, 2024–2026; Mac/iPad only, $299.99 once)

**Best thing it does.** Magnetic Timeline speed + background-everything + continuous save. Primary storyline with connected clips/storylines auto-ripples, no sync loss, no gaps; fastest multicam and batch/background export; lowest beginner fear. **[V+U]**

**Why that works.** The timeline expresses *intent* (this B-roll belongs to that story moment) instead of bare coordinates, so restructuring never desyncs. Combined with autosave-that-just-works, beginners feel safe and experts restructure fast. **[A]**

**Worst recurring friction.**
1. Audio mixing inadequate ("good at clip repair, for mixing send to ProTools/Audition/Fairlight"); Roles/Lanes instead of a mixer. Jordan + many pros. **[U]**
2. No native speech-to-text until FCP 11 (Nov-2024); auto-captions rated "pretty bad" vs Premiere; trackers "useless for complex tasks". **[U]**
3. Mac-only (+iPad), slow cadence ("fallen behind"), large-project crash/render-file-corruption threads; FCP7→X memory persists. **[V+U]**

**Timeline philosophy.** Magnetic, trackless. One primary storyline; everything else connects above/below; moving primary moves connected clips; ripple is default; gaps = Gap clips; storylines/compound clips for grouping; always-on-feeling snapping. Loved for assembly, hated by track-thinkers wanting free gaps/manual patching ("STOP turning off magnetic timeline" tutorials exist). Roles/Lanes + Auditions + Index for organization. **[V]**

**Performance philosophy.** Apple Silicon + background render + file segmentation across cores + proxy/optimized toggle + Compressor/CommandPost batch/background export while editing. Best low-end behavior *on modern Macs*; Intel/old or huge libraries still choke on render files. No Win/Linux. **[V+U]**

**Audio philosophy.** Roles-based, not track-based: assign Dialogue/Music/Effects (+ custom Roles/subroles); Lanes group by Role; stem/multitrack export by Role. Excellent for organization + stems without track juggling; weak for mixing (limited buses/plugins vs Logic/Fairlight). Workflow: repair in FCP, mix in Logic via FCPXML. **[V]**

**Target user.** Mac-only speed-first creators, YouTubers, wedding/event, solo editors. Easiest beginner→intermediate ramp; advanced color/audio/VFX hit ceiling → round-trip.

**What PULSAR should learn.** Magnetic/connected *default* with easy escape (gap/override) kills beginner sync pain #1. Continuous save + versioned library backups = reliability gold standard. Roles for organization + stem export > raw tracks for beginners. Background batch export while editing.

**What PULSAR should NOT copy.** Trackless-only dogma; anemic mixer; platform lock-in; slow AI iteration; hiding pro controls so deep they need third-party tools.

### 1.4 CapCut Desktop (2025–2026, ByteDance; freemium)

**Best thing it does.** Speed-to-publish for vertical short-form: trend templates updated in days, fast auto-captions, one-click background removal / noise cleanup, beat markers. Trim/speed/cleanup/chroma without tutorial. **[V+U]**

**Why that works.** It optimizes the loop trend→template→caption→beat-cut→publish, not the craft of finishing. Beginners get a *done* video in minutes. **[A]**

**Worst recurring friction.** Paywall creep + export ambush + billing distrust. Free ≈1080p; Pro $9.99/mo or ~$89.99/yr unlocks 4K/HDR, no watermark, premium FX/templates/AI. Users build full projects then discover transitions/resolution need Pro ("99% transitions Pro"); post-trial charges, bot support (Trustpilot/Reddit, r/NewTubers). **[V+U]**

**Timeline philosophy.** Magnetic-first, gap-averse, auto-ripple-delete by default. Magnet/snapping toggle, Q/W trim to cursor, Ctrl/Cmd+B split, B blade / A select, JKL + I/O. Up to 50 timelines per draft. Philosophy: never leave dead air; optimize music-synced cuts. **[V]**

**Performance philosophy.** Hide complexity: Settings → Performance → Proxy Size + preview quality + hardware-encode toggle. Official 4K-on-low-spec guide pushes proxies + lowered preview. Failure: laggy on weak WiFi/PC; 2.5-min 1080p taking 20 min reported; Mac "disable hardware-encode speedup" workaround; proxy cache eats disk silently. **[V+U]**

**Audio philosophy.** Visual-sync first, not mix-first. Waveform + AI beat detection, volume slider + drag-volume-line + keyframable fades, isolate/enhance, denoise, loudness normalize, TTS/voice-clone. No mixer depth. **[V]**

**Target user.** Solo TikTok/Reels/Shorts creator, faceless channels, SMB social managers.

**What PULSAR should learn.** Trend-template + auto-caption + beat-sync loop; proxy + preview-quality as ONE switch; Q/W ergonomics; transparent export defaults (match source fps/res).

**What PULSAR should NOT copy.** Export-time paywall reveal; shifting free/Pro boundary; watermark coercion; cloud-lock; opaque telemetry (XDA: tracks where/how/when; 30-page policy; 2025 US-ban scare pushed users to Kdenlive).

### 1.5 Filmora (v13/14, 2025–2026, Wondershare; freemium + "perpetual")

**Best thing it does.** Gentlest beginner→intermediate ramp with real depth on demand: magnetic timeline + Auto Ripple + Shift+Delete close-gap, plus first-class creator audio (silence detection, auto beat sync, ducking) and batch/multi-aspect export. 2.9M-asset library, customizable shortcuts incl. Premiere/FCP presets. **[V+U]**

**Why that works.** It keeps density low but never dead-ends: keyframe graphs, speed curves, multicam, smart scene cut appear as skill grows. **[A]**

**Worst recurring friction.** Triple monetization: subscription + diamond-paywalled assets + AI credits. Basic $49.99/yr, Advanced $59.99/yr, Premium $99.99/yr, "Perpetual" $79.99 Win / $109.99 Mac (one major version only); full library needs ~$19.99/mo add-on after 14-day trial; free export = watermark + AI credits. Reddit: "everything 💎 asks subscription separately". **[V+U]**

**Timeline philosophy.** FCP-style magnetic but forgiving. Magnet toggle, auto-ripple removes gaps, A/V linkage. Hold-key to suspend snap (like FCP). One 14.3.2 snapping-regression thread shows how sensitive this contract is. **[V+U]**

**Performance philosophy.** Explicit Performance tab: GPU accel toggles, preview-quality (preview-only, not export), >720p proxy workflow, clear-cache guidance. Min 8 GB / GTX1050Ti / UHD630 2 GB; rec 16 GB + 4 GB VRAM; 8K needs i9/Ryzen9 + 32 GB + RTX3000 8 GB + NVMe. Still 14.5 4K120 slowness reports on modest HW. **[V+U]**

**Audio philosophy.** Creator-audio done right: enlarged waveforms, envelope/keyframe volume, right-click Detect Beats (red markers every 4 beats), ducking, denoise, vocal remover, stretch, FLAC lossless export (v14.9). More mix-capable than CapCut, less than a DAW. **[V]**

**Target user.** Hobbyist/YouTuber/educator on mid-range Win/Mac wanting templates + AI without pro curve.

**What PULSAR should learn.** Magnetic + Auto Ripple + one-key close-gap; beat/silence/ducking as core (not credit extras); batch multi-aspect queue; shortcut presets; proxy + preview co-located in Performance.

**What PULSAR should NOT copy.** Diamond-everything + credit meter + perpetual-that-isn't; watermark-after-edit reveal; app-trial vs asset-trial confusion.

### 1.6 Kdenlive (24.x → 25.x/26.x in 2026; GPL, free forever)

**Best thing it does.** Pro-paradigm zero-cost workhorse: unlimited tracks, nested timelines, grouping, 3-point edit, FFmpeg-native mixed-format timeline (no transcode), proxy + preview-res combo making 4K usable on 8 GB/iGPU. No watermark/limits. Atomi 2026: 20-min 1080p+CC → ~4-min render. **[V+U]**

**Why that works.** It respects ownership and modest hardware: transparent proxy settings + format freedom + portable/no-login trust. **[A]**

**Worst recurring friction.** Windows/macOS stability + setup burden. Official Windows Issues page (kill processes, delete `kdenliverc`, clear proxy cache, driver updates). Reddit 2024–26: "crashes 2–15x on startup", "corrupt clip crashes", "ruined project after crash"; Flatpak-vs-native variance. Linux = stable; Windows = bumpy. UI functional-not-beautiful; no stock/template pack; beat-blind waveforms ("doesn't represent beat spikes"). **[V+U]**

**Timeline philosophy.** Premiere-like traditional NLE: manual control, optional magnetic snapping, explicit ripple, hierarchical tracks, customizable everything. Power + ownership over hand-holding. **[V]**

**Performance philosophy.** Transparent + manual: Project Settings → Proxy Clips (e.g. 540p) auto-generates lightweights, final renders from originals; Project/Clip Monitor preview-resolution; timeline preview renders; GPU/OpenGL + hw encode where host allows. 24.05 "huge speed boost + AV1 NVENC". Failure: 700 MB→5.6 GB transcode surprise if proxy settings wrong. **[V]**

**Audio philosophy.** Engineer-audio: mixer, balance, gain/volume keyframable (dB), scopes, keyframed volume/pan. No native beat detection; manual markers; waveform thumbnails slow/inaccurate for beat work. **[V+U]**

**Target user.** Linux educator/dev/indie/non-profit, freedom-valuing power users, mixed-source long-form.

**What PULSAR should learn.** Proxy + preview-res as first-run default for modest HW; mixed-format ingest with no convert; nested timelines; autosave + one-click cache purge; portable/no-login trust.

**What PULSAR should NOT copy.** Exposing MLT/FFmpeg sharp edges (transcode bloat, cryptic "clip invalid", manual rc-delete rituals); second-class Windows stability; beat-blind waveforms.

### 1.7 Shotcut (2025–2026, MLT, open-source, free no-catch)

**Best thing it does.** Honest free: no watermark/time-limit/paywall, cross-platform, wide formats, proxy + preview scaling that saves low-end, autosave, transparent Jobs queue. Easiest open-source on-ramp. **[V+U]**

**Why that works.** Two-lever performance (Preview Scaling 360p/540p + Proxy coupled to it, else 540p HEVC, Jobs queue, async Replace retaining trims/filters) + surfaced autosave path + explicit ripple/snap toggles with persistent state. **[V]**

**Worst recurring friction.** Dated UX + timeline quirks + export gotchas: snapping-fails thread; ripple confusion (Ctrl+R ripple, Ctrl+Alt+R ripple-all, Shift+I/O ripple trim); overlap-by-3-frames creates unintended transition; 28-track project → 19 GB RAM + shortcut freeze; choppy playback even with proxy+scaling; export fails at 0%/16% tied to hw encoder / HTML filters / Controlled Folder Access / antivirus blocking melt/ffmpeg (fix = disable hw encoder, disable parallel). Failure messages are technical (exit code 3) — transparent but intimidating. "5 hrs in… autosave failed to recover" shows limits. **[V+U]**

**Timeline philosophy.** Explicit-tool NLE: Ripple / Ripple-All / Snap toggles, Split (S), Lift/Delete/Insert/Append, I/O trim. Correctness over magnetism; user opts into ripple. Less fluid than magnetic, more predictable once learned. **[V]**

**Performance philosophy.** Best when preview res matches source; handles HD/2K/4K on weak CPU, but Size filter + many tracks still chokes. Waveform rebuild lag after cuts. **[V+U]**

**Audio philosophy.** Filter-based: drag waveform peak line for gain, mute/lock per track (Ctrl+M/L), loudness/scopes, high-perf waveform toggle. No beat detection. **[V]**

**Target user.** Beginner-budget YouTuber/educator on Win/Mac/Linux needing free-no-catch basics.

**What PULSAR should learn.** Coupled Preview-Scale + Proxy default; Jobs queue with logs (not spinner); autosave path surfaced in UI; explicit ripple/snap toggles; no-login trust.

**What PULSAR should NOT copy.** Overlap-creates-transition trap; snap off-by-one; hw-encoder-on-by-default export failures; log-as-user-message; silent 19 GB RAM with no limiter.

### 1.8 Optional editors (lighter depth)

**Olive (0.1 stable / 0.2 rewrite, GPL).** Only FOSS attempt at node-compositing + NLE timeline. 0.1 conventional tracks; 0.2 timeline clips backed by node graph. Lightweight Qt/C++ appeal where Resolve/Premiere won't run, but 0.2 nightly unstable (last code Sep-2023, rebuilt Dec-2024; site: "Olive will return"). No mature proxy/smart audio. Lesson: node-timeline ambition is real but abandonment risk is itself friction; PULSAR's "architecture before shortcuts" is the antidote. **[V+U]**

**Lightworks (v2025.1).** "Universal timeline" (mixed fps/res; project inherits first-clip specs), Log/Edit/VFX/Audio rooms, bins/sequences/subclips, 64 video tracks, strong keyboard-first with Premiere/FCP/Avid emulation. Proxy mode praised; PCMag 2025 lab: 5-min 7-clip render 117 s vs Resolve 27 s (bottom quartile) + one crash. Audio: shift-drag gain, mix panel, voiceover, Compressor/EQ/Limiter/Noise-Reducer/Reverb; noise reduction weak/distorting; no stock/duck/AI captions. Free = 720p-only; Create $13.99/mo / $199.99 perpetual (4K); Pro $27.99/mo / $419.99 perpetual; 2-machine lock. Falls behind on AI. Target: long-time film editors, Linux/perpetual seekers. **[V+U]**

**VEGAS Pro (21/22, Windows-only).** "Modeless" multitrack (edit/grade/audio/composite, no round-trip), nested timelines, 3D compositing. 21/22 adds AI Text-Based Editing, Auto-Reframe 16:9→9:16, Sharpen/Smoothen/Dehaze, Smart/Depth Mask, multicam audio sync, beat/tempo detection. Fast on mid-HW (8K RED plays at 1080p24; ProRes XQ export 29 s on Puget Mobile) but preview-stutter + heavy-FX crash vectors; recurring "crashes far too much / GPU accel = crash speedrun" threads v14–23. Perpetual Suite ~$249 still offered vs $108/yr. Target: solo Windows YouTubers/corporate/doc. **[V+U]**

**Avid Media Composer (2026.8).** Bin-locking film NLE, Shared Projects, NEXIS-optimized, OpenTimelineIO adapter, Titler+ trim-surviving keyframes, PhraseFindAI Multicam Auto Cut, transcript sharing, First tier now uncapped tracks/resolutions. Heavyweight — not modest-HW friendly; reliability on large team projects is the sell. $49/mo or $539.99/yr Ultimate, $1299 perpetual + $399/yr support; tier reshuffle confuses. Target: Hollywood/broadcast/post-houses. Lesson for PULSAR: bin-locking + transcript-sharing solve team/long-project pains PULSAR doesn't have yet — note, don't chase. **[V+U]**

---

## 2. Cross-competitor matrix (descriptive, no scores)

| Dimension | Resolve | Premiere | Final Cut | CapCut | Filmora | Kdenlive | Shotcut |
|---|---|---|---|---|---|---|---|
| Timeline UX | Track-based, Cut+Edit pages, per-track ripple, N-snap, B-blade; powerful but lock-state ripple confuses | Track-based, S-snap, Q/W ripple-trim, Shift+Del ripple-del, C-blade; flexible, sync easily broken | Magnetic primary + connected clips/storylines, ripple-by-default, Gap clips; fastest restructure, track-thinkers resist | Magnetic-first gap-averse auto-ripple-del, Q/W to cursor; never dead air; music-cut optimized | Magnetic + Auto Ripple + Shift+Del close-gap, hold-to-suspend-snap; forgiving | Premiere-like manual, optional snap, explicit ripple, nested timelines, 3-point; power over hand-holding | Explicit toggles Ripple/Ripple-All/Snap, S-split, I/O; predictable, less fluid |
| Audio | Fairlight DAW: buses, meters, Voice Isolation, Dialogue Match, AI Assistant; deep but Edit/Fairlight desync, no track lock | Essential Sound tags→presets, Enhance Speech rescue, Audition round-trip; approachable, phasey if overused | Roles/Lanes org + stem export; repair-good, mix-poor; Logic via FCPXML | Visual-sync: beat markers, drag-volume-line, isolate/enhance, TTS; no mixer | Creator-audio: big waveforms, envelope, beat detect, ducking, vocal remover, FLAC out; sweet spot for target | Engineer-audio: mixer, keyframable dB gain, scopes; beat-blind waveforms, manual markers | Filter-based gain-by-dragging-waveform, per-track mute/lock, scopes; rebuild lag |
| Waveforms | Editable, loudness meters; sub-frame de-pop hard (frame-lock off + deep zoom) | Always-on, Essential Sound sliders; approachable | Clean but secondary; Roles-first | Beat-oriented, immediate | Enlarged, beat-marked, envelope-editable; best creator readability in this set | Thumbnails slow/inaccurate for beats **[U]** | High-perf toggle; rebuild lag after cuts |
| Performance | GPU-first, manual proxies/optimized/cache; great tuned, punishing untuned | Proxies + reduced playback + Render In-Out; responsive edit, slow export | Apple Silicon + background render + segmentation; best on modern Mac, Mac-only | Proxy Size + preview quality + hw-encode toggle; one-switch simplicity, silent cache growth | Performance tab: GPU + preview + proxy co-located; clear RAM tiers; 4K120 still slow on modest HW | Proxy-540p + preview-res + timeline preview; 4K on 8 GB viable; transcode-setting trap | Preview Scaling + Proxy coupled (540p HEVC, Jobs queue); best documented FOSS low-end path |
| Proxies | Optimized Media + proxy res; manual, powerful, homework-heavy | Ingest proxies + toggle; full docs page needed; unlinked-file traps | Proxy/optimized toggle, near-invisible | Proxy Size setting; hidden cost = disk | >720p proxy workflow; co-located UI | Auto-generate 540p, render from originals; enable-by-default advice | Vertical-res coupling rule; async Replace keeps trims |
| Asset mgmt | Fusion/Fairlight depth, no audio templates; Deliver presets live-linked | Power Bins/search bins, MOGRTs, stock; clip-in-one-bin limits; 1000s-stills search breaks | Keyword/ratings/skimmer + Roles Index; fast find, thin stock | Huge trend templates/FX/stock/music, crown-gating unclear | Categorized Media/Music/Text/FX + search + favorites star; owned-vs-💎 mixing | FX library + favorites star, LUTs, transitions auto-fit; no stock/templates | Playlist/Bins only; no upsell but no velocity |
| Learning curve | Steep (pages/panels), pays off | High density, fast once learned, high switch cost | Easiest ramp, pro ceiling | Instant, intermediate push to Pro | Gentlest with depth-on-demand | Steep but rewarding; stick to Editing layout | Newbie-friendly vs Kdenlive;早 hits wall |
| Power-user speed | JKL, I/O, Q/W-equivalents, custom map, Speed Editor HW | Gold-standard JKL/Q/W/Shift-Del/C, remappable; fastest for converts | JKL, B/Cmd+B, Shift-Del, skimmer/keyword/multicam flow; fewer shortcuts needed | Q/W, Ctrl+B, +/-, custom panel; mouse/template-led | Full list + visual editor + Premiere/FCP presets; Ctrl+B, Shift-Del, Ctrl+Alt+D detach | Fully custom, 3-point, nested, param keyframes | Extensive + custom since 22.09, AHK macros common |
| AI | Magic Mask v2, Depth Map, Voice Isolation, Dialogue Match (useful); transcription 60–70%→90–95% clean (rough-cut ok, deliver needs cleanup); local/private | STT→captions + Text-Based Edit + filler removal (killer); Enhance Speech rescue; Generative Extend uneven | Magnetic Mask (useful, on-device), Transcribe-to-Captions (fast, accuracy criticized), Enhance Light/Color (starting point) | Auto-captions (fast, accent/jargon needs fix), Long→Shorts, TTS, bg-remove, isolate (useful); script-gen generic; premium voices/Pro-locked | Silence remover, auto-frame, beat sync, TTS/STT, denoise (useful); Copilot/text-edit/enhancer credit-burn; included-vs-credit split unclear | ~None (no credits = trust, no magic = gap); tracking unrefined | None |
| Export | Deliver presets + queue live-linked to timeline (mutates!); pixelated/oversized if codec wrong | Presets + Media Encoder background queue (edit while exporting); quality-toggle/cache-clear lore; fails on high-end PCs too | Share presets + Compressor batch/background while editing; fastest batch | TikTok/YT/IG presets, 4K/HDR Pro, bitrate/HDR toggles; no queue; opaque failures (fps mismatch, disk-full, hw-encode) | Best-in-target: Local/Device/Social, 4K/8K ProRes, per-task res/folder, Add-Task batch multi-aspect + Export All; generic failure text | Full MLT/FFmpeg profiles, hw optional, queue; codec/driver/corrupt-clip failures; stable-build warning | Presets (H.264/YouTube/audio-only/alpha/PNG) + Jobs + log; exit-code-3 intimidating but transparent |
| Reliability | Live Save risk; timed backups needed; cache-loss threads | Crash/autosave-loss 2024–26; interval bugs; corruption threads | Continuous save + library backups; safe force-quit; render-file corruption on huge libs | Stable; router-block after update; free-cloud exhaustion | Autosave 4-min default; greedy-update gripes > corruption | Recovery strong if saved once; corrupt file wedges startup; Linux > Windows | Autosave folder + manual recovery; "autosave failed" limits |
| Low-end suitability | Poor untuned; great tuned + dGPU | Tolerable with proxies; stability drops | Best on modern Mac; N/A elsewhere | One-switch proxies; weak-WiFi/PC lag; silent disk use | Tiered specs honest; 4K120 still slow | 4K on 8 GB/iGPU viable (proxy+preview); setup burden | HD/2K/4K on weak CPU viable; Size-filter/many-track cliff |

---

## 3. First 10 minutes (where obvious / confusing / assumed knowledge / click counts)

- **CapCut/Filmora:** obvious — import → drop → split (Ctrl+B) → Q/W trim → caption → export preset. Assumes phone-footage mental model. Common work = 2–4 clicks. Confusing only at export (paywall/watermark reveal). **[V+U]**
- **FCP:** obvious — magnetic assembly, skimmer, keyword/ratings; first cut/trim/navigation fast. Assumes Mac conventions. Confusing for track-thinkers (where are tracks?). Export via Share = 2 clicks + background. **[V+U]**
- **Premiere:** obvious if you know NLEs (I/O, Q/W, C-blade); confusing for true beginners (bins, sequences, work area, ingest settings, Essential Sound tags). First export requires codec literacy. **[V+U]**
- **Resolve:** confusing — pages (Media/Cut/Edit/Color/Fairlight/Deliver), project fps lock, cache toggles, Fusion titles. Assumes finishing knowledge. First export = Deliver-page literacy. **[V+U]**
- **Kdenlive/Shotcut:** obvious ingest (mixed formats, no transcode — Kdenlive's killer first-minute win); confusing in settings vocabulary (proxy vertical res, preview scaling, MLT sharp edges) and Windows setup rituals. Export = profile literacy + hw-encoder trap. **[V+U]**

**[A] PULSAR lesson:** first-10-minutes must be CapCut-obvious (import→cut→trim→caption→export-preset) while *preserving* Premiere/Resolve keyboard contracts (JKL, I/O, Q/W, Shift+Delete, B/S split) underneath. Never gate the first export behind a surprise.

---

## 4. Best-in-class patterns (14)

1. **Problem:** B-roll loses sync with main edit. **Approach:** Connected-clip relationship (FCP primary storyline + connected clips/storylines; Filmora/CapCut magnetic echoes). **Why it works:** Relationship expresses intent, not coordinates; restructuring preserves sync by construction. **PULSAR lesson:** default connected/attached behavior for B-roll/music with one-key escape to free placement; never rely purely on coordinates. **[V+U+A]**
2. **Problem:** Beginners fear breaking the timeline. **Approach:** Ripple-by-default + Gap clips + auto-ripple-delete (FCP/CapCut/Filmora). **Why:** No dead air, no silent desync; delete always closes. **Lesson:** gap-free default; explicit "leave gap" action, not accidental gaps. **[V+A]**
3. **Problem:** Interviews/podcasts take hours to rough-cut. **Approach:** Accurate STT → Text-Based Editing + filler/silence removal (Premiere killer; VEGAS/Filmora/CapCut echoes). **Why:** Editing prose is faster than scrubbing waveforms for speech. **Lesson:** local-first transcription → text edit + captions is the highest-ROI future AI; must work offline. **[V+U+A]**
4. **Problem:** Bad field audio blocks beginners. **Approach:** Guided repair: tag → preset (Premiere Essential Sound; Resolve AI Assistant; FCP Enhance). **Why:** One click beats a mixer for 80% of cases. **Lesson:** "Fix my dialogue" button (duck/denoise/level/loudness) in front of full mixer. **[V+A]**
5. **Problem:** Finding stems without track juggling. **Approach:** Roles/Lanes organization + stem export (FCP). **Why:** Semantic groups survive restructuring better than track numbers. **Lesson:** roles/tags as first-class citizens alongside tracks; export stems by role. **[V+A]**
6. **Problem:** Preview too slow on modest HW. **Approach:** Coupled Preview-Scale + Proxy default (Shotcut scaling+proxy coupling; Kdenlive 540p default advice; Filmora co-located Performance tab). **Why:** One switch, predictable size, async replace keeps trims. **Lesson:** ship 540p-coupled default on first import; never require manual "Chapter 8". **[V+A]**
7. **Problem:** Waiting on export blocks editing. **Approach:** Background/batch export queue while editing (Premiere+Media Encoder; FCP+Compressor; Filmora multi-aspect batch; Shotcut Jobs). **Why:** Throughput = edit during render. **Lesson:** future queue must be *decoupled* from timeline (unlike Resolve live-link) + multi-aspect batch. **[V+U+A]**
8. **Problem:** Knowing what render actually previews. **Approach:** Render Selection / Render In-Out (Premiere; Resolve users explicitly beg for it). **Why:** Bounded, visible, re-doable preview contract. **Lesson:** In-Out preview with explicit state beats clip-cache magic. **[V+U+A]**
9. **Problem:** Crash loses work. **Approach:** Continuous save + versioned backups (FCP gold standard; Resolve timed backups; Kdenlive/Shotcut autosave folders). **Why:** Never overwrite sole copy on crash; force-quit is safe. **Lesson:** continuous + versioned + surfaced recovery path. **[V+U+A]**
10. **Problem:** Beat-syncing music cuts by eye. **Approach:** One-click beat detection + markers + snap-to-beat (Filmora right-click Detect Beats; CapCut AI markers; VEGAS tempo). **Why:** Removes manual waveform-guessing. **Lesson:** beat markers as core audio (not AI-credit extra) + accurate waveforms. **[V+A]**
11. **Problem:** Music drowns dialogue. **Approach:** Auto-ducking (Premiere/Filmora). **Why:** Sidechain-by-default expresses intent ("dialogue first"). **Lesson:** one-click duck with visible envelope. **[V+A]**
12. **Problem:** Asset paralysis (1000s of FX/sounds). **Approach:** Search + favorites star + categorized browser + shortcut-preset switching (Filmora Ctrl+1–8 + star; Kdenlive star; Premiere/FCP keyword). **Why:** Finding beats quantity. **Lesson:** PULSAR Sounds 191-SFX needs search + favorites + audition-in-place, not just a folder. **[V+A]**
13. **Problem:** Shortcut relearning blocks switching. **Approach:** Remappable everything + Premiere/FCP preset packs + visual editor (Filmora/Premiere/Kdenlive/Shotcut; Lightworks emulation). **Why:** Muscle memory ports in minutes. **Lesson:** ship Premiere + FCP preset maps + Q/W/Shift-Del/JKL contracts from day one. **[V+A]**
14. **Problem:** Loudness roulette on YouTube. **Approach:** Loudness meters + one-click normalize (Resolve/Fairlight "YouTube perfect"; Premiere loudness; CapCut normalize). **Why:** Platform target as preset, not tribal knowledge. **Lesson:** loudness preset + meter visible in audio path. **[V+A]**

---

## 5. Recurring industry failures (13 — only with evidence)

1. **Proxy friction.** Ingest+toggle+relink confuses; wrong preset = "expensive mess later", unlinked files, grade/crop surprises. Constant ELI5-proxy threads (r/VideoEditing); EOSHD "messier workflow". **[U+V]**
2. **Cache bloat / no cap.** Resolve "almost 900 GB", "storage goes down while editing"; Premiere "100GB+ RAM/cache" manual-delete lore; CapCut silent proxy-disk use; TikTok/IG "find the cache folder" genre. No auto-cap by default. **[U]**
3. **Timeline desync.** Linked-selection toggles break sync; nudge desyncs; VFR phone footage drifts (needs CFR transcode / slip-by-frames). Blackmagic "Huge issue with linked video+audio"; r/premiere drift threads. **[U+V]**
4. **UI overload vs simplistic trap.** Premiere/Resolve density overwhelms; CapCut/FCP simplicity hits pro ceiling → round-trip. No studied editor grows smoothly; Filmora comes closest but paywalls depth. **[U+A]**
5. **Hidden settings.** Ripple/snapping/hw-encode/cache states buried or regress silently (Filmora 14.3.2 snap regression; Premiere dynamic-ripple controversy; Shotcut hw-encoder default failures). **[U+V]**
6. **Crashes + corrupt projects.** Premiere autosave-loss 2024–26; VEGAS "GPU accel = crash speedrun"; Kdenlive Windows wedges; Resolve Live-Save corruption risk; Shotcut "autosave failed to recover". **[U]**
7. **Slow save / slow open on large projects.** Resolve bloated DB/Media Pool paste-between-timelines slowness, slow open; Premiere 1000–5000-source playback collapse; 2-hr 4K60 multicam renders take hours. **[U]**
8. **Memory growth / no limiter.** Shotcut 28-track → 19 GB RAM + shortcut freeze with no warning; Premiere 100 GB+ reports. **[U]**
9. **Asset discovery.** Clip-in-one-bin limits; 1000s-stills search breaks; Avid match-frame/find-bin failures; no auto-tagging without AI; owned-vs-locked search mixing (Filmora). **[U+V]**
10. **Subscription/cloud/login friction.** Adobe fee + cancellation-fee + CC-login; CapCut login + shifting Pro boundary; Filmora triple-monetization; Lightworks machine-lock; Avid tier reshuffle. Churn driver #1 for freemium. **[U+V]**
11. **Export complexity / opaque failures.** "Why is exporting SO difficult" (Premiere, high-end PCs too); Resolve pixelated/oversized on wrong codec; Shotcut exit-code-3; CapCut fps-mismatch artifacts; Filmora generic "update drivers, clear cache". Endless best-settings tutorials prove defaults fail. **[U]**
12. **Waveform inaccuracy.** Kdenlive beat-blind thumbnails; Shotcut rebuild lag; Resolve sub-frame editing hard; CapCut/Filmora beat-accurate but shallow. Beat work by eye still unreliable in FOSS. **[U+V]**
13. **AI credit/paywall + privacy.** Filmora/CapCut included-vs-credit split unclear; Premiere cloud STT/Enhance privacy concern vs Resolve/FCP on-device; ByteDance telemetry; Avid/VEGAS AI upsell bundles. **[U+V]**

---

## 6. "Why has nobody fixed this?" (highest-value section)

1. **Proxies are still homework.** *Who:* everyone on modest HW. *Why weak:* ingest-vs-toggle-vs-relink split across settings; coupling rules (Shotcut vertical-res) tribal; wrong choice compounds at export. *Why hard:* proxy correctness spans decoder, timeline, effects, export (frame-accurate relink, grade/crop invariance, hw variance). *PULSAR opportunity:* coupled Preview+Proxy single default (540p HEVC) on first import + visible state + async replace keeping trims; render always from originals. *Risk:* background proxy generation + storage accounting is real engineering; needs bounded cache + eviction (aligns with budgets.md). **[A]**
2. **Caches grow unbounded and invisibly.** *Who:* all long-project users. *Why weak:* defaults uncompressed/uncapped; purge is manual lore. *Why hard:* cache spans preview/proxy/waveform/peak/thumbnail; eviction must never break edit truth. *Opportunity:* PULSAR differentiator — every cache with limit + eviction + observability (already in budgets.md); one-click inspector + auto-cap. *Risk:* needs disciplined size accounting from day one. **[A]**
3. **A/V sync is coordinate-fragile.** *Who:* beginners + VFR-phone editors + long-form. *Why weak:* linked-selection toggles, per-track ripple, VFR drift. *Why hard:* VFR→CFR normalization + sample-accurate audio + explicit relationship model. *Opportunity:* connected/attached default + VFR detect-and-normalize on import + visible link state. *Risk:* relationship model must not become trackless dogma; keep escape hatch. **[A]**
4. **Waveforms lie about beats.** *Who:* music-cut creators. *Why weak:* peak thumbnails downsample away transients; rebuild lags. *Why hard:* accurate peaks need background analysis + zoom-aware resolution without blocking UI (UI-thread ban in budgets.md). *Opportunity:* A8 waveform correctness + beat markers as core; zoom-aware peaks computed off-UI-thread with bounded cache. *Risk:* analysis cost on patata PCs; degrade gracefully (Lite mode). **[A]**
5. **Asset findability plateaus at scale.** *Who:* anyone with 1000s of clips/FX/SFX. *Why weak:* single-bin hierarchy, search mixing owned/locked, no favorites-first flow. *Why hard:* tagging + indexing + audition latency. *Opportunity:* PULSAR Sounds wins via search + favorites + audition-in-place + keymapped insert; smart collections later. *Risk:* over-building DAM before editor works. **[A]**
6. **Export defaults fail silently.** *Who:* beginners → pros switching codecs. *Why weak:* presets don't match source (fps/res), hw-encoder variance, live-linked queues. *Why hard:* matrix of codecs/containers/hw × platform drivers. *Opportunity:* match-source default + preflight (disk/fps/hw-fallback) + plain-language errors + log expander + decoupled queue. *Risk:* hw-encode matrix testing burden. **[A]**
7. **Large projects degrade non-linearly.** *Who:* podcast/long-record/multi-hour editors. *Why weak:* Media Pool/DB bloat, thumbnail storms, undo bloat. *Why hard:* requires virtualized timeline + lazy thumbnails + bounded undo — architectural, not a patch. *Opportunity:* PULSAR low-memory design + virtualized timeline from the start; never require 32 GB for basics. *Risk:* long-horizon work; instrument early. **[A]**
8. **Autosave can destroy as well as rescue.** *Who:* everyone (Premiere loss threads; Live-Save corruption). *Why weak:* overwriting sole copy, ambiguous versioning. *Why hard:* crash-consistent versioning + cheap snapshots. *Opportunity:* continuous + versioned + never-overwrite-sole-copy + surfaced recovery. *Risk:* storage cost; needs retention policy. **[A]**
9. **Guided vs pro UI never graduates.** *Who:* beginner→expert transition. *Why weak:* either permanent simplicity (CapCut ceiling) or permanent density (Premiere/Resolve). *Why hard:* progressive disclosure without mode confusion. *Opportunity:* "simple by default, powerful by choice" as real mechanism: inspector/timeline controls reveal on selection/skill, shortcuts always available. *Risk:* design discipline; avoid Clippy-style AI. **[A]**

Be skeptical note: "nobody does this" was checked per item — e.g. Render In-Out exists in Premiere (so Resolve's gap is real, not universal); coupled proxy defaults exist in Shotcut (so the gap is *defaults/UX*, not invention); Roles exist in FCP (so semantic grouping is proven, not novel). Opportunities above are integration + defaults + correctness plays, not inventions. **[A]**

---

## 7. Performance / low-end deep dive (Auto/Lite/Balanced/Pro relevance)

- **Proxy support ≠ good low-end UX.** All seven core editors *support* proxies; only Shotcut/Kdenlive/Filmora/CapCut make them *one-switch*. Resolve/Premiere require manual study. PULSAR must ship the switch, not just the capability. **[V+U+A]**
- **Preview scaling is the unsung hero.** Shotcut Preview Scaling + Kdenlive preview-res + Filmora preview-quality + CapCut preview-quality all beat raw proxying for interactivity-per-watt. PULSAR Lite should couple preview-scale + proxy + waveform-resolution as ONE mode. **[V+A]**
- **Background work must never block editing.** FCP background render/segmentation, Premiere+ME queue, Filmora batch, Shotcut Jobs all preserve interactivity; Resolve foreground caching doesn't. PULSAR budgets.md already bans heavy work on UI thread — enforce withProxy/waveform/export off-thread + playback priority. **[V+A]**
- **Honest tiers win trust.** Filmora publishes min/rec/8K specs; PULSAR budgets.md (2 s startup, <400 MB idle, 1–1.5 GB simple-1080p, Chill ~700 MB proxy editing, 8 GB supported / 16 GB recommended) is already more honest than most competitors. Keep publishing. **[V+A]**
- **Can PULSAR credibly claim "runs well on modest hardware without sacrificing edit correctness"?** Yes — *if* scoped: (a) coupled proxy+preview defaults, (b) bounded caches with eviction + observability, (c) UI-thread ban enforced, (d) render-from-originals, (e) VFR normalization, (f) decoupled background export later. No studied competitor does all six with sane defaults; Kdenlive/Shotcut prove the low-end is reachable in FOSS, FCP proves background-everything feel, Resolve proves what happens without defaults. The claim must be *measured* (startup/RAM/timeline-responsiveness budgets), never marketing. **[A]**

---

## 8. Timeline workflow lessons

- Default connected/gap-free with visible snap/ripple state; one-key escape to free-track mode (synthesis of FCP + Shotcut explicit toggles). **[A]**
- Preserve industry contracts: JKL, I/O, Q/W ripple-trim-to-playhead, Shift+Delete ripple-delete, B (or S) blade/split, +/- zoom, customizable + Premiere/FCP preset maps. Small friction × 500 reps/session dominates flashy features. **[V+A]**
- Ripple semantics must be explicit and undoable; never change ripple behavior without opt-in (Premiere 2026 lesson). **[U+A]**
- Gaps must be intentional objects (Gap clips), never accidents. **[A]**
- Nested timelines/compound/storylines for long projects (Kdenlive/FCP/Resolve); 3-point edit for source-driven cutting (Kdenlive). **[V+A]**
- VFR phone footage: detect + normalize to CFR on import or drift is guaranteed. **[U+A]**

## 9. Audio lessons (for M10 + A8)

- Waveforms always-on, zoom-aware, accurate; sub-frame editing easy (Resolve's frame-lock trap is the anti-pattern). High-perf toggle for patata PCs. **[A]**
- Beat detection + markers + snap-to-beat as core, not credit extra (Filmora/CapCut prove demand; Kdenlive/Shotcut prove the gap). **[A]**
- Guided repair first ("Fix dialogue": denoise/isolate/level/duck/loudness), mixer second (Essential Sound + Fairlight Assistant pattern). **[A]**
- Volume envelope on clip + mixer + mute/link/detach states visible; linkage state must be *visible* to prevent accidental desync. **[A]**
- Loudness preset + meter ("YouTube perfect") in the audio path. **[A]**
- Roles/tags alongside tracks; stem export by role (FCP lesson). **[A]**
- PULSAR Sounds 191-SFX: search + favorites + audition-in-place + keymapped insert; never a raw folder. **[A]**

## 10. Asset discoverability lessons

- Finding beats quantity: search + favorites star + categorized browser + audition-in-place (Filmora/Kdenlive pattern). **[V+A]**
- Badge locked/Pro assets *inline before editing*, never at export (CapCut/Filmora anti-pattern). **[U+A]**
- Keyword/ratings + smart collections for scale; single-bin hierarchy doesn't survive 1000s of assets. **[V+A]**
- No-login, offline-first library trust (Kdenlive/Shotcut lesson). **[A]**

## 11. AI: useful vs distracting

- **Useful (copy the job, not the hype):** accurate STT → text edit + captions + filler/silence removal; voice isolate/enhance as rescue; auto-levels/loudness; beat/silence detect; subject mask/isolate; auto-reframe as starting point. Evidence: Premiere text-edit killer; Resolve mask/isolate praised; Filmora/CapCut silence/beat/TTS used daily; FCP on-device mask respected. **[U+V]**
- **Distracting (avoid):** generic script generators; credit-metered basics; cloud-mandatory transcribe for private material; generative fill with uneven quality; AI that interrupts flow or gates export. **[U+A]**
- **PULSAR stance (validated):** optional, offline-capable, never required for core edit; local transcription option for privacy (Resolve/FCP on-device pattern beats Premiere cloud pattern on trust). **[A]**

## 12. Contradictions / tradeoffs (no universal winner)

- **Magnetic vs tracks:** fast restructuring + no desync vs explicit spatial control for track-thinkers. Resolution: magnetic default + free-mode escape, not dogma. **[A]**
- **Density vs approachability:** power-visible overwhelms; power-hidden ceilings. Resolution: progressive disclosure + shortcut presets. **[A]**
- **Auto vs manual performance:** invisible proxies delight until they surprise at export (transcode bloat, grade shift). Resolution: visible state + render-from-originals. **[A]**
- **Cloud AI accuracy vs privacy:** Premiere cloud STT most accurate; Resolve/FCP on-device private but weaker. Resolution: local-first with optional cloud upgrade, explicit consent. **[A]**
- **Background export vs correctness:** editing during export delights (FCP/ME) but live-linked queues corrupt intent (Resolve). Resolution: decoupled snapshot queue. **[A]**
- **Template velocity vs trust:** CapCut/Filmora templates accelerate but paywall-shift burns trust. Resolution: badge early, honor perpetual boundaries. **[A]**

---

## 13. PULSAR opportunity map

Grounded in current architecture (domain ← application ← adapters/UI; budgets.md limits; headless playback foundation + FFmpeg adapters + Qt Quick viewer; A8 waveforms, M10 audio in flight). No frozen-architecture rewrites proposed.

### NOW (useful during current alpha)

1. Adopt keyboard contracts now: JKL, I/O, Q/W, Shift+Delete, blade/split, +/- zoom + remappable + future Premiere/FCP presets — cheap in domain/application, expensive to retrofit. **[A]**
2. Waveform correctness principles (A8): zoom-aware accurate peaks off UI thread, always-on, high-perf degrade path; sub-frame editing must stay easy. **[A]**
3. Explicit snap/ripple/link states in domain model (visible, toggleable, undoable); gaps as intentional objects. **[A]**
4. Cache discipline from day one: every cache (thumbnail/peak/proxy) gets limit + eviction + observability per budgets.md; one-click purge; silence the future "900 GB cache" thread before it exists. **[A]**
5. Export honesty early: match-source defaults + plain-language errors + log expander; never watermark/paywall-ambush the first export. **[A]**
6. PULSAR Sounds integration: search + favorites + audition-in-place + keymapped insert (finding > quantity). **[A]**
7. Continuous + versioned save (never overwrite sole copy); surfaced recovery path. **[A]**

### NEXT (valuable after basic editor works)

1. Coupled Preview-Scale + Proxy single default (540p HEVC-style) on first import + visible state + async replace keeping trims; render from originals. **[A]**
2. Render Selection / Render In-Out bounded preview. **[A]**
3. Guided audio repair ("Fix dialogue": denoise/level/duck/loudness preset + meter) in front of mixer; beat detect + markers + snap-to-beat; auto-ducking with visible envelope. **[A]**
4. Roles/tags alongside tracks + stem export by role. **[A]**
5. Batch/multi-aspect export + decoupled background queue (edit while exporting; snapshot semantics). **[A]**
6. VFR detect-and-normalize on import; preflight (disk/fps/hw-fallback). **[A]**
7. Local-first transcription → text edit + captions + silence/filler removal (highest-ROI AI). **[A]**

### LATER (major differentiating features)

1. Adaptive performance modes (Auto/Lite/Balanced/Pro) with editing-truth invariance: Lite couples preview/proxy/waveform-res/effects; Auto senses HW; Pro unlocks full quality — *truth never changes with mode*. **[A]**
2. Virtualized timeline + lazy thumbnails + bounded undo for hundreds-of-clips / multi-hour projects. **[A]**
3. Smart collections + auto-tagging (people/shaky/silence) with local-first privacy. **[A]**
4. On-device subject mask/isolate + auto-reframe as starting points (FCP/Resolve pattern, offline). **[A]**
5. PULSAR Chill (~700 MB proxy editing) as measured, published target — not slogan. **[A]**

### AVOID (tempting but poor strategic fit)

1. Cloning any single timeline (FCP magnetic-only dogma; Premiere manual-only; Resolve page maze). **[A]**
2. Subscription/cloud-login/watermark/credit-metered basics; shifting free/Pro boundary; export-time reveals. **[A]**
3. Forcing simple titles through node compositor (Resolve/Fusion lesson); forcing mix through full DAW. **[A]**
4. Live-linked render queue; unbounded/capped-invisible caches; hw-encoder-on-by-default. **[A]**
5. Gimmicky generative AI before transcription/captions/filler/isolate/beat jobs are excellent. **[A]**
6. Template-stock quantity race without findability + licensing clarity. **[A]**
7. Chasing Avid-style team/bin-locking or Hollywood interchange before solo-editor correctness. **[A]**

---

## 14. PULSAR product principles (evidence-derived, 11)

1. **Editing truth must never change with performance mode.** Lite/Pro alter preview cost, never timeline semantics, sync, or export source. (From proxy/export-surprise + adaptive-mode evidence.)
2. **Common actions should be obvious in the first 10 minutes.** Import → cut → trim → caption → export-preset without tutorial or paywall surprise. (CapCut/Filmora/FCP.)
3. **Advanced power should be discoverable, not permanently visible.** Progressive disclosure + shortcut presets; density on demand. (Premiere/Resolve overload vs CapCut ceiling.)
4. **Derived caches must never endanger projects.** Bounded, evictable, observable, purgeable; render/export always from originals; purge never loses edits. (Cache-bloat + transcode-trap evidence.)
5. **No silent desync, ever.** Connected default, visible link/snap/ripple states, VFR normalization, gaps intentional. (Desync + VFR-drift evidence.)
6. **Waveforms must tell the truth.** Zoom-aware accurate peaks; beat-visible; rebuild never blocks editing. (A8; beat-blind + rebuild-lag evidence.)
7. **Fix-first audio, mixer-second.** One-click dialogue rescue + loudness preset before full mixer; stems by role. (Essential Sound/Fairlight/Roles evidence.)
8. **Small friction × 500 reps beats flashy features.** Honor JKL/Q/W/Shift-Del/blade contracts; remappable; batch what repeats (captions, silence, ducking, multi-aspect). (Power-user evidence.)
9. **The editor works FOR the user, offline, without permission.** No login for core edit; no watermark/coercion; no cloud-required basics; telemetry opt-in; small download; portable trust. (Business-friction evidence.)
10. **Failures must explain themselves in plain language.** Preflight + match-source defaults + actionable export errors + log expander; autosave versions + surfaced recovery. (Export/reliability evidence.)
11. **AI is optional assistance, never a toll or interruption.** Local-first where possible; credit-free basics; flow-preserving; core edit fully functional without it. (AI evidence.)

---

## 15. Sources

~95 URLs consulted across official docs, manuals, release notes, forums, Reddit, reviews. Grouped by editor in §1 + cross-cutting pains in §5–6. Full list below for auditability.

**DaVinci Resolve**
- https://www.blackmagicdesign.com/products/davinciresolve
- https://www.blackmagicdesign.com/products/davinciresolve/whatsnew
- https://video.stackexchange.com/questions/37276/does-davinci-resolve-have-a-magnetic-timeline
- https://www.reddit.com/r/davinciresolve/comments/1nypf2z/davinci_resolves_ripple_edit_logic_feels/
- https://forum.blackmagicdesign.com/viewtopic.php?t=206005&p=1073602
- https://forum.blackmagicdesign.com/viewtopic.php?f=21&t=145710
- https://forum.blackmagicdesign.com/viewtopic.php?f=21&t=175845
- https://forum.blackmagicdesign.com/viewtopic.php?f=21&t=175323
- https://forum.blackmagicdesign.com/viewtopic.php?f=21&t=157645
- https://forum.blackmagicdesign.com/viewtopic.php?f=33&t=96563
- https://forum.blackmagicdesign.com/viewtopic.php?f=21&t=66285
- https://larryjordan.com/articles/comparing-editing-in-davinci-resolve-to-apple-final-cut-pro-and-adobe-premiere-pro/
- https://www.keycodemedia.com/whats-new-in-davinci-resolve-20/
- https://www.kunalganglani.com/blog/davinci-resolve-21-ai-features-review
- https://davinciresolve21.com/blog/davinci-resolve-slow-to-open-large-projects-fix
- https://www.reddit.com/r/davinciresolve/comments/1kbyt4g/what_is_your_opinion_of_the_ai_tools_in_davinci/
- https://www.reddit.com/r/davinciresolve/comments/1r1s5as/davinci_autosave_behavior_while_crashing_and/
- https://www.reddit.com/r/davinciresolve/comments/xddegh/does_resolve_have_anything_comparable_to/
- https://www.reddit.com/r/davinciresolve/comments/1hpfjhk/when_im_editing_in_davinchi_resolve_my_storage/
- https://www.reddit.com/r/davinciresolve/comments/10jgfg4/extremely_long_time_for_a_video_export_in_davinci/

**Premiere Pro**
- https://helpx.adobe.com/premiere/desktop/edit-projects/change-clip-sequence/snap-clips.html
- https://helpx.adobe.com/premiere/desktop/get-started/keyboard-shortcuts/default-keyboard-shortcuts.html
- https://helpx.adobe.com/premiere/desktop/add-audio-effects/adjust-volume-and-levels/enhance-speech.html
- https://helpx.adobe.com/premiere/desktop/organize-media/ingest-proxy-workflow/ingest-and-proxy-workflow.html
- https://helpx.adobe.com/premiere/desktop/organize-media/file-organization/bins-overview.html
- https://helpx.adobe.com/media-encoder/desktop/encoding-and-exporting/encode-export-video-audio.html
- https://helpx.adobe.com/account/individual/subscriptions-and-plans/renewals-and-cancellations/cancel-adobe-subscription.html
- https://research.adobe.com/news/new-ai-features-make-audio-editing-easier/
- https://dev.larryjordan.com/articles/premiere-pro-cc-create-and-use-search-bins/
- https://community.adobe.com/questions-729/premiere-pro-2025-crash-deleted-all-project-files-and-auto-saves-1419848
- https://community.adobe.com/bug-reports-728/auto-save-causing-premiere-pro-2025-crash-1332105
- https://community.adobe.com/questions-729/playback-slows-down-on-a-project-with-many-files-1413155
- https://community.adobe.com/questions-729/poor-quality-video-export-despite-using-high-quality-settings-1387667
- https://www.reddit.com/r/premiere/comments/1r2675i/help_i_lost_hours_of_work_because_premier_crashed/
- https://www.reddit.com/r/premiere/comments/1qv6kpi/the_ripple_edit_action_sucks_now/
- https://www.reddit.com/r/premiere/comments/1df5cmm/enhance_speech_in_premiere_makes_no_sense_to_me/
- https://www.reddit.com/r/premiere/comments/1moioeq/thoughts_about_the_future_of_premiere_pro/
- https://www.reddit.com/r/premiere/comments/1r89e87/premiere_pro_2026_hate_it_or_love_it/
- https://www.reddit.com/r/premiere/comments/1l0u9mt/why_is_exporting_in_premiere_so_difficult/
- https://www.reddit.com/r/premiere/comments/1950lg4/has_anyone_figured_out_a_solution_to_the_your/
- https://www.reddit.com/r/premiere/comments/1s1mpj5/what_is_this_dragging_linked_videoaudio_no_longer/
- https://www.reddit.com/r/premiere/comments/1go2ae1/how_to_fix_audio_drift_over_time_in_premiere_pro/
- https://www.reddit.com/r/VideoEditing/comments/zltpsu/can_someone_explain_proxies_to_me_like_i_was_an/
- https://www.eoshd.com/comments/topic/28454-which-are-pros-and-cons-of-proxy-video-editing-color-correction-color-grade-and-crop/
- https://store.hollyland.com/blogs/creator-hub/fix-premiere-pro-audio-not-synced-with-video

**Final Cut Pro**
- https://support.apple.com/guide/final-cut-pro/intro-to-the-magnetic-timeline-verb8fcfc133/mac
- https://support.apple.com/guide/final-cut-pro/save-and-back-up-projects-ver79aa3d71/mac
- https://support.apple.com/guide/final-cut-pro/export-roles-verb71cb7e5/mac
- https://support.apple.com/guide/final-cut-pro/share-using-compressor-ver1ff89071/mac
- https://support.apple.com/guide/final-cut-pro/use-auto-mask-or-magnetic-mask-ver574fc632c/mac
- https://www.apple.com/newsroom/2024/11/final-cut-pro-11-begins-a-new-chapter-for-video-editing-on-mac/
- https://larryjordan.com/articles/final-cut-pro-x-10-4-4-batch-export/
- https://larryjordan.com/articles/final-cut-pro-x-roles-role-components-and-audio-stems/
- https://www.newsshooter.com/2024/11/13/final-cut-pro-11-features-magnetic-mask-transcribe-to-captions-and-more/
- https://www.cined.com/final-cut-pro-11-introduced-magnetic-mask-transcribe-to-captions-more/
- https://www.reddit.com/r/finalcutpro/comments/1ox5k7p/what_do_you_think_of_final_cut_in_the_current/
- https://www.reddit.com/r/finalcutpro/comments/1lbg5ot/buying_fcp_in_2025/
- https://www.reddit.com/r/editors/comments/1jr2lov/is_final_cut_pro_seen_as_a_inferior_software_used/
- https://www.reddit.com/r/editors/comments/1fgni5g/which_is_better_premier_prodavincifinal_cut/
- https://www.reddit.com/r/Filmmakers/comments/1t228m6/i_am_so_done_with_adobe_do_i_switch_to_da_vinci/

**CapCut**
- https://www.capcut.com/resource/capcut-standard-vs-pro
- https://www.capcut.com/create/4k-video-editing-low-spec-computer
- https://bigvu.tv/blog/capcut-desktop-review-2026-features-pricing-smart-alternatives
- https://www.agencyhandy.com/informative/capcut-pricing
- https://agilityportal.io/blog/create-viral-shorts-with-this-ultimate-guide-to-syncing-music-trends-in-capcut-pc
- https://www.skillademia.com/shortcuts/capcut-shortcuts
- https://www.reddit.com/r/NewTubers/comments/1uq0ltr/capcut_keeps_moving_features_behind_the_paywall
- https://www.reddit.com/r/CapCut/comments/1nyvjqj/capcut_is_no_longer_free
- https://www.reddit.com/r/CapCut/comments/1dsk8qj/capcut_on_pc_is_slow_and_laggy
- https://www.reddit.com/r/CapCut/comments/1nuybj9/has_anyone_else_been_having_major_issues_with
- https://www.xda-developers.com/why-capcut-privacy-policy-is-scary
- https://www.reddit.com/r/privacy/comments/wtl0m3/is_capcut_pc_safe

**Filmora**
- https://filmora.wondershare.com/tech-spec.html
- https://filmora.wondershare.com/whats-new-in-filmora-video-editor.html
- https://filmora.wondershare.com/advanced-video-editing/filmora-magnetic-timeline.html
- https://filmora.wondershare.com/basic-video-editing/auto-ripple-filmora.html
- https://filmora.wondershare.com/ai-efficiency/filmora-beat-detection.html
- https://filmora.wondershare.com/tutorial/preferences-for-better-performance.html
- https://filmora.wondershare.com/basic-video-editing/export-video-premiere-pro.html
- https://unifab.ai/resource/filmora-review
- https://www.reddit.com/r/Filmora/comments/1hsb37e/why_filmora_links_everything_to_subscription
- https://www.reddit.com/r/Filmora/comments/1kw7sh5/filmora_145_slowness
- https://www.reddit.com/r/Filmora/comments/1ixhah7/the_new_update_sucks

**Kdenlive**
- https://kdenlive.org/download
- https://kdenlive.org/news/releases/24.05.0
- https://docs.kdenlive.org/en/troubleshooting/windows_issues.html
- https://docs.kdenlive.org/en/effects_and_filters/audio_effects/volume_and_dynamics/volume_keyframable.html
- https://atomisystems.com/screencasting/kdenlive-review-2026-honest-expert-verdict-for-serious-creators
- https://news.ycombinator.com/item?id=47815118
- https://www.reddit.com/r/kdenlive/comments/1ekesb4/crashes_all_of_the_time
- https://www.reddit.com/r/kdenlive/comments/1ql9ixp/unfortunately_ive_decided_to_stop_using_kdenlive
- https://www.reddit.com/r/kdenlive/comments/1n8sock/considering_switching_filmora_12_with_kden_what

**Shotcut**
- https://forum.shotcut.org/t/settings-proxy-editing/18517
- https://forum.shotcut.org/t/export-failed-find-solutions/21628
- https://forum.shotcut.org/t/how-to-manually-recover-an-auto-saved-project-after-a-crash/20814
- https://forum.shotcut.org/t/new-custom-keyboard-shortcuts-in-version-22-09-brilliant/35481
- https://atomisystems.com/screencasting/shotcut-review-2026
- https://www.reddit.com/r/shotcut/comments/1qw307i/choppy_playback_even_with_proxy_and_preview
- https://www.reddit.com/r/shotcut/comments/1dsu3hv/shotcut_failing_to_export_immediately_defcon_1

**Optional (Olive / Lightworks / VEGAS / Avid) + comparative**
- https://www.olivevideoeditor.org/
- https://github.com/olive-editor/olive/releases
- https://www.debugpoint.com/olive-0-2-video-editor-review/
- https://lwks.com/
- https://uk.pcmag.com/video-editing/157730/lightworks
- https://www.redsharknews.com/lightworks-upgrades-to-v2025.1
- https://www.redsharknews.com/vegas-pro-22-review-ai-features-enhance-editing-more
- https://www.provideocoalition.com/vegas-pro-21-receives-new-major-update-during-nab-2024/
- https://www.reddit.com/r/VegasPro/comments/1peva5x/how_do_you_find_the_quality_of_vegas_pro_22_vs_21/
- https://www.reddit.com/r/VegasPro/comments/1jlizsd/first_off_lets_talk_about_this_programs_stability/
- https://www.avid.com/resource-center/whats-new-avid-media-composer-2026-8
- https://www.cined.com/avid-media-composer-2026-8-coming-soon-ai-multicam-cutting-an-uncapped-free-tier-and-a-bin-locking-shake-up/
- https://www.keycodemedia.com/making-sense-of-avid-media-composer-subscription-vs-perpetual/
- https://thepostflow.com/post-production/video-editing/best-video-editing-software-2026/
- https://community.avid.com/forums/t/130058.aspx

---

## 16. Limitations

- No hands-on version-locked testing; versions identified by 2025–2026 public footprint, not installed builds.
- Forum evidence skews toward complaints (survivorship bias); patterns required repetition across independent users before inclusion, but prevalence cannot be quantified from these sources.
- Pricing/AI-credit details shift quickly (especially CapCut/Filmora/Adobe); treat figures as 2026-09-21 snapshot.
- No paid-content scraping; no proprietary assets copied; no editor cloned.
