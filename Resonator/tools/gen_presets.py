#!/usr/bin/env python3
"""Generates the Calabi-Yau Resonator factory bank (Source/RPresetsBank.inc).

Each category has a sound-design recipe; a seeded random generator varies it inside musical ranges,
so the bank is reproducible. Loudness trims measured by ResonatorRenderTest are read from
tools/bank_trims.json (name -> master dB) when present.

    python3 tools/gen_presets.py            # writes Source/RPresetsBank.inc
"""
import json, os, random

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "Source", "RPresetsBank.inc")
TRIMS = os.path.join(HERE, "bank_trims.json")

# mod sources (RParams.h ModSrc)
ENV1, ENV2, ENV3, ENV4 = "MS_Env1", "MS_Env2", "MS_Env3", "MS_Env4"
LFO = ["", "MS_Lfo1", "MS_Lfo2", "MS_Lfo3", "MS_Lfo4", "MS_Lfo5", "MS_Lfo6"]
VEL, KEY, MW, AT, RND = "MS_Velocity", "MS_Key", "MS_ModWheel", "MS_Aftertouch", "MS_Random"
# LFO shapes
SINE, TRI, SAWUP, SAWDN, SQUARE, SH, SMOOTH = range(7)
# divisions: 4 = 1/16, 6 = 1/8T, 7 = 1/8, 8 = 1/8D, 9 = 1/4T, 10 = 1/4, 12 = 1/2, 13 = 1/1, 14 = 2/1, 15 = 4/1
D16, D8T, D8, D8D, D4T, D4, D2, D1, D2B, D4B = 4, 6, 7, 8, 9, 10, 12, 13, 14, 15

MINOR7, MAJOR7, SUS4, FIFTHS, MINOR9, MINOR = [0, 3, 7, 10], [0, 4, 7, 11], [0, 5, 7, 12], [0, 7, 12, 19], [0, 3, 10, 14], [0, 3, 7, 12]


class P:
    def __init__(self, cat):
        self.cat, self.kv, self.mods = cat, {}, []

    def set(self, **kw):
        self.kv.update(kw); return self

    def osc(self, o, **kw):
        self.kv["osc%s_on" % o] = 1
        for k, v in kw.items(): self.kv["osc%s_%s" % (o, k)] = v
        return self

    def amp(self, a, d, s, r): return self.set(amp_a=a, amp_d=d, amp_s=s, amp_r=r)

    def env(self, n, a, d, s, r):
        for k, v in zip("adsr", (a, d, s, r)): self.kv["env%d_%s" % (n, k)] = v
        return self

    def lfo(self, n, rate=None, shape=None, sync=None, div=None, trig=None, fade=None, phase=None):
        for k, v in (("rate", rate), ("shape", shape), ("sync", sync), ("div", div), ("trig", trig), ("fade", fade), ("phase", phase)):
            if v is not None: self.kv["lfo%d_%s" % (n, k)] = v
        return self

    def mod(self, src, dst, amt):
        if len(self.mods) < 32: self.mods.append((src, dst, amt))
        return self

    def flt(self, cut, res=0.15, drive=0.0, typ=0, key=0.0):
        return self.set(flt_on=1, flt_type=typ, flt_cut=cut, flt_res=res, flt_drive=drive, flt_key=key)

    # ---- effects ----
    def reverb(self, mix, decay=3.0, size=0.7, damp=0.4, pre=10.0):
        return self.set(fx_rv_on=1, fx_rv_mix=mix, fx_rv_decay=decay, fx_rv_size=size, fx_rv_damp=damp, fx_rv_pre=pre)

    def delay(self, mix, fb=0.4, div=D8D, pp=1, hicut=7000.0):
        return self.set(fx_dl_on=1, fx_dl_mix=mix, fx_dl_fb=fb, fx_dl_div=div, fx_dl_sync=1, fx_dl_pp=pp, fx_dl_highcut=hicut)

    def chorus(self, mix=0.35, rate=0.6, depth=0.5): return self.set(mb_ch_on=1, mb_ch_mix=mix, mb_ch_rate=rate, mb_ch_depth=depth)
    def phaser(self, mix=0.35, rate=0.2, fb=0.4, stages=6): return self.set(fx_ph_on=1, fx_ph_mix=mix, fx_ph_rate=rate, fx_ph_fb=fb, fx_ph_stages=stages)
    def flanger(self, mix=0.35, rate=0.15, fb=0.5): return self.set(fx_fl_on=1, fx_fl_mix=mix, fx_fl_rate=rate, fx_fl_fb=fb)
    def tape(self, drive=0.3, wow=0.1): return self.set(mb_tp_on=1, mb_tp_drive=drive, mb_tp_wow=wow)
    def clouds(self, mix=0.4, size=0.5, density=0.7, texture=0.5, pitch=0.0, fb=0.2, rv=0.4):
        return self.set(fx_gr_on=1, fx_gr_mix=mix, fx_gr_size=size, fx_gr_density=density, fx_gr_texture=texture, fx_gr_pitch=pitch, fx_gr_fb=fb, fx_gr_reverb=rv)
    def rings(self, mix=0.35, model=0, structure=0.4, bright=0.5, damping=0.6):
        return self.set(fx_rn_on=1, fx_rn_mix=mix, fx_rn_model=model, fx_rn_structure=structure, fx_rn_bright=bright, fx_rn_damping=damping, fx_rn_follow=1)
    def gate(self, pat, div=D16, depth=1.0, smooth=0.2): return self.set(mb_tg_on=1, mb_tg_pat=pat, mb_tg_div=div, mb_tg_depth=depth, mb_tg_smooth=smooth)
    def stereo(self, width=1.3, haas=0.0): return self.set(fx_st_on=1, fx_st_width=width, fx_st_haas=haas)
    def comp(self, thr=-14.0, ratio=3.0, gain=2.0): return self.set(mb_cp_on=1, mb_cp_thr=thr, mb_cp_ratio=ratio, mb_cp_gain=gain)
    def crush(self, bits=8.0, rate=2.0, mix=0.5): return self.set(mb_bc_on=1, mb_bc_bits=bits, mb_bc_rate=rate, mb_bc_mix=mix)
    def imager(self, width=1.3, bass=150.0): return self.set(mb_im_on=1, mb_im_width=width, mb_im_bass=bass)


# =============================================================================================
# Recipes. r = random.Random, every value picked inside a range that sounds right for the role.
# =============================================================================================
def u(r, a, b, nd=3): return round(r.uniform(a, b), nd)
def patch(r, n): return r.randint(0, n - 1)


