#include "RSynth.h"
#include <algorithm>

namespace cyr
{
using pg::clampf;

static inline float toNorm (int rid, float v) { return ranges()[(size_t) rid].convertTo0to1 (v); }
static inline float fromNorm (int rid, float n) { return ranges()[(size_t) rid].convertFrom0to1 (n < 0.0f ? 0.0f : (n > 1.0f ? 1.0f : n)); }

// =============================================================================
// Voice
// =============================================================================
void RVoice::prepare (double sampleRate)
{
    sr = sampleRate;
    amp.setSampleRate ((float) sr);
    for (auto& e : env) e.setSampleRate ((float) sr);
    filter.reset();
    dcCoef = 1.0f - std::exp (-kTwoPiF * 12.0f / (float) sr);
    liveNorm.fill (-1.0f);
    xfLen = juce::jmax (16, (int) (sr * 0.01));
    gainK = 1.0f - std::exp (-(float) kCtrl / (float) (sr * 0.015));
    smReady = false;
}

// parameters that glide (everything continuous, plus octave / semitone / bend so pitch steps slide in 12 ms)
static const std::vector<char>& glideMask()
{
    static const std::vector<char> m = []
    {
        std::vector<char> v ((size_t) kNumR, 0);
        for (int i = 0; i < kNumR; ++i)
        {
            const auto& inf = infos()[(size_t) i];
            const bool pitchInt = inf.id.endsWith ("_oct") || inf.id.endsWith ("_semi") || inf.id == "bend";
            v[(size_t) i] = (! inf.isChoice && ! inf.isBool && (! inf.isInt || pitchInt)) ? 1 : 0;
        }
        return v;
    }();
    return m;
}

void RVoice::start (int nt, float velocity, float fromNote, const float* p, uint64_t ord, uint32_t seed)
{
    const bool wasActive = isActive();
    note = nt;
    order = ord;
    vel = velocity;
    held = true;
    targetPitch = (float) nt;
    pitch = (fromNote >= 0.0f && p[glide] > 0.0005f) ? fromNote : targetPitch;
    sinceOn = 0.0f;
    juce::Random r ((juce::int64) seed);
    randomVal = r.nextFloat() * 2.0f - 1.0f;
    for (int o = 0; o < kOscs; ++o)
    {
        auto& s = osc[o];
        const bool rph = p[osca_rphase + o * kOscStride] > 0.5f;
        for (int l = 0; l < kMaxUni; ++l) s.ph[l] = rph ? r.nextFloat() : 0.0f;
        if (! wasActive) { s.dcL = s.dcR = 0.0f; s.spin = 0.0f; s.first = true; prevOut[o] = 0.0f; }
    }
    for (int l = 0; l < kLfos; ++l)
        if (p[lfo1_trig + l * kLfoStride] > 0.5f) lfo[l].reset (p[lfo1_phase + l * kLfoStride]);
    if (! wasActive) { filter.reset(); filterOld.reset(); fltXf = 0; fltType = -1; smReady = false; ampVelReady = false; }
    lastSet.fill (-1.0f);
    amp.gateOn (! wasActive);
    for (auto& e : env) e.gateOn (true);
    ctrlLeft = 0;
}

void RVoice::release()
{
    held = false;
    amp.gateOff();
    for (auto& e : env) e.gateOff();
}

void RVoice::setupOsc (int o, const float* e, const RGlobal& g, bool smoothGain)
{
    auto& s = osc[o];
    const int b = o * kOscStride;
    Cfg c;
    c.on = e[osca_on + b] > 0.5f;
    c.mode = (int) e[osca_mode + b];
    const int n = juce::jlimit (2, 7, (int) std::lround (e[osca_n + b]));
    const int k1 = (int) std::lround (e[osca_k1 + b]) % n, k2 = (int) std::lround (e[osca_k2 + b]) % n;
    auto& sh = c.shape;
    sh.pw = 2.0f / (float) n;
    sh.k1 = kTwoPiF * (float) k1 / (float) n;
    sh.k2 = kTwoPiF * (float) k2 / (float) n;
    sh.ruA = e[osca_ru + b] * 0.78539816f;
    sh.rvA = e[osca_rv + b] * kHalfPiF;
    sh.ratio = (float) juce::jlimit (1, 7, (int) std::lround (e[osca_ratio + b]));
    const float a = e[osca_alpha + b];
    s.alpha = s.first ? a : s.alpha + (a - s.alpha) * 0.35f;
    sh.ca = fast::cosR (s.alpha);
    sh.sa = fast::sinR (s.alpha);
    sh.width = clampf (e[osca_width + b], 0.0f, 1.0f);
    c.scan = makeScan (n, e[osca_cover + b], e[osca_pos + b]);
    c.uni = juce::jlimit (1, kMaxUni, (int) std::lround (e[osca_uni + b]));
    const float base = pitch + 12.0f * e[osca_oct + b] + e[osca_semi + b] + e[osca_fine + b] * 0.01f + g.pitchBend * e[bend];
    const float det = e[osca_det + b];
    for (int l = 0; l < c.uni; ++l)
    {
        const float spread = c.uni > 1 ? ((float) l / (float) (c.uni - 1) - 0.5f) * det * 0.01f : 0.0f;
        c.inc[l] = pg::midiToHz (base + spread) / (float) sr;
        c.layerOff[l] = (float) l * 0.7f;
    }
    const int key[9] = { c.on ? 1 : 0, c.mode, n, k1, k2, (int) sh.ratio, c.uni,
                         c.mode == 1 ? c.scan.count : 0, c.mode == 1 ? (int) std::floor (c.scan.start) : 0 };
    std::copy (key, key + 9, c.key);
    const bool discrete = ! s.first && ! std::equal (key, key + 9, s.cur.key);
    // loudness match: a discrete change takes the new value at once (the old setting keeps its own while
    // they crossfade); continuous moves glide over ~15 ms
    const float target = g.oscGain[o];
    if (s.first || (discrete && s.xf == 0)) s.gainSm = target;
    else if (smoothGain) s.gainSm += (target - s.gainSm) * gainK;
    c.norm = s.gainSm / std::sqrt ((float) c.uni);

    auto glideTo = [] (Cfg& cur, const Cfg& t)
    {
        const float k = 1.0f / (float) kCtrl;
        cur.dRu = (t.shape.ruA - cur.shape.ruA) * k;
        cur.dRv = (t.shape.rvA - cur.shape.rvA) * k;
        cur.dCa = (t.shape.ca - cur.shape.ca) * k;
        cur.dSa = (t.shape.sa - cur.shape.sa) * k;
        cur.dW = (t.shape.width - cur.shape.width) * k;
        cur.dNorm = (t.norm - cur.norm) * k;
        cur.dStart = (t.scan.start - cur.scan.start) * k;
        for (int l = 0; l < cur.uni; ++l) cur.dInc[l] = (t.inc[juce::jmin (l, t.uni - 1)] - cur.inc[l]) * k;
    };
    if (s.first) { s.cur = c; s.cur.freeze(); s.xf = 0; }
    else if (discrete && s.xf == 0) { s.old = s.cur; s.old.freeze(); s.cur = c; s.cur.freeze(); s.xf = xfLen; }
    else glideTo (s.cur, c);   // continuous move, or a discrete one waiting for the running fade to finish
    s.first = false;

    s.spinInc = e[osca_spin + b] * kTwoPiF / (float) sr;
    float pl, pr;
    pg::panGains (e[osca_pan + b], pl, pr);
    const float lvl = e[osca_level + b] * 1.4142f;
    // ramp level / pan to the new value across this control block
    s.dgL = (lvl * pl - s.gL) / (float) kCtrl;
    s.dgR = (lvl * pr - s.gR) / (float) kCtrl;
}

void RVoice::control (const float* p, const RGlobal& g)
{
    const float dt = (float) kCtrl / (float) sr;
    sinceOn += dt;
    // glide
    if (p[glide] > 0.0005f) { const float c = std::exp (-dt / (p[glide] * 0.35f)); pitch = targetPitch + (pitch - targetPitch) * c; }
    else pitch = targetPitch;

    // ---- sources ----
    srcVal[MS_None] = 0.0f;
    for (int i = 0; i < kEnvs; ++i) srcVal[MS_Env1 + i] = env[i].current();
    for (int l = 0; l < kLfos; ++l)
    {
        const int b = l * kLfoStride;
        float v;
        if (p[lfo1_trig + b] > 0.5f)
        {
            float rate = eff[lfo1_rate + b];
            if (p[lfo1_sync + b] > 0.5f) rate = (float) (g.bpm / 60.0 / choices::divisionInBeats ((int) p[lfo1_div + b]));
            v = lfo[l].advance ((int) p[lfo1_shape + b], rate * dt);
        }
        else v = g.lfo[l];
        const float fade = p[lfo1_fade + b];
        if (fade > 0.001f) v *= clampf (sinceOn / fade, 0.0f, 1.0f);
        srcVal[MS_Lfo1 + l] = v;
    }
    srcVal[MS_AmpEnv] = amp.current();
    srcVal[MS_Velocity] = vel;
    srcVal[MS_Key] = clampf (((float) note - 60.0f) / 36.0f, -1.0f, 1.0f);
    srcVal[MS_ModWheel] = g.modWheel;
    srcVal[MS_Aftertouch] = g.aftertouch;
    srcVal[MS_PitchBend] = g.pitchBend;
    srcVal[MS_Random] = randomVal;

    // ---- knob values glide (12 ms), discrete ones switch and crossfade in setupOsc ----
    {
        const auto& mask = glideMask();
        const float k = smReady ? 1.0f - std::exp (-dt / 0.012f) : 1.0f;
        for (int i = 0; i < kNumR; ++i)
            sm[(size_t) i] = mask[(size_t) i] ? sm[(size_t) i] + (p[i] - sm[(size_t) i]) * k : p[i];
        smReady = true;
    }
    // ---- modulation slots: offsets on the 0..1 scale of each destination ----
    std::copy (sm.begin(), sm.end(), eff.begin());
    const int nd = numDests();
    int touched[kSlots], nt = 0;
    for (int s = 0; s < kSlots; ++s)
    {
        const int src = (int) p[slotSrc (s)], dst = (int) p[slotDst (s)];
        if (src <= 0 || dst <= 0 || dst >= nd) continue;
        const float amt = p[slotAmt (s)];
        if (amt == 0.0f) continue;
        bool seen = false;
        for (int k = 0; k < nt; ++k) seen |= touched[k] == dst;
        if (! seen) { touched[nt++] = dst; offs[(size_t) dst] = 0.0f; }
        offs[(size_t) dst] += srcVal[src] * amt;
    }
    for (int k = 0; k < prevCount; ++k) liveNorm[(size_t) prevTouched[k]] = -1.0f;
    for (int k = 0; k < nt; ++k) prevTouched[k] = touched[k];
    prevCount = nt;
    const auto& dp = modDestParams();
    for (int k = 0; k < nt; ++k)
    {
        const int d = touched[k], rid = dp[(size_t) d - 1];
        const float nv = clampf (toNorm (rid, sm[(size_t) rid]) + offs[(size_t) d], 0.0f, 1.0f);
        eff[(size_t) rid] = fromNorm (rid, nv);
        liveNorm[(size_t) d] = nv;
    }
    const float* e = eff.data();

    // ---- envelopes (recomputed only when a setting moved) ----
    {
        bool changed = false;
        for (int k = 0; k < 4; ++k) changed |= lastSet[(size_t) k] != e[amp_a + k];
        if (changed) { amp.set (e[amp_a], e[amp_d], e[amp_s], e[amp_r]); for (int k = 0; k < 4; ++k) lastSet[(size_t) k] = e[amp_a + k]; }
        for (int i = 0; i < kEnvs; ++i)
        {
            const int b = i * kEnvStride;
            bool ch = false;
            for (int k = 0; k < 4; ++k) ch |= lastSet[(size_t) (4 + i * 4 + k)] != e[env1_a + b + k];
            if (ch)
            {
                env[i].set (e[env1_a + b], e[env1_d + b], e[env1_s + b], e[env1_r + b]);
                for (int k = 0; k < 4; ++k) lastSet[(size_t) (4 + i * 4 + k)] = e[env1_a + b + k];
            }
        }
    }

    // ---- oscillators and matrices ----
    for (int o = 0; o < kOscs; ++o) setupOsc (o, e, g, true);
    for (int s = 0; s < kOscs; ++s)
        for (int d = 0; d < kOscs; ++d)
        {
            const float f = e[fm_aa + s * 4 + d];
            fm[s][d] = f * f * 1.5f;          // cycles of phase offset per unit of source
            pd[s][d] = e[pd_aa + s * 4 + d];
        }

    // ---- filter: on / off fades the wet signal, a type change crossfades from the old type ----
    {
        fltMixTarget = e[flt_on] > 0.5f ? 1.0f : 0.0f;
        if (fltType < 0) { fltMix = fltMixTarget; fltType = (int) e[flt_type]; }
        const int type = (int) e[flt_type];
        if (type != fltType && fltXf == 0) { filterOld = filter; fltXf = xfLen; fltType = type; }
        const float cut = clampf (e[flt_cut] * std::exp2 ((pitch - 60.0f) / 12.0f * e[flt_key]), 20.0f, 20000.0f);
        if (cut != lastSet[20] || e[flt_res] != lastSet[21] || e[flt_drive] != lastSet[22] || (float) fltType != lastSet[23])
        {
            filter.set (0, fltType, false, cut, e[flt_res], e[flt_drive], (float) sr);
            lastSet[20] = cut; lastSet[21] = e[flt_res]; lastSet[22] = e[flt_drive]; lastSet[23] = (float) fltType;
        }
    }
    const float vs = e[velsens];
    ampVelTarget = 1.0f - vs + vs * vel;
    if (! ampVelReady) { ampVel = ampVelTarget; ampVelReady = true; }
    outGain = 0.8f;
}

void RVoice::renderCfg (const Cfg& c, Osc& s, float fmOff, float knee, float& L, float& R) const
{
    L = R = 0.0f;
    if (! c.on) return;
    for (int l = 0; l < c.uni; ++l)
    {
        float ph = s.ph[l] + fmOff;
        ph -= fast::floorf (ph);
        if (knee != 0.5f) ph = pdWarp (ph, knee);
        float ol, orr;
        if (c.mode == 1) scanSample (c.shape, c.scan, ph, s.spin, ol, orr);
        else orbitSample (c.shape, ph, c.layerOff[l], s.spin, ol, orr);
        L += ol; R += orr;
    }
    L *= c.norm;
    R *= c.norm;
}

void RVoice::render (float* L, float* R, int n, const float* p, const RGlobal& g)
{
    const float fltStep = 1.0f / (float) xfLen;
    for (int i = 0; i < n; ++i)
    {
        if (ctrlLeft <= 0) { control (p, g); ctrlLeft = kCtrl; }
        --ctrlLeft;
        const float a = amp.next();
        for (auto& en : env) en.next();
        if (! amp.active()) { note = -1; return; }
        ampVel += (ampVelTarget - ampVel) * 0.002f;

        float vl = 0.0f, vr = 0.0f;
        float out[kOscs] {};
        for (int o = 0; o < kOscs; ++o)
        {
            auto& s = osc[o];
            s.gL += s.dgL;
            s.gR += s.dgR;
            if (! s.cur.on && s.xf == 0) continue;
            float fmOff = 0.0f, pdk = 0.0f;
            for (int src = 0; src < kOscs; ++src) { fmOff += fm[src][o] * prevOut[src]; pdk += pd[src][o] * prevOut[src]; }
            const float knee = 0.5f + 0.47f * clampf (pdk, -1.0f, 1.0f);
            s.spin += s.spinInc;
            if (s.spin > kTwoPiF) s.spin -= kTwoPiF; else if (s.spin < 0.0f) s.spin += kTwoPiF;
            const int layers = s.xf > 0 ? juce::jmax (s.cur.uni, s.old.uni) : s.cur.uni;
            for (int l = 0; l < layers; ++l)
            {
                s.ph[l] += s.cur.inc[juce::jmin (l, s.cur.uni - 1)];
                s.ph[l] -= fast::floorf (s.ph[l]);
            }
            float xl, xr;
            renderCfg (s.cur, s, fmOff, knee, xl, xr);
            s.cur.step();
            if (s.xf > 0)
            {
                float ol, orr;
                renderCfg (s.old, s, fmOff, knee, ol, orr);
                const float w = (float) s.xf / (float) xfLen;   // weight of the old setting
                xl += (ol - xl) * w;
                xr += (orr - xr) * w;
                --s.xf;
            }
            // DC blocker, then level / pan
            s.dcL += (xl - s.dcL) * dcCoef;
            s.dcR += (xr - s.dcR) * dcCoef;
            xl -= s.dcL; xr -= s.dcR;
            out[o] = 0.5f * (xl + xr);
            vl += xl * s.gL;
            vr += xr * s.gR;
        }
        for (int o = 0; o < kOscs; ++o) prevOut[o] = clampf (out[o], -1.5f, 1.5f);

        // filter with crossfades
        if (fltMix > 0.0f || fltMixTarget > 0.0f)
        {
            float fl = filter.tick (0, vl), fr = filter.tick (1, vr);
            if (fltXf > 0)
            {
                const float w = (float) fltXf / (float) xfLen;
                const float ol = filterOld.tick (0, vl), orr = filterOld.tick (1, vr);
                fl += (ol - fl) * w;
                fr += (orr - fr) * w;
                --fltXf;
            }
            fltMix += fltMixTarget > fltMix ? juce::jmin (fltStep, fltMixTarget - fltMix) : -juce::jmin (fltStep, fltMix - fltMixTarget);
            vl += (fl - vl) * fltMix;
            vr += (fr - vr) * fltMix;
        }
        const float gain = a * ampVel * outGain;
        L[i] += vl * gain;
        R[i] += vr * gain;
    }
    for (int o = 0; o < kOscs; ++o) orbitPhase[o] = osc[o].ph[0];
}

// =============================================================================
// Synth
// =============================================================================
void RSynth::prepare (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& v : voices) { v.prepare (sr); v.kill(); }
    heldStack.clear();
    sustained.clear();
    for (auto& a : liveNorm) a.store (-1.0f);
    for (auto& k : loudKey) k.fill (-999.0f);
}

