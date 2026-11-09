#!/usr/bin/env python3
"""Loudness match for the generated bank (one calibration pass).

    CYR_CALIBRATE=1 python3 tools/gen_presets.py  # bank at -6 dB, build, then
    ResonatorRenderTest <wavDir>                 # renders every factory preset to WAV
    python3 tools/match_loudness.py <wavDir>      # writes tools/bank_trims.json, then run gen_presets.py again

Loudness = loudest 100 ms window (RMS of both channels). The target is the median of the 25 hand-made
presets, which are already matched, so the new bank sits at the same level. Short sounds (psy basses,
zaps, plucks) are judged by their loudest 100 ms instead of a long average, so they are not pushed too hot.
If a preset would need more than the +6 dB master, the MIXBUS limiter makes up the rest.
"""
import json, os, sys, wave
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gen_presets as G


def read(path):
    with wave.open(path) as w:
        n, ch, sw = w.getnframes(), w.getnchannels(), w.getsampwidth()
        raw = np.frombuffer(w.readframes(n), dtype=np.uint8).reshape(-1, sw)
        v = (raw[:, 0].astype(np.int32) | (raw[:, 1].astype(np.int32) << 8) | (raw[:, 2].astype(np.int32) << 16))
        v = np.where(v >= 1 << 23, v - (1 << 24), v) / float(1 << 23)
        return v.reshape(-1, ch), w.getframerate()


def measure(path):
    x, sr = read(path)
    p2 = (x ** 2).mean(axis=1)
    win = int(sr * 0.1)
    c = np.convolve(p2, np.ones(win) / win, mode="valid")
    return 10 * np.log10(c.max() + 1e-12), float(np.abs(x).max())


def legal(name):
    return name.replace(":", "").replace("/", "").replace("\\", "")


def main():
    d = sys.argv[1]
    old = sorted(G.EXISTING)
    ref = [measure(os.path.join(d, legal(n) + ".wav"))[0] for n in old]
    target = float(np.median(ref))
    trims_path = os.path.join(HERE, "bank_trims.json")
    out, report = {}, []
    for name, p in G.make_bank():
        loud, peak = measure(os.path.join(d, legal(name) + ".wav"))   # rendered at master -6 dB, no limiter
        gain = target - loud
        # sharp transients: keep the peak under ~1.2 before the output soft clip (about -0.5 dBFS after it)
        gain = min(gain, 20 * np.log10(1.2 / max(peak, 1e-6)))
        total = -6.0 + gain
        if total <= 6.0:
            out[name] = round(total, 1)
        else:   # needs more than the master can give: MIXBUS limiter makes it up, master at 0 dB
            out[name] = {"master": 0.0, "lm": round(min(18.0, total), 1)}
        report.append((name, round(loud, 1), round(peak, 3), out[name]))
    json.dump(out, open(trims_path, "w"), indent=1, sort_keys=True)
    print("target %.1f dB (100 ms), %d presets" % (target, len(out)))
    for r in sorted(report, key=lambda r: r[1])[:12]: print("quietest", r)
    print("limiter on:", sum(1 for v in out.values() if isinstance(v, dict)))
    vals = [v if not isinstance(v, dict) else 6.0 for v in out.values()]
    print("master range %.1f .. %.1f" % (min(vals), max(vals)))


if __name__ == "__main__":
    main()