def bass(r, i):
    p = P("Bass")
    kind = ["sub", "psy", "psy", "fm", "reese", "pd", "acid", "growl", "scan"][i % 9]
    p.set(poly=1, glide=u(r, 0.0, 0.06))
    n = r.randint(2, 6)
    if kind == "sub":
        p.osc("a", n=r.randint(2, 3), ru=u(r, 0.3, 0.6), rv=u(r, 0.0, 0.1), oct=-1, width=0.0, rphase=0, level=0.85)
        if r.random() < 0.5: p.osc("b", n=2, ru=u(r, 0.2, 0.5), rv=0.0, oct=-2, width=0.0, rphase=0, level=u(r, 0.3, 0.5))
        p.flt(u(r, 280, 650), u(r, 0.05, 0.2)).amp(0.002, u(r, 0.4, 0.9), u(r, 0.8, 0.95), u(r, 0.08, 0.18))
        if r.random() < 0.5: p.tape(u(r, 0.15, 0.35), 0.0)
    elif kind == "psy":
        p.set(glide=0.0)
        p.osc("a", n=n, k1=patch(r, n), ru=u(r, 0.8, 0.95), rv=u(r, 0.1, 0.3), ratio=r.choice([1, 1, 2]), oct=-1, width=u(r, 0.05, 0.2), rphase=0, level=0.85)
        p.flt(u(r, 500, 1300), u(r, 0.25, 0.5), u(r, 0.15, 0.4)).amp(0.001, u(r, 0.13, 0.22), u(r, 0.0, 0.25), u(r, 0.04, 0.08))
        p.env(1, 0.001, u(r, 0.09, 0.17), 0.0, 0.08).mod(ENV1, "flt_cut", u(r, 0.35, 0.55))
        if r.random() < 0.6:
            p.osc("b", n=r.randint(2, 4), ru=u(r, 0.4, 0.8), rv=u(r, 0.0, 0.3), oct=r.choice([-1, 0]), level=0.0, rphase=0)
            p.set(fm_ba=u(r, 0.1, 0.3)).mod(ENV1, "fm_ba", u(r, 0.2, 0.4))
        else:
            p.mod(ENV1, "osca_rv", u(r, 0.2, 0.4))
        p.mod(VEL, "flt_cut", u(r, 0.05, 0.15))
        if r.random() < 0.5: p.tape(u(r, 0.25, 0.45), 0.0)
    elif kind == "fm":
        p.osc("a", n=r.randint(2, 4), ru=u(r, 0.4, 0.8), rv=u(r, 0.05, 0.3), oct=-1, width=0.1, rphase=0, level=0.85)
        p.osc("b", n=r.randint(2, 5), ru=u(r, 0.3, 0.7), rv=u(r, 0.0, 0.4), oct=r.choice([-1, 0, 0, 1]), semi=r.choice([0, 0, 7, 12]), level=0.0, rphase=0)
        p.set(fm_ba=u(r, 0.2, 0.45)).flt(u(r, 1200, 3500), u(r, 0.1, 0.3))
        p.amp(0.001, u(r, 0.3, 0.6), u(r, 0.5, 0.8), u(r, 0.08, 0.15))
        p.env(2, 0.001, u(r, 0.12, 0.3), u(r, 0.0, 0.2), 0.1).mod(ENV2, "fm_ba", u(r, 0.3, 0.55))
    elif kind == "reese":
        for o, pan in (("a", -0.2), ("b", 0.2)):
            p.osc(o, n=r.randint(3, 5), ru=u(r, 0.6, 0.85), rv=u(r, 0.2, 0.4), oct=-1, uni=r.choice([2, 3]), det=u(r, 12, 24), width=u(r, 0.2, 0.4), pan=pan, level=0.6, rphase=0)
        p.set(oscb_fine=u(r, 4, 12)).flt(u(r, 700, 1400), u(r, 0.15, 0.35), u(r, 0.1, 0.3))
        p.amp(0.004, 0.6, 0.9, 0.15).lfo(1, rate=u(r, 0.1, 0.4), shape=TRI).mod(LFO[1], "flt_cut", u(r, 0.15, 0.3))
        p.lfo(2, rate=u(r, 0.05, 0.2), shape=SMOOTH).mod(LFO[2], "osca_alpha", u(r, 0.2, 0.4))
    elif kind == "pd":
        p.osc("a", n=n, ru=u(r, 0.6, 0.9), rv=u(r, 0.1, 0.35), oct=-1, width=0.1, rphase=0, level=0.85)
        p.osc("b", n=r.randint(2, 4), ru=u(r, 0.4, 0.7), rv=u(r, 0.0, 0.3), oct=r.choice([-1, 0]), level=0.0, rphase=0)
        p.set(pd_ba=u(r, 0.3, 0.6)).flt(u(r, 900, 2500), u(r, 0.15, 0.35))
        p.amp(0.001, u(r, 0.25, 0.5), u(r, 0.4, 0.7), 0.1).env(1, 0.001, u(r, 0.12, 0.25), 0.0, 0.1).mod(ENV1, "pd_ba", u(r, 0.25, 0.45))
    elif kind == "acid":
        p.set(glide=u(r, 0.06, 0.12))
        p.osc("a", n=r.randint(3, 5), ru=u(r, 0.85, 0.95), rv=u(r, 0.05, 0.2), oct=-1, width=0.1, rphase=0, level=0.85)
        p.flt(u(r, 350, 700), u(r, 0.6, 0.78), u(r, 0.25, 0.45)).amp(0.001, 0.3, 0.7, 0.08)
        p.env(1, 0.001, u(r, 0.18, 0.35), 0.0, 0.1).mod(ENV1, "flt_cut", u(r, 0.5, 0.7)).mod(VEL, "flt_cut", 0.15)
        p.delay(u(r, 0.1, 0.2), 0.35, D8D)
    elif kind == "growl":
        p.osc("a", n=r.randint(5, 7), k1=patch(r, 5), ru=u(r, 0.85, 0.95), rv=u(r, 0.1, 0.3), oct=-1, width=0.15, rphase=0, level=0.85)
        p.flt(u(r, 900, 1800), u(r, 0.35, 0.55), u(r, 0.3, 0.5)).amp(0.001, 0.2, u(r, 0.5, 0.7), 0.08)
        p.lfo(1, sync=1, div=r.choice([D16, D8T, D8]), shape=r.choice([SINE, TRI, SQUARE])).mod(LFO[1], "osca_alpha", u(r, 0.3, 0.5)).mod(LFO[1], "flt_cut", u(r, 0.15, 0.3))
        p.env(1, 0.001, 0.12, 0.0, 0.1).mod(ENV1, "pd_aa", u(r, 0.3, 0.45)).set(pd_aa=u(r, 0.1, 0.25)).tape(u(r, 0.3, 0.5), 0.0)
    else:  # scan
        p.osc("a", mode=1, n=n, cover=u(r, 0.0, 0.04), pos=u(r, 0.0, 0.6), oct=-1, width=0.1, rphase=0, level=0.8)
        p.osc("b", n=2, ru=0.4, rv=0.0, oct=-2, width=0.0, rphase=0, level=u(r, 0.35, 0.5))
        p.flt(u(r, 900, 2000), u(r, 0.2, 0.4)).amp(0.002, 0.4, 0.7, 0.12)
        p.env(1, 0.001, u(r, 0.15, 0.35), 0.0, 0.2).mod(ENV1, "osca_pos", u(r, 0.15, 0.35)).mod(ENV1, "flt_cut", u(r, 0.25, 0.45))
    return p


