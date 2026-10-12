#!/usr/bin/env python3
"""A harsh critic of the physical guitar against recordings of real guitars.

For every recording in the reference manifest (assets/samples/guitar/manifest.json) the critic
renders the same note on the matching model, the same string and fret where the recording
documents them, with `fuzzboi-guitar-render`, then measures both the same way and grades the
model on what a listener hears in a plucked string:

  pitch       the fundamental, cents against the reference's own
  stretch     the inharmonicity coefficient B fitted to partials 2..12
  envelope    the levels of partials 1..12 early in the note, dB against the fundamental
  decay       the T60 of partials 1..8, as a ratio to the reference's
  two-stage   the fundamental's early decay rate against its late one (beating, polarizations)
  attack      the onset-to-peak time and the share of energy above 3 kHz in the first 30 ms
  brightness  the spectral centroid at 0.05-0.15 s, 0.3-0.6 s and 1.0-1.5 s
  noise       the energy between the partials in 0.05-0.3 s

Every measure has a tolerance a careful listener would accept; outside it the line FAILs. The
report (Markdown and JSON) ends with a grade per instrument and the worst offenders, and
side-by-side spectrograms are written for a reviewer to look at. Recordings carry their own room,
microphone and player; the model is dry and plucked by a robot, so a few measures (noise, the
top of the envelope) are expected to differ and are marked as such rather than excused.

    python3 -I tools/guitar_critic.py --render build/fuzzboi-guitar-render \\
        --manifest assets/samples/guitar/manifest.json --out /tmp/critic

Reads the recordings as data only (numpy and the standard library; spectrograms need ffmpeg).
"""
from __future__ import annotations

import argparse
import json
import math
import shutil
import subprocess
import sys
import wave
from pathlib import Path

import numpy as np

RATE = 48000

# How each recording maps onto a model: instrument -> (model, pick hardness, pluck mm, pickup).
MODELS = {
    "nylon": (1, 0.15, 130.0, 0),
    "steel": (0, 0.85, 100.0, 0),
    "electric": (2, 0.85, 90.0, 2),
}
MODEL_NAMES = ["Steel dreadnought", "Nylon classical", "Single-coil solid", "Humbucker solid"]


def read_wav(path: Path) -> tuple[np.ndarray, int]:
    with wave.open(str(path), "rb") as w:
        rate, width, channels, frames = w.getframerate(), w.getsampwidth(), w.getnchannels(), w.getnframes()
        raw = w.readframes(frames)
    if width == 3:
        b = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3)
        x = (b[:, 0].astype(np.int32) | (b[:, 1].astype(np.int32) << 8) | (b[:, 2].astype(np.int32) << 16))
        x = np.where(x >= 1 << 23, x - (1 << 24), x).astype(np.float64) / (1 << 23)
    elif width == 2:
        x = np.frombuffer(raw, dtype="<i2").astype(np.float64) / 32768.0
    elif width == 4:
        x = np.frombuffer(raw, dtype="<i4").astype(np.float64) / 2147483648.0
    else:
        raise ValueError(f"{path}: {width * 8}-bit PCM not handled")
    if channels > 1:
        x = x.reshape(-1, channels).mean(axis=1)
    return x, rate


def resample(x: np.ndarray, rate: int, to: int = RATE) -> np.ndarray:
    if rate == to:
        return x
    t = np.arange(int(len(x) * to / rate)) * rate / to
    return np.interp(t, np.arange(len(x)), x)


def onset(x: np.ndarray) -> int:
    """First sample within 30 dB of the peak, less 2 ms."""
    a = np.abs(x)
    i = int(np.argmax(a > a.max() * 10 ** (-30 / 20)))
    return max(0, i - int(0.002 * RATE))