void RSynth::allNotesOff()
{
    for (auto& v : voices) v.release();
    heldStack.clear();
}

void RSynth::noteOn (int note, float vel, const float* p)
{
    const int maxV = juce::jlimit (1, kMaxVoices, (int) p[poly]);
    RVoice* target = nullptr;
    // same note still sounding: retrigger it
    for (auto& v : voices) if (v.isActive() && v.note == note) { target = &v; break; }
    int activeCount = 0;
    for (auto& v : voices) activeCount += v.isActive() ? 1 : 0;
    if (target == nullptr && activeCount < maxV)
        for (auto& v : voices) if (! v.isActive()) { target = &v; break; }
    if (target == nullptr)
    {
        // steal: the oldest released voice, else the oldest
        uint64_t best = UINT64_MAX;
        for (auto& v : voices)
            if (v.isActive() && ! v.isHeld() && v.order < best) { best = v.order; target = &v; }
        if (target == nullptr)
            for (auto& v : voices)
                if (v.isActive() && v.order < best) { best = v.order; target = &v; }
        if (target == nullptr) target = &voices[0];
    }
    target->start (note, vel, lastPitch, p, ++orderCounter, (uint32_t) rnd.nextInt());
    lastPitch = (float) note;
    lastNote.store (note);
    noteOnCounter.fetch_add (1);
    heldStack.push_back (note);
}