def lead(r, i):
    p = P("Lead")
    kind = ["super", "mono", "pd", "fm", "screamer", "sync"][i % 6]
    n = r.randint(3, 7)
    p.set(poly=r.choice([1, 1, 8]), glide=u(r, 0.03, 0.12))
    if kind == "super":
        p.osc("a", n=n, k1=patch(r, n), ru=u(r, 0.75, 0.92), rv=u(r, 0.2, 0.45), ratio=r.choice([1, 2, 3]), uni=r.choice([3, 4]), det=u(r, 12, 22), width=u(r, 0.8, 1.0), level=0.6)
        p.osc("b", n=r.randint(3, 6), ru=u(r, 0.7, 0.9), rv=u(r, 0.2, 0.4), oct=r.choice([-1, 1]), uni=2, det=u(r, 8, 14), level=u(r, 0.3, 0.45))
        p.flt(u(r, 4000, 8000), u(r, 0.1, 0.25)).chorus(u(r, 0.2, 0.35))
    elif kind == "mono":
        p.set(poly=1)
        p.osc("a", n=n, ru=u(r, 0.8, 0.95), rv=u(r, 0.2, 0.4), ratio=r.choice([1, 2]), uni=2, det=u(r, 6, 12), level=0.75)
        p.osc("b", n=r.randint(3, 5), ru=u(r, 0.7, 0.9), rv=u(r, 0.2, 0.4), semi=r.choice([7, 12, -12, 5]), level=u(r, 0.25, 0.4))
        p.flt(u(r, 2500, 6000), u(r, 0.2, 0.4), u(r, 0.1, 0.3)).mod(AT, "flt_cut", u(r, 0.2, 0.35))
    elif kind == "pd":
        p.osc("a", n=n, k1=patch(r, n), ru=u(r, 0.7, 0.88), rv=u(r, 0.25, 0.45), ratio=r.choice([1, 2, 3]), level=0.8)
        p.osc("b", n=r.randint(2, 4), oct=r.choice([0, 1]), ru=u(r, 0.5, 0.7), rv=u(r, 0.1, 0.3), level=0.0)
        p.set(pd_ba=u(r, 0.3, 0.55)).flt(u(r, 5000, 9000))
        p.lfo(2, rate=u(r, 0.2, 0.6), shape=TRI).mod(LFO[2], "pd_ba", u(r, 0.2, 0.4))
    elif kind == "fm":
        p.osc("a", n=r.randint(3, 5), ru=u(r, 0.6, 0.85), rv=u(r, 0.2, 0.4), uni=2, det=u(r, 5, 10), level=0.8)
        p.osc("b", n=r.randint(2, 5), semi=r.choice([0, 7, 12, 19]), ru=u(r, 0.4, 0.7), rv=u(r, 0.1, 0.4), level=0.0)
        p.set(fm_ba=u(r, 0.15, 0.35)).flt(u(r, 4000, 8000)).mod(MW, "fm_ba", u(r, 0.3, 0.5))
        p.env(1, 0.001, u(r, 0.3, 0.6), u(r, 0.2, 0.4), 0.3).mod(ENV1, "fm_ba", u(r, 0.2, 0.35))
    elif kind == "screamer":
        p.osc("a", n=r.randint(5, 7), k1=patch(r, 5), k2=patch(r, 5), ru=u(r, 0.88, 0.96), rv=u(r, 0.4, 0.6), ratio=r.choice([2, 3, 4]), uni=3, det=u(r, 10, 18), level=0.7)
        (p.flt(u(r, 2500, 5000), u(r, 0.3, 0.5), u(r, 0.35, 0.6)) if r.random() < 0.6 else p.flt(u(r, 1200, 2200), u(r, 0.3, 0.45), u(r, 0.35, 0.6), typ=1)).tape(u(r, 0.3, 0.5), 0.05)
        p.mod(MW, "osca_rv", u(r, 0.3, 0.5)).mod(AT, "flt_res", 0.2)
    else:  # sync-ish: modulator an octave up, strong FM, filter env
        p.osc("a", n=r.randint(3, 5), ru=u(r, 0.75, 0.9), rv=u(r, 0.25, 0.4), level=0.8)
        p.osc("b", n=r.randint(3, 6), oct=1, semi=r.choice([0, 5, 7]), ru=u(r, 0.6, 0.85), rv=u(r, 0.2, 0.4), level=0.0)
        p.set(fm_ba=u(r, 0.25, 0.4), pd_aa=u(r, 0.0, 0.2)).flt(u(r, 3000, 6000), u(r, 0.15, 0.3))
        p.env(2, 0.001, u(r, 0.4, 1.0), u(r, 0.1, 0.3), 0.4).mod(ENV2, "fm_ba", u(r, 0.3, 0.5)).mod(MW, "osca_alpha", 0.4)
    if kind != "super": p.set(osca_width=u(r, 0.3, 0.55))
    p.amp(u(r, 0.002, 0.01), u(r, 0.3, 0.6), u(r, 0.75, 0.9), u(r, 0.2, 0.45))
    p.lfo(1, rate=u(r, 4.8, 6.2), fade=u(r, 0.4, 0.9), trig=1).mod(LFO[1], "osca_fine", u(r, 0.04, 0.07))
    p.delay(u(r, 0.18, 0.3), u(r, 0.3, 0.45), r.choice([D8D, D8, D4T])).reverb(u(r, 0.15, 0.28), u(r, 2.0, 3.5))
    return p