def peak_near(x: np.ndarray, start: float, length: float, hz: float, span: float = 0.03) -> tuple[float, float]:
    """Frequency and amplitude of the strongest component within +-span of hz (Hann window,
    zero-padded FFT, parabolic interpolation)."""
    a, n = int(start * RATE), int(length * RATE)
    seg = x[a:a + n]
    if len(seg) < 1024:
        return hz, 0.0
    w = np.hanning(len(seg))
    nfft = 1 << int(math.ceil(math.log2(len(seg) * 8)))
    spec = np.abs(np.fft.rfft(seg * w, nfft))
    lo, hi = int(hz * (1 - span) * nfft / RATE), int(hz * (1 + span) * nfft / RATE) + 1
    lo, hi = max(lo, 1), min(hi, len(spec) - 2)
    if hi <= lo:
        return hz, 0.0
    k = lo + int(np.argmax(spec[lo:hi]))
    y0, y1, y2 = spec[k - 1], spec[k], spec[k + 1]
    d = 0.5 * (y0 - y2) / (y0 - 2 * y1 + y2) if (y0 - 2 * y1 + y2) != 0 else 0.0
    return (k + d) * RATE / nfft, y1 * 2 / w.sum()


def partials(x: np.ndarray, f0: float, start: float, length: float, count: int, b: float = 0.0) -> list[tuple[float, float]]:
    out = []
    f1 = f0
    for n in range(1, count + 1):
        guess = n * f1 * math.sqrt((1 + b * n * n) / (1 + b))
        if guess > 0.45 * RATE:
            break
        f, a = peak_near(x, start, length, guess, (0.012 if b else 0.025) if n > 1 else 0.03)
        out.append((f, a))
        if n == 1:
            f1 = f
    return out


def fit_b(parts: list[tuple[float, float]]) -> float:
    """B from (f_n / (n f_1))^2 = (1 + B n^2) / (1 + B) ~ 1 + B (n^2 - 1), weighted by level."""
    f1 = parts[0][0]
    num = den = 0.0
    amax = max(a for _, a in parts) or 1.0
    for n, (f, a) in enumerate(parts[1:12], start=2):
        if a < amax * 1e-3:
            continue
        y = (f / (n * f1)) ** 2 - 1
        num += (n * n - 1) * y
        den += (n * n - 1) ** 2
    return num / den if den else 0.0


def track(x: np.ndarray, hz: float, hop: float = 0.02, win: float = 0.08) -> tuple[np.ndarray, np.ndarray]:
    """Level (dB) of the component near hz over time."""
    n, h = int(win * RATE), int(hop * RATE)
    w = np.hanning(n)
    times, levels = [], []
    t = np.arange(n) / RATE
    probe = np.exp(-2j * np.pi * hz * t) * w
    for a in range(0, len(x) - n, h):
        levels.append(20 * math.log10(abs(np.dot(x[a:a + n], probe)) * 2 / w.sum() + 1e-12))
        times.append((a + n / 2) / RATE)
    return np.array(times), np.array(levels)


def t60(times: np.ndarray, levels: np.ndarray, t_from: float, t_to: float, floor_db) -> float | None:
    """T60 from a straight-line fit of the level, over the frames at least 10 dB above the noise
    floor measured between the partials (`floor_db`, per frame or one value), so a recording's
    hiss does not flatten the fit."""
    sel = (times >= t_from) & (times <= t_to) & (levels > np.asarray(floor_db) + 10.0)
    if sel.sum() < 5:
        return None
    slope = np.polyfit(times[sel], levels[sel], 1)[0]
    if slope >= -0.05:
        return 99.0
    return min(99.0, -60.0 / slope)


def centroid(x: np.ndarray, a: float, b: float) -> float:
    seg = x[int(a * RATE):int(b * RATE)]
    if len(seg) < 512:
        return float("nan")
    s = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))) ** 2
    f = np.fft.rfftfreq(len(seg), 1 / RATE)
    return float((s * f).sum() / (s.sum() + 1e-30))


def highpass(x: np.ndarray, hz: float = 40.0) -> np.ndarray:
    """Second-order Butterworth high-pass (bilinear), so DC and rumble in a recording do not
    drag its centroid or masquerade as beating."""
    w = math.tan(math.pi * hz / RATE)
    k = 1 / (1 + math.sqrt(2) * w + w * w)
    b0, b1, b2 = k, -2 * k, k
    a1, a2 = 2 * (w * w - 1) * k, (1 - math.sqrt(2) * w + w * w) * k
    y = np.zeros_like(x)
    x1 = x2 = y1 = y2 = 0.0
    for i, v in enumerate(x):
        o = b0 * v + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1, y2, y1 = x1, v, y1, o
        y[i] = o
    return y


