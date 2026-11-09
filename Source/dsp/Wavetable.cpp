#include "Wavetable.h"
#include <juce_dsp/juce_dsp.h>

namespace pg
{
int Wavetable::levelOffset (int level)
{
    int off = 0;
    for (int l = 0; l < level; ++l) off += levelLength (l) + 1;   // +1 guard sample
    return off;
}

int Wavetable::frameStorage()
{
    static const int s = levelOffset (kLevels);
    return s;
}

std::shared_ptr<const Wavetable> Wavetable::fromFrames (const std::vector<std::vector<float>>& frames, const juce::String& name)
{
    auto wt = std::make_shared<Wavetable>();
    wt->name = name;
    wt->numFrames = (int) frames.size();
    wt->data.assign ((size_t) wt->numFrames * (size_t) frameStorage(), 0.0f);

    juce::dsp::FFT fwd (11);   // 2048
    std::vector<std::unique_ptr<juce::dsp::FFT>> inv;
    for (int l = 0; l < kLevels; ++l)
        inv.push_back (std::make_unique<juce::dsp::FFT> ((int) std::log2 (levelLength (l))));

    std::vector<std::complex<float>> spec (kFrameLen), tmpIn (kFrameLen), tmpOut (kFrameLen);
    for (int f = 0; f < wt->numFrames; ++f)
    {
        const auto& src = frames[(size_t) f];
        float dc = 0.0f;
        for (int i = 0; i < kFrameLen; ++i) dc += src[(size_t) i];
        dc /= (float) kFrameLen;
        for (int i = 0; i < kFrameLen; ++i) tmpIn[(size_t) i] = { src[(size_t) i] - dc, 0.0f };
        fwd.perform (tmpIn.data(), spec.data(), false);

        float peakAll = 0.0f;
        for (int l = 0; l < kLevels; ++l)
        {
            const int len = levelLength (l);
            const int maxH = juce::jmin (1024 >> l, len / 2 - 1);
            std::vector<std::complex<float>> s ((size_t) len), out ((size_t) len);
            for (int h = 1; h <= maxH; ++h)
            {
                s[(size_t) h] = spec[(size_t) h];
                s[(size_t) (len - h)] = spec[(size_t) (kFrameLen - h)];
            }
            inv[(size_t) l]->perform (s.data(), out.data(), true);
            float* dst = wt->data.data() + (size_t) f * (size_t) frameStorage() + (size_t) levelOffset (l);
            const float scale = (float) len / (float) kFrameLen;
            for (int i = 0; i < len; ++i)
            {
                dst[i] = out[(size_t) i].real() * scale;
                if (l == 0) peakAll = juce::jmax (peakAll, std::abs (dst[i]));
            }
            dst[len] = dst[0];
        }
        // normalise each frame on its full-band peak
        if (peakAll > 1.0e-5f)
        {
            float* fr = wt->data.data() + (size_t) f * (size_t) frameStorage();
            juce::FloatVectorOperations::multiply (fr, 0.95f / peakAll, frameStorage());
        }
    }
    return wt;
}

// ---------------------------------------------------------------------------
// Built-in tables
// ---------------------------------------------------------------------------
static std::vector<float> makeFrame (const std::function<float (float)>& fn)
{
    std::vector<float> v ((size_t) Wavetable::kFrameLen);
    for (int i = 0; i < Wavetable::kFrameLen; ++i)
        v[(size_t) i] = fn ((float) i / (float) Wavetable::kFrameLen);
    return v;
}

static float saw (float p) { return 2.0f * p - 1.0f; }
static float sq (float p, float pw = 0.5f) { return p < pw ? 1.0f : -1.0f; }
static float triw (float p) { return 1.0f - 4.0f * std::abs (p - 0.5f); }
static float frac (float x) { return x - std::floor (x); }

const std::vector<WavetablePtr>& builtInWavetables()
{
    static const std::vector<WavetablePtr> tables = []
    {
        const int N = 32;
        std::vector<WavetablePtr> t;
        auto build = [&] (const juce::String& name, std::function<float (float, float)> gen)
        {
            std::vector<std::vector<float>> frames;
            for (int f = 0; f < N; ++f)
            {
                const float m = (float) f / (float) (N - 1);
                frames.push_back (makeFrame ([&] (float p) { return gen (p, m); }));
            }
            t.push_back (Wavetable::fromFrames (frames, name));
        };

        build ("Basic", [] (float p, float m)   // sine > triangle > saw > square
        {
            const float s = std::sin (kTwoPi * p);
            const float x = m * 3.0f;
            if (x < 1.0f) return s + (triw (frac (p + 0.25f)) - s) * x;
            if (x < 2.0f) return triw (frac (p + 0.25f)) + (saw (p) - triw (frac (p + 0.25f))) * (x - 1.0f);
            return saw (p) + (sq (p) - saw (p)) * (x - 2.0f);
        });
        build ("PWM", [] (float p, float m) { return sq (p, 0.5f - 0.46f * m); });
        build ("Harmonics", [] (float p, float m)
        {
            const int n = 1 + (int) (m * 47.0f);
            float x = 0.0f;
            for (int h = 1; h <= n; ++h) x += std::sin (kTwoPi * p * (float) h) / std::sqrt ((float) h);
            return x;
        });
        build ("Formant", [] (float p, float m)
        {
            const float c1 = 2.0f + 6.0f * m, c2 = 7.0f + 16.0f * (1.0f - m);
            float x = 0.0f;
            for (int h = 1; h <= 40; ++h)
            {
                const float a = std::exp (-std::pow (((float) h - c1) / 1.5f, 2.0f))
                              + 0.6f * std::exp (-std::pow (((float) h - c2) / 2.5f, 2.0f)) + 0.05f / (float) h;
                x += a * std::sin (kTwoPi * p * (float) h);
            }
            return x;
        });
        build ("Sync", [] (float p, float m) { const float r = 1.0f + 7.0f * m; return saw (frac (p * r)) * (1.0f - p * 0.3f); });
        build ("FM", [] (float p, float m) { return std::sin (kTwoPi * p + 6.0f * m * std::sin (kTwoPi * 2.0f * p)); });
        build ("Fold", [] (float p, float m) { return std::sin (std::sin (kTwoPi * p) * (1.0f + 7.0f * m) * 1.5707963f); });
        build ("Digital", [] (float p, float m)
        {
            const float levels = std::round (32.0f - 30.0f * m);
            const float s = std::sin (kTwoPi * p) * 0.5f + 0.5f * saw (p);
            return std::round (s * levels) / levels;
        });
        return t;
    }();
    return tables;
}

WavetablePtr loadWavetableFile (const juce::File& file, juce::String& error)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (file));
    if (r == nullptr) { error = "Could not read " + file.getFileName(); return nullptr; }
    const int len = (int) std::min<juce::int64> (r->lengthInSamples, (juce::int64) Wavetable::kFrameLen * 256);
    if (len < 32) { error = "The file is too short to become a wavetable."; return nullptr; }
    juce::AudioBuffer<float> buf (1, len);
    r->read (&buf, 0, len, 0, true, false);
    const float* d = buf.getReadPointer (0);

    std::vector<std::vector<float>> frames;
    if (len % Wavetable::kFrameLen == 0)
    {
        for (int f = 0; f < len / Wavetable::kFrameLen; ++f)
            frames.emplace_back (d + f * Wavetable::kFrameLen, d + (f + 1) * Wavetable::kFrameLen);
    }
    else if (len < Wavetable::kFrameLen * 2)
    {
        std::vector<float> v ((size_t) Wavetable::kFrameLen);   // resample one cycle
        for (int i = 0; i < Wavetable::kFrameLen; ++i)
        {
            const float x = (float) i * (float) len / (float) Wavetable::kFrameLen;
            const int a = (int) x;
            const float t = x - (float) a;
            v[(size_t) i] = d[a % len] + (d[(a + 1) % len] - d[a % len]) * t;
        }
        frames.push_back (v);
    }
    else
    {
        for (int f = 0; f < juce::jmin (64, len / Wavetable::kFrameLen); ++f)
            frames.emplace_back (d + f * Wavetable::kFrameLen, d + (f + 1) * Wavetable::kFrameLen);
    }
    return Wavetable::fromFrames (frames, file.getFileNameWithoutExtension());
}