def pad(r, i):
    p = P("Pad")
    kind = ["wide", "choir", "evolve", "gated", "glass", "dark", "warm", "shimmer"][i % 8]
    oscs = "abcd"[: r.choice([2, 3, 3, 4])]
    for j, o in enumerate(oscs):
        n = r.randint(3, 7)
        p.osc(o, n=n, k1=patch(r, n), k2=patch(r, n), ru=u(r, 0.35, 0.7), rv=u(r, 0.3, 0.8), ratio=r.choice([1, 2, 2, 3]),
              uni=r.choice([2, 2, 3]) if j < 2 else 1, det=u(r, 8, 24), width=u(r, 0.7, 1.0),
              oct=[0, 0, 1, -1][j] if kind != "dark" else [0, -1, 0, -1][j], pan=[-0.4, 0.4, -0.15, 0.15][j],
              level=[0.55, 0.45, 0.3, 0.3][j], spin=u(r, -0.3, 0.3))
        p.lfo(j + 1, rate=u(r, 0.04, 0.2), shape=r.choice([SINE, TRI, SMOOTH]))
        p.mod(LFO[j + 1], "osc%s_alpha" % o, u(r, 0.12, 0.3))
        if r.random() < 0.5: p.mod(LFO[j + 1], "osc%s_rv" % o, u(r, 0.08, 0.2))
    atk = u(r, 0.6, 2.2)
    p.amp(atk, 1.0, 1.0, u(r, 2.5, 6.0))
    if kind == "choir": p.flt(u(r, 1000, 1800), u(r, 0.2, 0.35), typ=1)
    elif kind == "dark": p.flt(u(r, 700, 1500), u(r, 0.1, 0.25))
    elif kind == "glass": p.flt(u(r, 250, 600), u(r, 0.15, 0.3), typ=2)
    else: p.flt(u(r, 2200, 6000), u(r, 0.05, 0.25))
    p.lfo(5, rate=u(r, 0.05, 0.15), shape=SMOOTH).mod(LFO[5], "flt_cut", u(r, 0.1, 0.25))
    if kind == "evolve":
        p.env(1, u(r, 2, 5), u(r, 3, 6), u(r, 0.3, 0.6), 4.0).mod(ENV1, "osca_ru", u(r, 0.2, 0.35)).mod(ENV1, "flt_cut", u(r, 0.15, 0.3))
    if kind == "gated":
        p.amp(u(r, 0.05, 0.3), 1.0, 1.0, u(r, 1.0, 2.5)).gate(r.choice([0, 1, 2, 3, 4, 7]), r.choice([D16, D16, D8]), u(r, 0.85, 1.0), u(r, 0.1, 0.3))
    if kind == "shimmer": p.clouds(u(r, 0.25, 0.4), u(r, 0.5, 0.8), 0.7, 0.6, 12.0, 0.3, 0.6)
    if kind == "warm": p.tape(u(r, 0.2, 0.35), u(r, 0.15, 0.3))
    if r.random() < 0.5: p.chorus(u(r, 0.25, 0.45), u(r, 0.3, 0.8))
    if r.random() < 0.3: p.phaser(u(r, 0.2, 0.35), u(r, 0.05, 0.2))
    p.reverb(u(r, 0.35, 0.55), u(r, 4.0, 9.0), u(r, 0.8, 1.0), pre=u(r, 10, 40))
    p.mod(MW, "flt_cut", u(r, 0.2, 0.35))
    return p


def pluck(r, i):
    p = P("Pluck")
    kind = ["filter", "pd", "fm", "trance", "kalimba", "harp"][i % 6]
    n = r.randint(3, 7)
    p.osc("a", n=n, k1=patch(r, n), ru=u(r, 0.6, 0.9), rv=u(r, 0.25, 0.55), ratio=r.choice([1, 2, 3, 4]), width=u(r, 0.6, 1.0), level=0.8)
    dec = u(r, 0.25, 0.6)
    p.amp(0.001, dec, 0.0, u(r, 0.25, 0.45))
    if kind in ("filter", "trance"):
        p.flt(u(r, 400, 1200), u(r, 0.2, 0.4)).env(1, 0.001, u(r, 0.15, 0.3), 0.0, 0.2).mod(ENV1, "flt_cut", u(r, 0.45, 0.6))
        if kind == "trance":
            p.osc("b", n=r.randint(3, 5), ru=u(r, 0.7, 0.9), rv=u(r, 0.2, 0.4), uni=3, det=u(r, 10, 18), oct=r.choice([0, 1]), level=0.4, width=1.0)
            p.osc("a", uni=3, det=u(r, 10, 18))
    elif kind == "pd":
        p.osc("b", n=r.randint(2, 4), oct=r.choice([0, 1]), ru=u(r, 0.4, 0.7), rv=u(r, 0.1, 0.3), level=0.0)
        p.set(pd_ba=u(r, 0.3, 0.6)).flt(u(r, 1500, 4000), 0.2).env(1, 0.001, u(r, 0.12, 0.25), 0.0, 0.2).mod(ENV1, "pd_ba", u(r, 0.3, 0.45))
    elif kind == "fm":
        p.osc("b", n=r.randint(2, 5), oct=r.choice([0, 1, 2]), semi=r.choice([0, 7]), ru=u(r, 0.4, 0.7), rv=u(r, 0.2, 0.5), level=0.0)
        p.set(fm_ba=u(r, 0.1, 0.25), flt_on=0).env(1, 0.001, u(r, 0.1, 0.25), 0.0, 0.2).mod(ENV1, "fm_ba", u(r, 0.35, 0.55))
    elif kind == "kalimba":
        p.osc("a", ratio=r.choice([3, 4, 5]), rv=u(r, 0.6, 0.85)).osc("b", n=r.randint(4, 6), oct=2, ru=0.3, rv=0.5, level=u(r, 0.15, 0.25))
        p.flt(u(r, 3000, 6000), 0.1).env(1, 0.001, 0.08, 0.0, 0.1).mod(ENV1, "osca_rv", -u(r, 0.25, 0.4)).rings(u(r, 0.2, 0.35), 0, 0.3, 0.6, 0.6)
    else:  # harp
        p.flt(u(r, 2000, 4500), 0.15, key=0.5).env(1, 0.001, 0.4, 0.0, 0.3).mod(ENV1, "osca_ru", -u(r, 0.15, 0.3)).rings(u(r, 0.25, 0.4), 2, 0.4, 0.55, 0.7)
    p.mod(VEL, "flt_cut", u(r, 0.1, 0.2)).mod(KEY, "flt_cut", u(r, 0.1, 0.2))
    p.delay(u(r, 0.25, 0.4), u(r, 0.4, 0.55), r.choice([D8D, D8, D8T, D16])).reverb(u(r, 0.2, 0.35), u(r, 2.0, 4.0))
    return p