void RSynth::noteOff (int note)
{
    heldStack.erase (std::remove (heldStack.begin(), heldStack.end(), note), heldStack.end());
    if (sustain) { sustained.push_back (note); return; }
    for (auto& v : voices) if (v.isActive() && v.isHeld() && v.note == note) v.release();
}

void RSynth::handleMidi (const juce::MidiMessage& m, const float* p)
{
    if (m.isNoteOn()) noteOn (m.getNoteNumber(), m.getFloatVelocity(), p);
    else if (m.isNoteOff()) noteOff (m.getNoteNumber());
    else if (m.isPitchWheel()) global.pitchBend = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
    else if (m.isChannelPressure()) global.aftertouch = (float) m.getChannelPressureValue() / 127.0f;
    else if (m.isAftertouch()) global.aftertouch = (float) m.getAfterTouchValue() / 127.0f;
    else if (m.isController())
    {
        if (m.getControllerNumber() == 1) global.modWheel = (float) m.getControllerValue() / 127.0f;
        else if (m.getControllerNumber() == 64)
        {
            sustain = m.getControllerValue() >= 64;
            if (! sustain)
            {
                for (int nt : sustained)
                    if (std::find (heldStack.begin(), heldStack.end(), nt) == heldStack.end())
                        for (auto& v : voices) if (v.isActive() && v.note == nt) v.release();
                sustained.clear();
            }
        }
        else if (m.getControllerNumber() == 123) allNotesOff();
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff()) allNotesOff();
}