def analyse(x: np.ndarray, expected_hz: float, usable_s: float) -> dict:
    x = highpass(x[:int(min(len(x), (usable_s + 0.5) * RATE))])
    x = x[onset(x):]
    x = x / (np.abs(x).max() + 1e-12)
    dur = min(len(x) / RATE, usable_s)
    m: dict = {}
    f0, _ = peak_near(x, 0.1, min(1.0, dur - 0.15), expected_hz, 0.04)
    m["f0"] = f0
    parts = partials(x, f0, 0.05, min(0.5, dur - 0.1), 16)
    m["B"] = max(0.0, fit_b(parts))
    # A second pass from the first estimate, so a stiff string's upper partials are found.
    m["B"] = max(0.0, fit_b(partials(x, f0, 0.05, min(0.5, dur - 0.1), 16, m["B"])))
    parts = partials(x, f0, 0.05, min(0.3, dur - 0.1), 12, m["B"])
    a1 = parts[0][1] or 1e-9
    m["envelope_db"] = [20 * math.log10(a / a1 + 1e-9) for _, a in parts]
    m["partial_hz"] = [f for f, _ in parts]
    # Partial decays over what the recording offers (loops and room tails excluded by usable_s).
    m["t60"] = []
    for n, (f, _) in enumerate(parts[:8], start=1):
        times, lv = track(x[:int(dur * RATE)], f)
        # The floor: the level half way to the next partial, where only noise lives.
        _, nf = track(x[:int(dur * RATE)], f + 0.5 * f0)
        top = lv.max() if len(lv) else 0.0
        m["t60"].append(t60(times, lv - top, 0.15, dur - 0.1, np.maximum(nf - top, -90.0)) if len(lv) else None)
    times, lv = track(x[:int(dur * RATE)], parts[0][0])
    _, nf = track(x[:int(dur * RATE)], 1.5 * parts[0][0])
    floor = np.maximum(nf - lv.max(), -90.0) if len(lv) else -90.0
    if len(lv):
        lv = lv - lv.max()
        early = t60(times, lv, 0.1, min(0.6, dur - 0.1), floor)
        late = t60(times, lv, 0.6, dur - 0.1, floor) if dur > 1.3 else None
        m["two_stage"] = (late / early) if early and late else None
        sel = (times > 0.2) & (times < dur - 0.1)
        if sel.sum() > 8:
            fit = np.polyval(np.polyfit(times[sel], lv[sel], 2), times[sel])
            m["beating_db"] = float(np.std(lv[sel] - fit))
        else:
            m["beating_db"] = None
    win = int(0.01 * RATE)
    env = np.sqrt(np.convolve(x[:int(0.3 * RATE)] ** 2, np.ones(win) / win, mode="full")[:int(0.3 * RATE)])
    m["attack_ms"] = float(np.argmax(env >= env.max() * 10 ** (-1 / 20)) / RATE * 1000)
    m["level_50ms"] = float(env[int(0.05 * RATE)] / env.max())
    head = x[:int(0.03 * RATE)]
    s = np.abs(np.fft.rfft(head * np.hanning(len(head)))) ** 2
    f = np.fft.rfftfreq(len(head), 1 / RATE)
    m["attack_hf"] = float(s[f > 3000].sum() / (s.sum() + 1e-30))
    m["centroid"] = [centroid(x, 0.05, 0.15), centroid(x, 0.3, min(0.6, dur)), centroid(x, 1.0, min(1.5, dur)) if dur > 1.2 else float("nan")]
    # Noise: energy away from the partials early on.
    seg = x[int(0.05 * RATE):int(0.3 * RATE)]
    spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))) ** 2
    fr = np.fft.rfftfreq(len(seg), 1 / RATE)
    mask = np.zeros_like(spec, dtype=bool)
    for n in range(1, 200):
        fn = n * f0 * math.sqrt((1 + m["B"] * n * n) / (1 + m["B"]))
        if fn > 16000:
            break
        mask |= np.abs(fr - fn) < max(8.0, fn * 0.01)
    m["noise_db"] = 10 * math.log10(spec[~mask & (fr > 60) & (fr < 16000)].sum() / (spec.sum() + 1e-30) + 1e-12)
    m["duration"] = dur
    return m