def keys(r, i):
    p = P("Keys")
    kind = ["organ", "ep", "clav", "mirror", "vibes"][i % 5]
    if kind == "organ":
        for o, oc, lv in (("a", 0, 0.6), ("b", 1, 0.4), ("c", 2, 0.25), ("d", -1, 0.3)):
            if o == "d" and r.random() < 0.5: continue
            p.osc(o, n=r.randint(2, 3), ru=u(r, 0.25, 0.45), rv=u(r, 0.05, 0.2), oct=oc, level=lv, rphase=0, width=u(r, 0.15, 0.35))
        p.amp(0.008, 0.2, 1.0, 0.12).set(flt_on=0).chorus(u(r, 0.35, 0.55), u(r, 3.5, 5.5), 0.4)
        if r.random() < 0.5: p.tape(0.3, 0.05)
    elif kind == "ep":
        p.osc("a", n=r.randint(2, 3), ru=u(r, 0.4, 0.6), rv=u(r, 0.05, 0.2), level=0.8, width=u(r, 0.2, 0.4))
        p.osc("b", n=2, oct=r.choice([1, 2]), ru=u(r, 0.3, 0.6), rv=0.1, level=0.0)
        p.set(fm_ba=u(r, 0.1, 0.2)).env(1, 0.001, u(r, 0.6, 1.2), 0.0, 0.5).mod(ENV1, "fm_ba", u(r, 0.25, 0.4)).mod(VEL, "fm_ba", 0.2)
        p.amp(0.002, u(r, 1.5, 2.5), u(r, 0.2, 0.35), 0.5).flt(u(r, 3000, 6000)).chorus(0.2, 0.4, 0.3)
    elif kind == "clav":
        p.osc("a", n=r.randint(3, 5), ru=u(r, 0.8, 0.92), rv=u(r, 0.3, 0.5), ratio=r.choice([2, 3]), level=0.8, width=u(r, 0.2, 0.4))
        p.flt(u(r, 1000, 2000), u(r, 0.3, 0.45)).env(1, 0.001, 0.2, 0.1, 0.1).mod(ENV1, "flt_cut", 0.45)
        p.amp(0.001, 0.8, 0.35, 0.12).phaser(0.3, 0.4)
    elif kind == "mirror":
        n = r.randint(3, 5)
        p.osc("a", n=n, k1=1, k2=1, ru=u(r, 0.4, 0.6), rv=u(r, 0.2, 0.35), width=u(r, 0.35, 0.5), pan=-0.3, level=0.6)
        p.osc("b", n=n, k1=1, k2=1, ru=u(r, 0.4, 0.6), rv=u(r, 0.2, 0.35), alpha=u(r, 3.0, 4.5), width=u(r, 0.35, 0.5), pan=0.3, level=0.6, fine=u(r, 4, 9))
        p.amp(0.005, u(r, 1.0, 1.8), u(r, 0.3, 0.45), 0.7).flt(u(r, 3000, 5000), key=0.5).env(1, 0.001, 0.6, 0.0, 0.5).mod(ENV1, "flt_cut", 0.25)
    else:  # vibes
        p.osc("a", n=r.randint(4, 6), ru=u(r, 0.3, 0.5), rv=u(r, 0.6, 0.8), ratio=4, level=0.8, width=u(r, 0.25, 0.45))
        p.osc("b", n=2, oct=2, ru=0.3, rv=0.2, level=0.0).set(fm_ba=0.08).env(1, 0.001, 0.4, 0.0, 0.4).mod(ENV1, "fm_ba", 0.3)
        p.amp(0.001, u(r, 2.0, 3.0), 0.0, 1.5).set(flt_on=0).lfo(1, rate=u(r, 4, 6)).mod(LFO[1], "master", 0.05)
    p.reverb(u(r, 0.15, 0.3), u(r, 1.8, 3.5))
    return p


def bell(r, i):
    p = P("Bell")
    n = r.randint(5, 7)
    p.osc("a", n=n, k1=patch(r, n), k2=patch(r, n), ru=u(r, 0.3, 0.55), rv=u(r, 0.7, 0.95), ratio=r.choice([3, 4, 5, 6, 7]), spin=u(r, 0.5, 3.0), width=u(r, 0.4, 0.65), level=0.75)
    p.osc("b", n=r.randint(3, 6), oct=r.choice([1, 2]), semi=r.choice([0, 0, 7, 4, 3]), fine=u(r, -15, 15), ru=u(r, 0.2, 0.5), rv=u(r, 0.3, 0.6), level=u(r, 0.0, 0.3))
    if r.random() < 0.5: p.osc("c", n=r.randint(2, 4), oct=3, fine=u(r, -30, 30), ru=0.3, rv=0.4, level=0.0).set(fm_ca=u(r, 0.05, 0.15))
    p.set(fm_ba=u(r, 0.15, 0.35), flt_on=0)
    p.amp(0.002, u(r, 2.0, 4.5), 0.0, u(r, 2.0, 3.5)).env(1, 0.001, u(r, 0.8, 1.8), 0.0, 1.0).mod(ENV1, "fm_ba", u(r, 0.3, 0.5)).mod(ENV1, "osca_rv", -u(r, 0.2, 0.35))
    p.mod(VEL, "fm_ba", 0.2)
    if r.random() < 0.4: p.rings(u(r, 0.15, 0.3), r.choice([0, 1, 4]), u(r, 0.5, 0.8), 0.6, 0.7)
    if r.random() < 0.4: p.delay(u(r, 0.15, 0.3), 0.45, r.choice([D8D, D4T]))
    p.reverb(u(r, 0.35, 0.5), u(r, 3.5, 6.0), 0.8)
    return p


def arp(r, i):
    p = P("Arp")
    n = r.randint(3, 6)
    p.osc("a", n=n, k1=patch(r, n), k2=patch(r, n), ru=u(r, 0.6, 0.85), rv=u(r, 0.3, 0.55), ratio=r.choice([1, 2, 3]), spin=u(r, 0.0, 1.8), level=0.9)
    if r.random() < 0.6: p.osc("b", n=r.randint(3, 5), ru=u(r, 0.7, 0.9), rv=u(r, 0.2, 0.4), oct=1, level=u(r, 0.3, 0.45))
    p.amp(0.001, u(r, 0.22, 0.38), u(r, 0.0, 0.2), u(r, 0.2, 0.3)).flt(u(r, 2500, 5500), u(r, 0.25, 0.45))
    p.lfo(1, sync=1, div=r.choice([D16, D8T, D8, D4]), shape=r.choice([SINE, TRI, SAWDN, SH])).mod(LFO[1], "flt_cut", u(r, 0.2, 0.35))
    p.lfo(2, sync=1, div=r.choice([D1, D2B]), shape=TRI).mod(LFO[2], "osca_alpha", u(r, 0.2, 0.4))
    p.mod(RND, "osca_rv", u(r, 0.1, 0.25)).env(1, 0.001, 0.15, 0.0, 0.15).mod(ENV1, "flt_cut", u(r, 0.2, 0.35))
    p.delay(u(r, 0.3, 0.42), u(r, 0.45, 0.6), r.choice([D8D, D8, D16, D8T])).reverb(u(r, 0.2, 0.3))
    return p


