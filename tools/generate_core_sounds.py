#!/usr/bin/env python3
"""Generate the original, deterministic PULSAR Core Sounds Pack V1."""
from __future__ import annotations

import json
import math
import random
import struct
import wave
from pathlib import Path

RATE = 48_000
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets" / "sounds"
SEED = 0x50554C53

SOUNDS = [
    ("click_soft", "Click Soft", "UI", 0.12, "click"),
    ("click_hard", "Click Hard", "UI", 0.12, "click_hard"),
    ("confirm", "Confirm", "UI", 0.42, "confirm"),
    ("cancel", "Cancel", "UI", 0.28, "cancel"),
    ("notification", "Notification", "UI", 0.55, "notification"),
    ("error", "Error", "UI", 0.36, "error"),
    ("impact_soft", "Impact Soft", "IMPACTS", 0.45, "impact_soft"),
    ("impact_medium", "Impact Medium", "IMPACTS", 0.55, "impact_medium"),
    ("impact_heavy", "Impact Heavy", "IMPACTS", 0.75, "impact_heavy"),
    ("thump", "Thump", "IMPACTS", 0.5, "thump"),
    ("whoosh_short", "Whoosh Short", "WHOOSHES", 0.45, "whoosh"),
    ("whoosh_medium", "Whoosh Medium", "WHOOSHES", 0.9, "whoosh"),
    ("whoosh_reverse", "Whoosh Reverse", "WHOOSHES", 0.8, "reverse"),
    ("beep_short", "Beep Short", "TECHNOLOGY", 0.24, "beep"),
    ("beep_double", "Beep Double", "TECHNOLOGY", 0.48, "double_beep"),
    ("digital_tick", "Digital Tick", "TECHNOLOGY", 0.18, "tick"),
    ("glitch_short", "Glitch Short", "TECHNOLOGY", 0.38, "glitch"),
    ("riser_short", "Riser Short", "CINEMATIC", 1.25, "riser"),
    ("drop_short", "Drop Short", "CINEMATIC", 0.7, "drop"),
    ("pulse", "Pulse", "CINEMATIC", 0.8, "pulse"),
    ("sweep_soft", "Sweep Soft", "TRANSITIONS", 0.55, "sweep"),
    ("sweep_fast", "Sweep Fast", "TRANSITIONS", 0.32, "sweep"),
    ("transition_tick", "Transition Tick", "TRANSITIONS", 0.2, "tick"),
    ("room_tone_synthetic", "Room Tone Synthetic", "AMBIENCE", 2.0, "room"),
    ("low_hum", "Low Hum", "AMBIENCE", 1.5, "hum"),
    # Added after V1: appended so the seeds (and therefore the bytes) of every earlier sound stay identical.
    ("pop_soft", "Pop Soft", "POPS", 0.18, "pop"),
    ("pop_bubble", "Pop Bubble", "POPS", 0.3, "bubble"),
    ("pop_double", "Pop Double", "POPS", 0.35, "pop_double"),
    ("ding_bright", "Ding Bright", "ALERTS", 0.9, "ding"),
    ("ding_soft", "Ding Soft", "UI", 0.7, "ding_soft"),
    ("alert_triple", "Alert Triple", "ALERTS", 0.6, "triple_beep"),
    ("shutter", "Shutter", "CLICKS", 0.25, "shutter"),
    ("click_double", "Click Double", "CLICKS", 0.22, "click_double"),
    ("whoosh_long", "Whoosh Long", "WHOOSHES", 1.4, "whoosh"),
    ("boom_deep", "Boom Deep", "IMPACTS", 1.2, "boom"),
    ("sparkle_up", "Sparkle Up", "TRANSITIONS", 0.8, "sparkle"),
    ("swell_soft", "Swell Soft", "CINEMATIC", 1.6, "swell"),
]

# Category of each sound for the library (the id order above must not change; categories may).
CATEGORY_OVERRIDES = {
    "click_soft": "CLICKS", "click_hard": "CLICKS", "digital_tick": "CLICKS",
    "notification": "ALERTS", "error": "ALERTS", "beep_short": "ALERTS", "beep_double": "ALERTS",
}