def grade(ref: dict, mod: dict) -> list[tuple[str, str, bool, str]]:
    """(measure, detail, passed, note) lines."""
    lines = []
    cents = 1200 * math.log2(mod["f0"] / ref["labelled_hz"])
    ref_cents = 1200 * math.log2(ref["f0"] / ref["labelled_hz"])
    lines.append(("pitch", f"model {cents:+.1f} ct from the note (the recording {ref_cents:+.1f} ct)", abs(cents) <= 8.0, ""))
    rb, mb = ref["B"], mod["B"]
    ratio = (mb + 2e-6) / (rb + 2e-6)
    lines.append(("stretch B", f"ref {rb:.2e} model {mb:.2e} (x{ratio:.2f})", 0.5 <= ratio <= 2.0 or (rb < 2e-5 and mb < 2e-5), ""))
    n = min(len(ref["envelope_db"]), len(mod["envelope_db"]), 12)
    diff = [mod["envelope_db"][i] - ref["envelope_db"][i] for i in range(1, n)]
    rms = math.sqrt(sum(d * d for d in diff) / max(1, len(diff)))
    lo = [d for d in diff[:5]]
    lines.append(("envelope 2-6", " ".join(f"{d:+.0f}" for d in lo) + f" dB", math.sqrt(sum(d * d for d in lo) / max(1, len(lo))) <= 6.0, ""))
    lines.append(("envelope 2-12", f"rms {rms:.1f} dB", rms <= 9.0, ""))
    ratios = []
    for i in range(min(8, len(ref["t60"]), len(mod["t60"]))):
        r, m_ = ref["t60"][i], mod["t60"][i]
        if r and m_:
            ratios.append(m_ / r)
    if ratios:
        geo = math.exp(sum(math.log(r) for r in ratios) / len(ratios))
        worst = max(ratios, key=lambda r: abs(math.log(r)))
        lines.append(("decay T60", "x" + " x".join(f"{r:.2f}" for r in ratios) + f" (geo x{geo:.2f})", 0.6 <= geo <= 1.6 and 0.4 <= worst <= 2.5, ""))
        lines.append(("decay tilt", f"fundamental x{ratios[0]:.2f}, upper x{ratios[-1]:.2f}", abs(math.log(ratios[-1] / ratios[0])) <= math.log(2.0), ""))
    if ref.get("two_stage") and mod.get("two_stage"):
        lines.append(("two-stage", f"late/early ref {ref['two_stage']:.2f} model {mod['two_stage']:.2f}", abs(math.log(mod["two_stage"] / ref["two_stage"])) <= math.log(2.0), ""))
    if ref.get("beating_db") is not None and mod.get("beating_db") is not None:
        lines.append(("beating", f"ref {ref['beating_db']:.1f} dB model {mod['beating_db']:.1f} dB", abs(mod["beating_db"] - ref["beating_db"]) <= 2.0, ""))
    lines.append(("attack", f"peak at ref {ref['attack_ms']:.1f} ms model {mod['attack_ms']:.1f} ms", abs(mod["attack_ms"] - ref["attack_ms"]) <= max(4.0, ref["attack_ms"] * 0.5), ""))
    hf = 10 * math.log10((mod["attack_hf"] + 1e-6) / (ref["attack_hf"] + 1e-6))
    both_tiny = ref["attack_hf"] < 0.005 and mod["attack_hf"] < 0.005
    lines.append(("attack >3k", f"ref {ref['attack_hf']*100:.2f}% model {mod['attack_hf']*100:.2f}% ({hf:+.1f} dB)", both_tiny or abs(hf) <= 6.0, ""))
    lines.append(("attack shape", f"level at 50 ms ref {ref['level_50ms']:.2f} model {mod['level_50ms']:.2f} of peak", abs(mod["level_50ms"] - ref["level_50ms"]) <= 0.25, ""))
    for i, label in enumerate(["0.1 s", "0.45 s", "1.25 s"]):
        r, m_ = ref["centroid"][i], mod["centroid"][i]
        if not (math.isnan(r) or math.isnan(m_)):
            lines.append((f"centroid {label}", f"ref {r:.0f} Hz model {m_:.0f} Hz (x{m_ / r:.2f})", 0.75 <= m_ / r <= 1.33, ""))
    lines.append(("noise", f"ref {ref['noise_db']:.1f} dB model {mod['noise_db']:.1f} dB", abs(mod["noise_db"] - ref["noise_db"]) <= 10.0,
                  "recordings carry room and preamp noise"))
    return lines