def stab(r, i):
    p = P("Stab")
    chord = [MINOR7, MAJOR7, SUS4, FIFTHS, MINOR9, MINOR][i % 6]
    n = r.randint(3, 6)
    ru, rv = u(r, 0.6, 0.85), u(r, 0.3, 0.5)
    for o, st in zip("abcd", chord):
        p.osc(o, n=n, ru=ru, rv=rv, semi=st if st <= 12 else st - 12, oct=0 if st <= 12 else 1, level=0.42, uni=r.choice([1, 2]), det=u(r, 8, 14))
    p.amp(0.001, u(r, 0.35, 0.7), 0.0, u(r, 0.3, 0.5)).flt(u(r, 700, 1500), u(r, 0.2, 0.4)).env(1, 0.001, u(r, 0.2, 0.35), 0.0, 0.3).mod(ENV1, "flt_cut", u(r, 0.45, 0.6))
    p.delay(u(r, 0.25, 0.35), 0.45, r.choice([D8D, D4T])).reverb(u(r, 0.3, 0.4), 3.0)
    return p


def drone(r, i):
    p = P("Drone")
    for j, o in enumerate("abc"[: r.choice([2, 3])]):
        n = r.randint(4, 7)
        p.osc(o, n=n, k1=patch(r, n), k2=patch(r, n), ru=u(r, 0.4, 0.8), rv=u(r, 0.4, 0.9), ratio=r.choice([2, 3, 5]),
              oct=[-2, -1, -1][j], fine=[0, 3, -4][j], spin=u(r, -0.5, 0.5), level=[0.7, 0.5, 0.35][j], width=1.0)
        p.lfo(j + 1, rate=u(r, 0.02, 0.1), shape=r.choice([SMOOTH, SINE, TRI])).mod(LFO[j + 1], "osc%s_alpha" % o, u(r, 0.25, 0.45))
    p.set(fm_ab=u(r, 0.05, 0.25), fm_ba=u(r, 0.0, 0.15))
    p.lfo(5, rate=u(r, 0.03, 0.08), shape=SMOOTH).mod(LFO[5], "fm_ab", u(r, 0.15, 0.3))
    p.amp(u(r, 2.0, 4.0), 1.0, 1.0, u(r, 5.0, 9.0)).flt(u(r, 1200, 3000), u(r, 0.1, 0.3))
    p.lfo(6, rate=u(r, 0.02, 0.06)).mod(LFO[6], "flt_cut", u(r, 0.15, 0.25))
    if r.random() < 0.5: p.clouds(u(r, 0.3, 0.45), u(r, 0.6, 0.9), 0.6, 0.6, r.choice([0.0, -12.0, 7.0]), 0.3, 0.5)
    p.reverb(u(r, 0.45, 0.6), u(r, 8.0, 14.0), 1.0)
    return p


def texture(r, i):
    p = P("Texture")
    for j, o in enumerate("abcd"[: r.choice([2, 3, 4])]):
        n = r.randint(4, 7)
        p.osc(o, n=n, k1=patch(r, n), k2=patch(r, n), ru=u(r, 0.4, 0.9), rv=u(r, 0.4, 1.0), ratio=r.randint(2, 7), spin=u(r, -2.0, 2.0),
              oct=r.choice([-1, 0, 1]), level=[0.6, 0.4, 0.3, 0.25][j], pan=u(r, -0.6, 0.6), width=1.0)
        p.lfo(j + 1, rate=u(r, 0.05, 0.6), shape=r.choice([SH, SMOOTH, TRI])).mod(LFO[j + 1], "osc%s_spin" % o, u(r, 0.1, 0.3))
    p.set(fm_ab=u(r, 0.0, 0.25), pd_ba=u(r, 0.0, 0.3))
    ft = r.choice([0, 0, 1, 2])
    p.amp(u(r, 0.8, 2.5), 1.0, 1.0, u(r, 3.0, 6.0)).flt([u(r, 2000, 6000), u(r, 900, 2200), u(r, 200, 500)][ft], u(r, 0.1, 0.3), typ=ft)
    if i % 2 == 0: p.clouds(u(r, 0.35, 0.55), u(r, 0.3, 0.8), u(r, 0.4, 0.9), u(r, 0.3, 0.8), r.choice([0.0, 12.0, -12.0, 7.0]), u(r, 0.2, 0.5), 0.5)
    else: p.rings(u(r, 0.3, 0.5), r.choice([0, 1, 2, 4, 5]), u(r, 0.3, 0.8), u(r, 0.4, 0.7), u(r, 0.5, 0.8))
    if r.random() < 0.4: p.flanger(u(r, 0.2, 0.35), u(r, 0.05, 0.2))
    p.reverb(u(r, 0.4, 0.55), u(r, 5.0, 10.0), 0.95)
    return p


def fx(r, i):
    p = P("FX")
    kind = ["riser", "down", "zap", "laser", "noise", "impact", "alarm", "space"][i % 8]
    n = r.randint(4, 7)
    p.osc("a", n=n, k1=patch(r, n), ru=u(r, 0.7, 0.95), rv=u(r, 0.5, 1.0), ratio=r.randint(3, 7), spin=u(r, 1.0, 5.0), width=1.0, level=0.8)
    if kind == "riser":
        p.amp(u(r, 2.0, 5.0), 1.0, 1.0, 2.0).env(1, u(r, 3.0, 6.0), 1.0, 1.0, 2.0).mod(ENV1, "osca_semi", u(r, 0.4, 0.5)).mod(ENV1, "osca_spin", u(r, 0.3, 0.5)).mod(ENV1, "flt_cut", 0.5)
        p.flt(500, 0.4).set(osca_uni=4, osca_det=u(r, 25, 45))
    elif kind == "down":
        p.amp(0.01, 1.0, 1.0, 3.0).env(1, 0.001, u(r, 2.0, 4.0), 0.0, 3.0).mod(ENV1, "osca_oct", u(r, 0.3, 0.4)).mod(ENV1, "flt_cut", 0.4).flt(800, 0.3)
    elif kind == "zap":
        p.set(poly=1, osca_width=0.4).amp(0.001, u(r, 0.12, 0.3), 0.0, 0.1).env(1, 0.001, u(r, 0.04, 0.1), 0.0, 0.05).mod(ENV1, "osca_oct", u(r, 0.35, 0.5)).mod(ENV1, "osca_rv", 0.4).flt(6000)
    elif kind == "laser":
        p.amp(0.001, u(r, 0.5, 1.2), 0.0, 0.5).env(1, 0.001, u(r, 0.2, 0.5), 0.0, 0.4).mod(ENV1, "osca_semi", u(r, 0.4, 0.5)).mod(ENV1, "osca_oct", 0.25)
        p.flt(5000, 0.5).delay(0.35, 0.6, D8D)
    elif kind == "noise":
        p.set(osca_uni=4, osca_det=50.0, osca_spin=u(r, 5.0, 8.0)).amp(0.4, 1.0, 1.0, 2.5).flt(u(r, 1000, 3000), 0.4, typ=1)
        p.lfo(1, rate=u(r, 0.1, 0.4), shape=SINE).mod(LFO[1], "flt_cut", 0.45)
    elif kind == "impact":
        p.set(osca_oct=-2).amp(0.001, u(r, 1.5, 3.0), 0.0, 2.0).env(1, 0.001, 0.25, 0.0, 0.2).mod(ENV1, "osca_oct", 0.35).mod(ENV1, "osca_rv", 0.4).flt(3000, 0.2).tape(0.5, 0.0)
    elif kind == "alarm":
        p.osc("a", ratio=1, spin=0.0).lfo(1, rate=u(r, 3.0, 9.0), shape=r.choice([SQUARE, TRI, SAWUP])).mod(LFO[1], "osca_semi", u(r, 0.15, 0.3)).amp(0.01, 1.0, 1.0, 0.5).flt(5000, 0.3)
    else:  # space
        p.amp(0.8, 1.0, 1.0, 4.0).lfo(1, rate=u(r, 0.2, 1.0), shape=SH).mod(LFO[1], "osca_alpha", 0.5).mod(LFO[1], "osca_semi", 0.15).flt(4000, 0.3)
        p.clouds(0.45, 0.7, 0.6, 0.7, 12.0, 0.4, 0.6)
    p.reverb(u(r, 0.35, 0.5), u(r, 4.0, 8.0), 0.9)
    return p