void RSynth::updateGlobal (const float* p, int n, const juce::AudioPlayHead::PositionInfo* pos)
{
    if (pos != nullptr)
    {
        if (auto b = pos->getBpm()) global.bpm = *b;
        global.playing = pos->getIsPlaying();
        if (auto q = pos->getPpqPosition()) global.beat = *q;
    }
    const float dt = (float) n / (float) sr;
    for (int l = 0; l < kLfos; ++l)
    {
        const int b = l * kLfoStride;
        const int shape = (int) p[lfo1_shape + b];
        if (p[lfo1_sync + b] > 0.5f)
        {
            const double beats = choices::divisionInBeats ((int) p[lfo1_div + b]);
            if (global.playing)
            {
                double ph = global.beat / beats + p[lfo1_phase + b];
                ph -= std::floor (ph);
                const float prev = globalLfo[l].phase;
                globalLfo[l].phase = (float) ph;
                if ((float) ph < prev) globalLfo[l].advance (shape, 0.0f + 1.0f);   // new cycle: refresh random shapes
                globalLfo[l].phase = (float) ph;
                global.lfo[l] = globalLfo[l].shapeAt (shape, (float) ph);
            }
            else
                global.lfo[l] = globalLfo[l].advance (shape, (float) (global.bpm / 60.0 / beats) * dt);
        }
        else
            global.lfo[l] = globalLfo[l].advance (shape, p[lfo1_rate + b] * dt);
        global.lfoPhase[l] = globalLfo[l].phase;
    }
    // free-running clock when the host is stopped (standalone, stopped DAW): the trance gate and synced
    // effects keep moving instead of freezing on one step; while playing the host position resets it each block
    global.beat += global.bpm / 60.0 * dt;
}