// ---------------------------------------------------------------------------
// Oscillators
// ---------------------------------------------------------------------------
void WavetableOsc::prepare (double sampleRate, uint32_t seed)
{
    sr = sampleRate;
    rng.seed (seed);
    setup (1, 0.0f, 0.0f, 0.0f);
}

void WavetableOsc::noteOn (bool randomPhase, float startPhase)
{
    for (int i = 0; i < kMaxUnison; ++i)
        phase[i] = randomPhase ? rng.uni() : startPhase;
}

void WavetableOsc::setup (int unison, float detune, float spread, float pan)
{
    voices = juce::jlimit (1, kMaxUnison, unison);
    for (int i = 0; i < voices; ++i)
    {
        const float k = voices == 1 ? 0.0f : ((float) i / (float) (voices - 1)) * 2.0f - 1.0f;   // -1..1
        detuneRatio[i] = semisToRatio (k * detune * 0.5f);   // up to +-50 cents at full detune
        float l, r;
        panGains (clampf (pan + k * spread, -1.0f, 1.0f), l, r);
        gainL[i] = l;
        gainR[i] = r;
    }
    norm = 1.0f / std::sqrt ((float) voices);
}

namespace
{
    enum Warp { W_Off, W_BendP, W_BendM, W_BendPM, W_BloatP, W_BloatM, W_Sync, W_SyncWin, W_Pulse, W_Fm, W_FmWin,
                W_Flip, W_Mirror, W_Breed, W_Brew, W_Sinc, W_Crush, W_Smooth, W_Window };