# Spanish names and categories shown in the interface (ids and files stay English).
SPANISH = {
    "click_soft": "Clic suave", "click_hard": "Clic fuerte", "confirm": "Confirmar", "cancel": "Cancelar",
    "notification": "Notificación", "error": "Error", "impact_soft": "Impacto suave", "impact_medium": "Impacto medio",
    "impact_heavy": "Impacto fuerte", "thump": "Golpe grave", "whoosh_short": "Whoosh corto", "whoosh_medium": "Whoosh medio",
    "whoosh_reverse": "Whoosh inverso", "beep_short": "Pitido corto", "beep_double": "Pitido doble", "digital_tick": "Tic digital",
    "glitch_short": "Glitch corto", "riser_short": "Subida corta", "drop_short": "Caída corta", "pulse": "Pulso",
    "sweep_soft": "Barrido suave", "sweep_fast": "Barrido rápido", "transition_tick": "Tic de transición",
    "room_tone_synthetic": "Ambiente de sala", "low_hum": "Zumbido grave", "pop_soft": "Pop suave", "pop_bubble": "Pop burbuja",
    "pop_double": "Pop doble", "ding_bright": "Ding brillante", "ding_soft": "Ding suave", "alert_triple": "Alerta triple",
    "shutter": "Obturador", "click_double": "Doble clic", "whoosh_long": "Whoosh largo", "boom_deep": "Boom profundo",
    "sparkle_up": "Destello", "swell_soft": "Crescendo suave",
}
CATEGORY_SPANISH = {
    "UI": "Interfaz", "CLICKS": "Clics", "IMPACTS": "Impactos", "WHOOSHES": "Whoosh", "POPS": "Pops",
    "TRANSITIONS": "Transiciones", "ALERTS": "Alertas", "TECHNOLOGY": "Tecnología", "CINEMATIC": "Cine", "AMBIENCE": "Ambiente",
}

def envelope(i: int, n: int, attack: float = 0.01, release: float = 0.04) -> float:
    # i is a sample index; convert it to seconds exactly once.  The previous
    # expression divided by the sample period, turning seconds into samples
    # and making the release phase zero after the first sample.
    t = i / RATE
    total = n / RATE
    a = min(1.0, t / attack) if attack else 1.0
    r = min(1.0, max(0.0, (total - t) / release)) if release else 1.0
    return a * r