// Peak of one mean-removed orbit cycle, so every surface shape plays at a similar level
void RSynth::updateLoudness (const float* p)
{
    for (int o = 0; o < kOscs; ++o)
    {
        const int b = o * kOscStride;
        std::array<float, 16> key { p[osca_n + b], p[osca_k1 + b], p[osca_k2 + b], p[osca_alpha + b], p[osca_ru + b],
                                    p[osca_rv + b], p[osca_ratio + b], p[osca_width + b], p[osca_mode + b], p[osca_cover + b], p[osca_pos + b] };
        if (key == loudKey[o]) continue;
        loudKey[o] = key;
        const int nn = juce::jlimit (2, 7, (int) std::lround (key[0]));
        OrbitShape s;
        s.pw = 2.0f / (float) nn;
        s.k1 = kTwoPiF * (float) ((int) std::lround (key[1]) % nn) / (float) nn;
        s.k2 = kTwoPiF * (float) ((int) std::lround (key[2]) % nn) / (float) nn;
        s.ca = std::cos (key[3]); s.sa = std::sin (key[3]);
        s.ruA = key[4] * 0.78539816f; s.rvA = key[5] * kHalfPiF; s.ratio = (float) juce::jlimit (1, 7, (int) std::lround (key[6]));
        s.width = key[7];
        const bool scan = key[8] > 0.5f;
        const auto sc = makeScan (nn, key[9], key[10]);
        const int M = scan ? juce::jlimit (256, kLoudMax, sc.count * 8) : 256;
        float* a = loudA.data();
        float* c = loudB.data();
        float ma = 0.0f, mc = 0.0f;
        for (int i = 0; i < M; ++i)
        {
            if (scan) scanSample (s, sc, (float) i / (float) M, 0.0f, a[i], c[i]);
            else orbitSample (s, (float) i / (float) M, 0.0f, 0.0f, a[i], c[i]);
            ma += a[i]; mc += c[i];
        }
        ma /= (float) M; mc /= (float) M;
        float pk = 1.0e-3f;
        for (int i = 0; i < M; ++i) pk = std::max (pk, std::max (std::abs (a[i] - ma), std::abs (c[i] - mc)));
        global.oscGain[o] = std::min (8.0f, 0.6f / pk);
    }
}