    inline float readTable (const float* t0, const float* t1, int len, float ft, float p)
    {
        p -= std::floor (p);
        const float x = p * (float) len;
        const int a = (int) x;
        const float t = x - (float) a;
        const float s0 = t0[a] + (t0[a + 1] - t0[a]) * t;
        const float s1 = t1[a] + (t1[a + 1] - t1[a]) * t;
        return s0 + (s1 - s0) * ft;
    }

    // Phase warps: map the oscillator phase p (0..1) to a new read position
    inline float warpPhase (int mode, float a, float p)
    {
        const float e = 1.0f + 4.0f * a;
        switch (mode)
        {
            case W_BendP:  return std::pow (p, e);
            case W_BendM:  return 1.0f - std::pow (1.0f - p, e);
            case W_BendPM: { const float u = 2.0f * p - 1.0f; return 0.5f + 0.5f * std::copysign (std::pow (std::abs (u), e), u); }
            case W_BloatP: return std::pow (p, 1.0f / e);
            case W_BloatM: return 1.0f - std::pow (1.0f - p, 1.0f / e);
            case W_Sync: case W_SyncWin: case W_Sinc:
            { const float q = p * (1.0f + 7.0f * a); return q - std::floor (q); }
            case W_Pulse:
            {
                const float w = 0.5f - 0.47f * a;
                return p < w ? p * 0.5f / w : 0.5f + (p - w) * 0.5f / (1.0f - w);
            }
            case W_Fm: case W_FmWin: return p + a * 0.35f * std::sin (kTwoPi * 2.0f * p);
            case W_Mirror: { const float m = p < 0.5f ? 2.0f * p : 2.0f - 2.0f * p; return p + (m * 0.5f - p) * a; }
            case W_Brew: return p + a * 0.12f * std::sin (kTwoPi * 5.0f * p) * std::sin (kPi * p);
            case W_Crush:
            {
                const float steps = std::exp2 (10.0f - 7.0f * a);
                return std::floor (p * steps) / steps;
            }
            default: return p;
        }
    }

    // Extra harmonics a warp adds: read a smoother mip level to limit aliasing
    inline int warpLevelBoost (int mode, float a)
    {
        switch (mode)
        {
            case W_Sync: case W_SyncWin: case W_Sinc: return (int) std::ceil (std::log2 (1.0f + 7.0f * a));
            case W_Fm: case W_FmWin: case W_Brew: return a > 0.3f ? 1 : 0;
            case W_Smooth: return (int) std::round (a * 7.0f);
            default: return 0;
        }
    }
}

static inline float phaseDistort (float p, float pd)
{
    if (pd == 0.0f) return p;
    const float w = 0.5f - 0.48f * clampf (pd, -0.95f, 0.95f);
    return p < w ? p * 0.5f / w : 0.5f + (p - w) * 0.5f / (1.0f - w);
}