def scan(r, i):
    p = P("Scan")
    kind = ["sweep", "pad", "pluck", "lead", "full", "bass"][i % 6]
    n = r.randint(3, 7)
    if kind == "sweep":
        p.osc("a", mode=1, n=n, cover=u(r, 0.0, 0.05), pos=u(r, 0.0, 0.3), uni=r.choice([1, 2, 3]), det=u(r, 8, 15), width=0.8, level=0.75)
        p.lfo(1, rate=u(r, 0.03, 0.12), shape=r.choice([TRI, SINE])).mod(LFO[1], "osca_pos", u(r, 0.35, 0.5))
        p.amp(0.05, 1.0, 0.9, 1.2).flt(u(r, 4000, 8000)).reverb(0.3, 4.0)
    elif kind == "pad":
        p.osc("a", mode=1, n=n, cover=u(r, 0.05, 0.3), pos=u(r, 0.0, 1.0), uni=3, det=u(r, 12, 20), width=1.0, level=0.55, pan=-0.3)
        p.osc("b", mode=1, n=r.randint(3, 7), cover=u(r, 0.05, 0.3), pos=u(r, 0.0, 1.0), uni=2, det=u(r, 10, 15), oct=1, width=1.0, level=0.35, pan=0.3)
        p.lfo(1, rate=u(r, 0.02, 0.08), shape=SMOOTH).mod(LFO[1], "osca_pos", 0.4).lfo(2, rate=u(r, 0.03, 0.1)).mod(LFO[2], "oscb_pos", 0.4)
        p.amp(u(r, 0.8, 2.0), 1.0, 1.0, u(r, 3.0, 5.0)).flt(u(r, 2500, 5000), 0.15).chorus(0.3).reverb(u(r, 0.4, 0.55), u(r, 5.0, 8.0), 0.9)
    elif kind == "pluck":
        p.osc("a", mode=1, n=n, cover=u(r, 0.0, 0.1), pos=u(r, 0.0, 1.0), width=0.7, level=0.85)
        p.amp(0.001, u(r, 0.4, 0.7), 0.0, 0.4).env(1, 0.001, u(r, 0.2, 0.4), 0.0, 0.3).mod(ENV1, "osca_cover", u(r, 0.1, 0.25)).mod(ENV1, "flt_cut", 0.35)
        p.flt(u(r, 1500, 4000), 0.25).delay(u(r, 0.25, 0.35), 0.45, r.choice([D8D, D8])).reverb(0.2)
    elif kind == "lead":
        p.set(poly=1, glide=u(r, 0.04, 0.1))
        p.osc("a", mode=1, n=n, cover=u(r, 0.0, 0.03), pos=u(r, 0.0, 1.0), uni=2, det=u(r, 8, 12), level=0.8)
        p.mod(MW, "osca_pos", u(r, 0.3, 0.5)).lfo(1, rate=5.5, fade=0.6, trig=1).mod(LFO[1], "osca_fine", 0.05)
        p.amp(0.005, 0.4, 0.85, 0.3).flt(u(r, 4000, 7000), 0.2).delay(0.22, 0.4).reverb(0.2)
    elif kind == "full":
        p.osc("a", mode=1, n=n, cover=u(r, 0.7, 1.0), oct=r.choice([-2, -1]), width=1.0, level=0.7)
        p.osc("b", n=r.randint(2, 4), ru=0.5, rv=0.3, oct=-1, level=0.4)
        p.lfo(1, rate=u(r, 0.02, 0.06), shape=SMOOTH).mod(LFO[1], "osca_alpha", 0.3).mod(LFO[1], "osca_cover", -0.15)
        p.amp(u(r, 1.5, 3.0), 1.0, 1.0, u(r, 4.0, 6.0)).flt(u(r, 1500, 3000), 0.2).reverb(u(r, 0.4, 0.5), u(r, 6.0, 10.0), 0.95)
    else:  # bass
        p.set(poly=1, glide=u(r, 0.0, 0.05))
        p.osc("a", mode=1, n=n, cover=u(r, 0.0, 0.02), pos=u(r, 0.0, 0.7), oct=-1, width=0.1, rphase=0, level=0.8)
        p.osc("b", n=2, ru=0.4, rv=0.0, oct=-2, width=0.0, rphase=0, level=0.45)
        p.flt(u(r, 900, 1800), u(r, 0.25, 0.4)).amp(0.002, 0.35, 0.6, 0.1).env(1, 0.001, u(r, 0.15, 0.3), 0.0, 0.2).mod(ENV1, "osca_pos", u(r, 0.2, 0.35)).mod(ENV1, "flt_cut", 0.35)
    return p


# ---------------------------------------------------------------------------------------------
ADJ = ["Quintic", "Ricci", "Hodge", "Mirror", "Toric", "Conifold", "Orbifold", "Fano", "Elliptic", "Holomorphic", "Symplectic",
       "Hyperbolic", "Tachyon", "Bosonic", "Fermionic", "Graviton", "Dilaton", "Axion", "Moduli", "Instanton", "Monopole", "Gauge",
       "Brane", "Flux", "Planck", "Compact", "Twisted", "Folded", "Kahler", "Riemann", "Spinor", "Tensor", "Vector", "Scalar",
       "Quantum", "Chiral", "Dual", "Hidden", "Warped", "Curved", "Torsion", "Lattice", "Fractal", "Ergodic", "Chaotic", "Holonomy",
       "Cohomology", "Homotopy", "Manifold", "Topology", "Singular", "Resonant", "Spectral", "Harmonic", "Nebular", "Stellar",
       "Pulsar", "Quasar", "Neutron", "Photon", "Gluon", "Lepton", "Muon", "Hadron", "Plasma", "Ion", "Cosmic", "Galactic", "Lunar",
       "Solar", "Dark", "Event", "Zero", "Infinite", "Sixfold", "Ninefold", "Eleventh", "Tenth", "Seventh", "Higher", "Inner", "Outer"]