void RSynth::render (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, const float* p,
                     const juce::AudioPlayHead::PositionInfo* pos)
{
    const int n = out.getNumSamples();
    float* L = out.getWritePointer (0);
    float* R = out.getWritePointer (1);
    updateGlobal (p, n, pos);
    updateLoudness (p);
    int start = 0;
    auto renderTo = [&] (int end)
    {
        if (end <= start) return;
        for (auto& v : voices)
            if (v.isActive()) v.render (L + start, R + start, end - start, p, global);
        start = end;
    };
    for (const auto meta : midi)
    {
        renderTo (juce::jlimit (0, n, meta.samplePosition));
        handleMidi (meta.getMessage(), p);
    }
    renderTo (n);

    // publish the newest sounding voice for the display
    int count = 0;
    RVoice* newest = nullptr;
    for (auto& v : voices)
        if (v.isActive()) { ++count; if (newest == nullptr || v.order > newest->order) newest = &v; }
    activeVoices.store (count);
    if (newest != nullptr)
    {
        for (size_t d = 0; d < liveNorm.size(); ++d) liveNorm[d].store (newest->liveNorm[d], std::memory_order_relaxed);
        for (int s = 0; s < MS_Count; ++s) liveSrc[(size_t) s].store (newest->srcVal[s], std::memory_order_relaxed);
        for (int o = 0; o < kOscs; ++o) liveOrbit[(size_t) o].store (newest->orbitPhase[o], std::memory_order_relaxed);
    }
    else
    {
        for (auto& a : liveNorm) a.store (-1.0f, std::memory_order_relaxed);
        for (int s = 0; s < MS_Count; ++s) liveSrc[(size_t) s].store (0.0f, std::memory_order_relaxed);
        for (int l = 0; l < kLfos; ++l) liveSrc[(size_t) (MS_Lfo1 + l)].store (global.lfo[l], std::memory_order_relaxed);
    }
}
} // namespace cyr