void WavetableOsc::tick (const Wavetable& wt, float freqHz, float pos, float phaseMod, float& outL, float& outR, float pd)
{
    const float maxHarm = (float) sr * 0.5f / juce::jmax (1.0f, freqHz);
    int lvl = 0;
    while (lvl < Wavetable::kLevels - 1 && (float) (1024 >> lvl) > maxHarm) ++lvl;
    const bool warping = warpMode != W_Off && warpAmt > 0.0f;
    if (warping) lvl = juce::jmin (Wavetable::kLevels - 1, lvl + warpLevelBoost (warpMode, warpAmt));

    const float fpos = clamp01 (pos) * (float) (wt.numFrames - 1);
    const int f0 = (int) fpos;
    const int f1 = juce::jmin (f0 + 1, wt.numFrames - 1);
    const float ft = fpos - (float) f0;
    const float* t0 = wt.level (f0, lvl);
    const float* t1 = wt.level (f1, lvl);
    const int len = Wavetable::levelLength (lvl);
    const float a = warpAmt;

    float l = 0.0f, r = 0.0f;
    const float inc = freqHz / (float) sr;
    for (int i = 0; i < voices; ++i)
    {
        float p = phase[i] + phaseMod;
        p -= std::floor (p);
        p = phaseDistort (p, pd);
        float s;
        if (! warping) s = readTable (t0, t1, len, ft, p);
        else
        {
            s = readTable (t0, t1, len, ft, warpPhase (warpMode, a, p));
            switch (warpMode)
            {
                case W_SyncWin: s *= 1.0f - p; break;                                       // fade each sync burst
                case W_Sinc:    s *= std::sin (kPi * p); break;                             // half-sine envelope
                case W_FmWin:   s *= 0.5f - 0.5f * std::cos (kTwoPi * p); break;
                case W_Flip:    if (p >= 0.5f) s *= 1.0f - 2.0f * a; break;                 // flip the second half
                case W_Breed:   s = (s + a * readTable (t0, t1, len, ft, 2.0f * p)) / (1.0f + a);  // add the octave
                                break;
                case W_Crush:
                {
                    const float lv = std::exp2 (8.0f - 6.0f * a);
                    s = std::round (s * lv) / lv;
                    break;
                }
                case W_Window:  s *= std::pow (std::sin (kPi * p), a * 4.0f); break;
                default: break;
            }
        }
        l += s * gainL[i];
        r += s * gainR[i];
        phase[i] += inc * detuneRatio[i];
        if (phase[i] >= 1.0f) phase[i] -= 1.0f;
        else if (phase[i] < 0.0f) phase[i] += 1.0f;
    }
    outL = l * norm;
    outR = r * norm;
    last = (outL + outR) * 0.7071f;
}

void WavetableOsc::renderCycle (const Wavetable& wt, float pos, int warpMode, float warpAmount, float* out, int n)
{
    WavetableOsc o;
    o.prepare ((double) n * 100.0, 1);
    o.setup (1, 0.0f, 0.0f, 0.0f);
    o.setWarp (warpMode, warpAmount);
    o.noteOn (false);
    for (int i = 0; i < n; ++i)
    {
        float l, r;
        o.tick (wt, 100.0f, pos, 0.0f, l, r);
        out[i] = (l + r) * 0.7071f;
    }
}

float BasicOsc::tick (int shape, float freqHz, float sampleRate, float phaseMod, float pd)
{
    const float dt = juce::jlimit (0.0f, 0.49f, freqHz / sampleRate);
    float p = phase + phaseMod;
    p -= std::floor (p);
    p = phaseDistort (p, pd);
    float y = 0.0f;
    switch (shape)
    {
        case 0: y = std::sin (kTwoPi * p); break;
        case 1:
        {
            float s = (p < 0.5f ? 1.0f : -1.0f) + polyBlep (p, dt) - polyBlep (frac (p + 0.5f), dt);
            tri = dt * 4.0f * s + (1.0f - dt * 0.5f) * tri;   // leaky integrated square
            y = tri;
            break;
        }
        case 2: y = (2.0f * p - 1.0f) - polyBlep (p, dt); break;
        default: y = (p < 0.5f ? 1.0f : -1.0f) + polyBlep (p, dt) - polyBlep (frac (p + 0.5f), dt); break;
    }
    phase += dt;
    if (phase >= 1.0f) phase -= 1.0f;
    return y;
}
} // namespace pg