NOUNS = {
    "Bass": ["Engine", "Pressure", "Rumble", "Core", "Roller", "Grind", "Pulse", "Undertow", "Weight", "Gravity", "Drive", "Bite",
             "Motor", "Furnace", "Piston", "Torque", "Thrust", "Anchor", "Bedrock", "Low End", "Roll", "Knock", "Growl", "Reactor"],
    "Lead": ["Runner", "Scream", "Voice", "Beam", "Blade", "Arc", "Flare", "Comet", "Signal", "Sabre", "Siren", "Line", "Hook",
             "Lance", "Spark", "Trail", "Jet", "Bolt", "Glider", "Hunter"],
    "Pad": ["Bloom", "Landscape", "Cathedral", "Horizon", "Mist", "Aurora", "Veil", "Ocean", "Haze", "Garden", "Halo", "Dawn",
            "Dusk", "Field", "Cloud", "Sky", "Expanse", "Tide", "Breath", "Glow", "Silk", "Velvet", "Ether", "Drift"],
    "Pluck": ["Diamond", "Droplet", "Spark", "Pick", "Pebble", "Crystal", "Shard", "Seed", "Tick", "Ripple", "Bead", "Petal",
              "Glint", "Flick", "Grain", "Prism"],
    "Keys": ["Organ", "Piano", "Tines", "Clavier", "Console", "Keys", "Manual", "Harmonium", "Celeste", "Board"],
    "Bell": ["Bell", "Chime", "Gong", "Carillon", "Tower", "Ring", "Toll", "Temple", "Glass", "Bowl"],
    "Arp": ["Network", "Circuit", "Sequence", "Lattice", "Matrix", "Ladder", "Spiral", "Cascade", "Loop", "Pattern", "Orbit", "Chain"],
    "Stab": ["Chord", "Hit", "Stab", "Blast", "Strike", "Punch", "Jab", "Shot"],
    "Drone": ["Abyss", "Void", "Monolith", "Ocean", "Cavern", "Engine", "Hum", "Throne", "Deep", "Shrine", "Well", "Eon"],
    "Texture": ["Space", "Dust", "Foam", "Weave", "Grain", "Static", "Swarm", "Mesh", "Fabric", "Sand", "Rain", "Fog"],
    "FX": ["Sweep", "Riser", "Fall", "Zap", "Laser", "Wash", "Impact", "Alarm", "Warp", "Jump", "Portal", "Launch", "Descent", "Transmission"],
    "Scan": ["Sweep", "Path", "Scanline", "Traverse", "Raster", "Atlas", "Cartography", "Survey", "Coverage", "Serpent", "Fold", "Map"],
}
PLAN = [("Bass", bass, 45), ("Lead", lead, 30), ("Pad", pad, 40), ("Pluck", pluck, 30), ("Keys", keys, 20), ("Bell", bell, 20),
        ("Arp", arp, 20), ("Stab", stab, 15), ("Drone", drone, 20), ("Texture", texture, 20), ("FX", fx, 20), ("Scan", scan, 20)]

EXISTING = {"Pad - Quintic Bloom", "Bass - Fermat Cubic", "Lead - Brane Runner", "Bell - Compactified", "Pluck - Kahler",
            "Keys - Mirror Symmetry", "Bass - Psy Brane Growl", "Pad - String Landscape", "Pluck - Hodge Diamond", "Drone - Orbifold",
            "FX - Tachyon Sweep", "Keys - Moduli Organ", "Bass - Calabi Sub", "Lead - Wormhole PD", "Pad - Hyperspace Choir",
            "Bass - Kaluza-Klein", "Stab - Singularity Chord", "FX - Dimension Seven", "Texture - Fold Space", "Lead - Event Horizon",
            "Arp - Spin Network", "Scan - Manifold Sweep", "Scan - Full Coverage Drone", "Scan - Coverage Pluck", "Scan - Wavetable Bass"}


def make_bank():
    """Names: Category - Adjective Noun, every adjective used once per category, never clashing with the original 25."""
    bank, used = [], set(EXISTING)
    for ci, (cat, fn, count) in enumerate(PLAN):
        r = random.Random(1000 + ci)
        adj, nouns = ADJ[:], NOUNS[cat][:]
        r.shuffle(adj)
        taken = {x.split(" - ")[1].split()[0] for x in used if x.startswith(cat + " - ")}
        adj = [a for a in adj if a not in taken]
        assert len(adj) >= count, cat
        for i in range(count):
            name = "%s - %s %s" % (cat, adj[i], nouns[(i * 7 + ci) % len(nouns)])
            assert name not in used
            used.add(name)
            bank.append((name, fn(random.Random(ci * 7919 + i * 104729), i)))
    return bank


def fmt(v):
    if isinstance(v, int): return "%d" % v
    s = ("%.4f" % v).rstrip("0").rstrip(".")
    return s + ("" if "." in s else ".0") + "f" if s not in ("0", "-0") else "0.0f"


def main():
    calibrate = os.environ.get("CYR_CALIBRATE") == "1"       # every preset at -6 dB, no limiter: for match_loudness.py
    trims = json.load(open(TRIMS)) if os.path.exists(TRIMS) and not calibrate else {}
    bank = make_bank()
    lines = ["// Generated by tools/gen_presets.py - do not edit by hand.", "// %d presets." % len(bank)]
    for name, p in bank:
        kv = dict(p.kv)
        t = trims.get(name, -6.0)
        if isinstance(t, dict):
            kv["master"] = t["master"]
            kv.update(mb_lm_on=1, mb_lm_in=t.get("lm", 0.0), mb_lm_ceil=-1.0)   # MIXBUS limiter: makeup and peak safety
        else:
            kv["master"] = t
        body = ", ".join("{ \"%s\", %s }" % (k, fmt(float(v)) if isinstance(v, float) else fmt(v)) for k, v in kv.items())
        mods = "".join(".mod (%s, \"%s\", %s)" % (s, d, fmt(float(a))) for s, d, a in p.mods)
        lines.append("{ Build b (\"%s\"); b.set ({ %s })%s; add (b); }" % (name, body, mods))
    open(OUT, "w").write("\n".join(lines) + "\n")
    print("wrote", len(bank), "presets ->", OUT, "(trims:", len(trims), ")")


if __name__ == "__main__":
    main()