def usable_seconds(entry: dict) -> float:
    """How much of a recording is the note itself: up to the uploader's loop point where there is
    one (the caveat names its frame), else 4 s."""
    import re
    caveat = entry.get("caveats") or ""
    dur = float(entry.get("duration_s") or 8.0)
    m = re.search(r"loop\w* region starting at frame (\d+)", caveat)
    if m:
        return min(dur, int(m.group(1)) / float(entry.get("sample_rate") or 48000) - 0.05, 4.0)
    if "loop" in caveat.lower():
        return min(dur, 2.0)
    return min(dur, 4.0)


def spectrogram(a: Path, b: Path, out: Path, title: str) -> None:
    if not shutil.which("ffmpeg"):
        return
    cmd = ["ffmpeg", "-v", "error", "-y", "-i", str(a), "-i", str(b), "-filter_complex",
           "[0:a]atrim=0:3,aresample=48000,showspectrumpic=s=640x400:legend=1:scale=log:fscale=log:stop=8000[l];"
           "[1:a]atrim=0:3,aresample=48000,showspectrumpic=s=640x400:legend=1:scale=log:fscale=log:stop=8000[r];"
           f"[l][r]hstack,drawtext=text='{title}   left real  right model':x=20:y=8:fontcolor=white:fontsize=18",
           "-frames:v", "1", str(out)]
    subprocess.run(cmd, check=False)


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--render", required=True)
    ap.add_argument("--manifest", default="assets/samples/guitar/manifest.json")
    ap.add_argument("--out", required=True)
    ap.add_argument("--only", default="", help="substring filter on file names")
    ap.add_argument("--no-images", action="store_true")
    ap.add_argument("--extra", default="", help="extra key=value arguments for the renderer (calibration)")
    args = ap.parse_args(argv)
    manifest = json.loads(Path(args.manifest).read_text())
    root = Path(args.manifest).parent
    out = Path(args.out)
    (out / "renders").mkdir(parents=True, exist_ok=True)
    (out / "spectrograms").mkdir(parents=True, exist_ok=True)
    results = []
    for entry in manifest["samples"]:
        name = entry["file"]
        if args.only and args.only not in name:
            continue
        model, pick, pluck, pickup = MODELS[entry["instrument"]]
        if "birch" in name:
            pickup = 1
        ref_x, ref_rate = read_wav(root / name)
        ref_x = resample(ref_x, ref_rate)
        render = out / "renders" / name
        cmd = [args.render, str(render), f"model={model}", f"pick={pick}", f"pluck_mm={pluck}", f"pickup={pickup}",
               f"note={entry['midi']}", "velocity=0.75", "seconds=4.5"] + args.extra.split()
        if entry.get("string"):
            cmd += [f"string={entry['string']}", f"fret={entry.get('fret') or 0}"]
        info = subprocess.run(cmd, check=True, capture_output=True, text=True).stdout.strip()
        mod_x, _ = read_wav(render)
        expected = float(entry.get("verified_pitch_hz") or 440.0 * 2 ** ((entry["midi"] - 69) / 12))
        usable = usable_seconds(entry)
        ref = analyse(ref_x, expected, usable)
        mod = analyse(mod_x, ref["f0"], usable)
        ref["labelled_hz"] = 440.0 * 2 ** ((entry["midi"] - 69) / 12)
        lines = grade(ref, mod)
        results.append({"file": name, "instrument": entry["instrument"], "model": MODEL_NAMES[model], "render": info,
                        "ref": ref, "model_measures": mod, "lines": lines})
        if not args.no_images:
            spectrogram(root / name, render, out / "spectrograms" / (Path(name).stem + ".png"), Path(name).stem)
        failed = sum(1 for l in lines if not l[2])
        print(f"{name:38s} {failed:2d}/{len(lines)} failed   ({info})")
    # Report
    md = ["# Guitar critic", "",
          "Each recording against the same note on the model. A FAIL is outside what a careful listener",
          "would accept; nothing is excused, though some measures are expected to differ (noted).", ""]
    by_instrument: dict[str, list[int]] = {}
    measure_fails: dict[str, int] = {}
    measure_total: dict[str, int] = {}
    for r in results:
        md.append(f"## {r['file']}  ->  {r['model']}  ({r['render']})")
        md.append("")
        md.append("| measure | detail | verdict |")
        md.append("|---|---|---|")
        for measure, detail, ok, note in r["lines"]:
            md.append(f"| {measure} | {detail} | {'pass' if ok else '**FAIL**'}{' (' + note + ')' if note and not ok else ''} |")
            key = measure.split()[0]
            measure_total[key] = measure_total.get(key, 0) + 1
            if not ok:
                measure_fails[key] = measure_fails.get(key, 0) + 1
        md.append("")
        p = sum(1 for l in r["lines"] if l[2])
        by_instrument.setdefault(r["instrument"], []).extend([p, len(r["lines"])])
    md.append("## Grades")
    md.append("")
    for inst, v in by_instrument.items():
        passed, total = sum(v[0::2]), sum(v[1::2])
        md.append(f"- **{inst}**: {passed}/{total} measures pass ({100 * passed / max(1, total):.0f}%)")
    md.append("")
    md.append("## Where in frequency the model is off")
    md.append("")
    md.append("Real minus model partial level, averaged over every note of an instrument after taking out")
    md.append("each note's own mean (so the excitation's note-to-note differences cancel and what is left is")
    md.append("the instrument's fixed colouring: the body's radiation, or the pickup and its load). Partials")
    md.append("1 to 12 in 0.05 to 0.3 s, one-third-octave bands.")
    md.append("")
    edges = [80 * 2 ** (k / 3) for k in range(0, 20)]
    md.append("| instrument | " + " | ".join(f"{e:.0f}" for e in edges[:-1]) + " |")
    md.append("|---|" + "---|" * (len(edges) - 1))
    for inst in by_instrument:
        sums = [0.0] * (len(edges) - 1)
        counts = [0] * (len(edges) - 1)
        for r in results:
            if r["instrument"] != inst:
                continue
            ref, mod = r["ref"], r["model_measures"]
            n = min(len(ref["envelope_db"]), len(mod["envelope_db"]))
            d = [ref["envelope_db"][i] - mod["envelope_db"][i] for i in range(n)]
            if not d:
                continue
            mean = sum(d) / len(d)
            for i in range(n):
                f = ref["partial_hz"][i]
                for b in range(len(edges) - 1):
                    if edges[b] <= f < edges[b + 1]:
                        sums[b] += d[i] - mean
                        counts[b] += 1
        md.append(f"| {inst} | " + " | ".join(f"{sums[b] / counts[b]:+.0f}" if counts[b] else "" for b in range(len(edges) - 1)) + " |")
    md.append("")
    md.append("## Failures by measure")
    md.append("")
    for key in sorted(measure_total, key=lambda k: -measure_fails.get(k, 0)):
        md.append(f"- {key}: {measure_fails.get(key, 0)}/{measure_total[key]} fail")
    (out / "report.md").write_text("\n".join(md) + "\n")
    (out / "report.json").write_text(json.dumps(results, indent=1, default=float))
    print("\n".join(md[md.index("## Grades"):]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