def render(kind: str, seconds: float, rng: random.Random) -> list[float]:
    n = max(1, round(seconds * RATE))
    out = [0.0] * n
    for i in range(n):
        t = i / RATE
        e = envelope(i, n)
        if kind.startswith("click"):
            f = 1800.0 if kind == "click" else 900.0
            out[i] = 0.45 * math.sin(2 * math.pi * f * t) * math.exp(-32 * t) * e
        elif kind in ("confirm", "notification", "beep"):
            f = 880.0 + (440.0 if kind != "beep" else 0.0)
            out[i] = 0.35 * math.sin(2 * math.pi * f * t) * e
        elif kind in ("cancel", "error"):
            out[i] = 0.35 * math.sin(2 * math.pi * (260.0 if kind == "error" else 420.0) * t) * e
        elif kind == "double_beep":
            out[i] = 0.3 * math.sin(2 * math.pi * (660.0 if t < seconds / 2 else 990.0) * t) * e
        elif kind == "tick":
            out[i] = 0.28 * math.sin(2 * math.pi * 2400.0 * t) * math.exp(-28 * t) * e
        elif kind in ("impact_soft", "impact_medium", "impact_heavy", "thump", "drop"):
            scale = {"impact_soft": .35, "impact_medium": .5, "impact_heavy": .7, "thump": .55, "drop": .6}[kind]
            out[i] = scale * (math.sin(2 * math.pi * (90 - 55 * t) * t) + .2 * rng.uniform(-1, 1)) * math.exp(-5 * t) * e
        elif kind in ("whoosh", "reverse", "sweep", "riser"):
            rising = kind != "reverse"
            u = t / seconds
            f = 180.0 + 3000.0 * (u if rising else 1.0 - u)
            out[i] = .32 * math.sin(2 * math.pi * f * t) * (u if rising else 1 - u) * e
        elif kind in ("pop", "bubble", "pop_double"):
            # A short pitched blip that drops in frequency; the double repeats it a little later.
            local = t if kind != "pop_double" or t < seconds / 2 else t - seconds / 2
            f0, f1 = (900.0, 260.0) if kind != "bubble" else (320.0, 1100.0)
            freq = f1 + (f0 - f1) * math.exp(-34 * local)
            out[i] = 0.5 * math.sin(2 * math.pi * freq * local) * math.exp(-26 * local) * e
        elif kind in ("ding", "ding_soft"):
            base = 1318.5 if kind == "ding" else 880.0
            decay = 5.5 if kind == "ding" else 7.0
            out[i] = (0.4 * math.sin(2 * math.pi * base * t) + 0.18 * math.sin(2 * math.pi * base * 2.76 * t)) * math.exp(-decay * t) * e
        elif kind == "triple_beep":
            slot = int(t / (seconds / 3))
            local = t - slot * seconds / 3
            gate = 1.0 if local < seconds / 3 * 0.6 else 0.0
            out[i] = 0.32 * math.sin(2 * math.pi * (740.0 + 160.0 * slot) * t) * gate * e
        elif kind == "shutter":
            first = rng.uniform(-1, 1) * math.exp(-90 * t)
            second_t = max(0.0, t - 0.09)
            second = rng.uniform(-1, 1) * math.exp(-70 * second_t) if t >= 0.09 else 0.0
            out[i] = 0.4 * (first + 0.8 * second) * e
        elif kind == "click_double":
            local = t if t < 0.09 else t - 0.09
            out[i] = 0.42 * math.sin(2 * math.pi * 1500.0 * local) * math.exp(-60 * local) * e
        elif kind == "boom":
            out[i] = 0.6 * (math.sin(2 * math.pi * (55 - 20 * min(t, 1.0)) * t) + 0.12 * rng.uniform(-1, 1) * math.exp(-8 * t)) * math.exp(-3.2 * t) * e
        elif kind == "sparkle":
            notes = (1568.0, 1976.0, 2349.0, 2637.0, 3136.0)
            step = seconds / len(notes)
            slot = min(len(notes) - 1, int(t / step))
            local = t - slot * step
            out[i] = 0.28 * math.sin(2 * math.pi * notes[slot] * t) * math.exp(-9 * local) * e
        elif kind == "swell":
            u = t / seconds
            out[i] = 0.3 * (math.sin(2 * math.pi * 220.0 * t) + 0.6 * math.sin(2 * math.pi * 330.0 * t)) * math.sin(math.pi * u) ** 2 * e
        elif kind == "glitch":
            out[i] = .24 * rng.uniform(-1, 1) * (1.0 if (i // 96) % 3 else .2) * e
        elif kind == "pulse":
            out[i] = .35 * math.sin(2 * math.pi * 3.0 * t) * e
        elif kind == "room":
            out[i] = .025 * rng.uniform(-1, 1) * e
        elif kind == "hum":
            out[i] = .12 * math.sin(2 * math.pi * 60.0 * t) * e
    peak = max(abs(x) for x in out) if out else 0.0
    scale = 0.85 / max(1.0, peak)
    return [max(-0.95, min(0.95, x * scale)) for x in out]

def write_wav(path: Path, samples: list[float]) -> None:
    with wave.open(str(path), "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(RATE)
        f.writeframes(b"".join(struct.pack("<h", round(x * 32767)) for x in samples))

def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    entries = []
    for index, (sid, name, category, duration, kind) in enumerate(SOUNDS):
        path = OUT / f"{sid}.wav"
        write_wav(path, render(kind, duration, random.Random(SEED + index)))
        category = CATEGORY_OVERRIDES.get(sid, category)
        entries.append({
            "id": sid, "name": name, "category": category,
            "name_es": SPANISH[sid], "category_es": CATEGORY_SPANISH[category],
            "path": f"sounds/{path.name}", "duration": duration,
            "duration_frames": round(duration * RATE),
            "sample_rate": RATE, "channels": 1,
            "source_type": "generated-original",
            "generator": "tools/generate_core_sounds.py",
            "third_party_source": "none",
            "license": "authored-in-project",
        })
    (OUT / "index.json").write_text(json.dumps({"version": 1, "sounds": entries}, indent=2) + "\n")

if __name__ == "__main__":
    main()
