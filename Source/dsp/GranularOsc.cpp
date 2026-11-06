#include "GranularOsc.h"

namespace pg
{
GrainWindows::GrainWindows()
{
    for (int i = 0; i <= kSize; ++i)
    {
        const float x = (float) i / (float) kSize;   // 0..1
        // 0 Hann
        table[0][i] = 0.5f - 0.5f * std::cos (kTwoPi * x);
        // 1 Triangle
        table[1][i] = 1.0f - std::abs (2.0f * x - 1.0f);
        // 2 Tukey (25% tapers)
        {
            const float a = 0.25f;
            float w = 1.0f;
            if (x < a * 0.5f) w = 0.5f * (1.0f - std::cos (kTwoPi * x / a));
            else if (x > 1.0f - a * 0.5f) w = 0.5f * (1.0f - std::cos (kTwoPi * (1.0f - x) / a));
            table[2][i] = w;
        }
        // 3 Gauss
        table[3][i] = std::exp (-0.5f * std::pow ((x - 0.5f) / 0.15f, 2.0f));
        // 4 Perc: fast attack, exponential decay
        table[4][i] = juce::jmin (1.0f, x * 50.0f) * std::exp (-5.0f * x) * (1.0f - std::pow (x, 8.0f));
        // 5 Swell: exponential rise, fast release
        table[5][i] = juce::jmin (1.0f, (1.0f - x) * 50.0f) * std::exp (-5.0f * (1.0f - x)) * (1.0f - std::pow (1.0f - x, 8.0f));
    }
}

void GranularOsc::prepare (double sampleRate, uint32_t seed)
{
    sr = sampleRate;
    rng.seed (seed);
    for (auto& g : grains) g.active = false;
    countdown = 0.0;
    scanOffset = 0.0;
}

void GranularOsc::noteOn (bool resetScan)
{
    countdown = 0.0;   // fire a grain immediately
    if (resetScan) scanOffset = 0.0;
}

inline float GranularOsc::readFrame (const SampleData& s, int ch, double pos)
{
    const int n = s.length;
    const float* d = s.audio.getReadPointer (ch);
    const int i = (int) std::floor (pos);
    const float t = (float) (pos - (double) i);
    auto at = [&] (int k) { return (k < 0 || k >= n) ? 0.0f : d[k]; };
    return hermite (at (i - 1), at (i), at (i + 1), at (i + 2), t);
}

void GranularOsc::spawn (const SampleData& s)
{
    Grain* g = nullptr;
    for (auto& c : grains)
        if (! c.active) { g = &c; break; }
    if (g == nullptr)
    {
        // steal the oldest grain relative to its length
        float best = -1.0f;
        for (auto& c : grains)
        {
            const float progress = (float) c.age * c.invLength;
            if (progress > best) { best = progress; g = &c; }
        }
    }

    const double srcRatio = s.sampleRate / sr;
    float semis = prm.pitch + prm.pitchRand * rng.bi();
    if (prm.mode == 0 && prm.keytrack)
        semis += note - (float) prm.root;
    double rate = srcRatio * (double) semisToRatio (semis);
    if (rng.uni() < prm.reverse) rate = -rate;

    const int len = juce::jmax (16, (int) (prm.sizeMs * 0.001f * (float) sr));
    double start = prm.pos + scanOffset + prm.spray * 0.5f * rng.bi();
    start -= std::floor (start);   // wrap 0..1
    double startFrame = start * (double) s.length;
    if (rate < 0.0)   // reversed grains start at the far end so they play through 'start'
        startFrame += (double) len * -rate;

    g->active = true;
    g->readPos = startFrame;
    g->rate = rate;
    g->age = 0;
    g->length = len;
    g->invLength = 1.0f / (float) len;
    g->window = juce::jlimit (0, GrainWindows::kShapes - 1, prm.window);
    const float pan = prm.stereo * rng.bi();
    panGains (pan, g->gainL, g->gainR);
    lastStart = (float) start;
}

void GranularOsc::tick (const SampleData& s, float& outL, float& outR)
{
    // scheduling
    countdown -= 1.0;
    if (countdown <= 0.0)
    {
        spawn (s);
        double interval;
        if (prm.mode == 1)
            interval = sr / (double) midiToHz (note);   // one grain per period of the played note
        else
            interval = sr / (double) juce::jmax (0.5f, prm.density) * (1.0 + 0.3 * rng.bi());
        countdown += juce::jmax (1.0, interval);
    }

    // scan (moves the read position through the file)
    if (prm.scan != 0.0f)
    {
        scanOffset += (double) prm.scan * (s.sampleRate / sr) / (double) s.length;
        scanOffset -= std::floor (scanOffset);
    }

    const auto& win = GrainWindows::get();
    float l = 0.0f, r = 0.0f;
    int active = 0;
    for (auto& g : grains)
    {
        if (! g.active) continue;
        ++active;
        const float ph = (float) g.age * g.invLength * (float) GrainWindows::kSize;
        const int wi = (int) ph;
        const float wf = ph - (float) wi;
        const float* wt = win.table[g.window];
        const float w = wt[wi] + (wt[wi + 1] - wt[wi]) * wf;

        // wrap the read position inside the file
        double p = g.readPos;
        if (p >= (double) s.length) p -= (double) s.length * std::floor (p / (double) s.length);
        else if (p < 0.0) p += (double) s.length * std::ceil (-p / (double) s.length);

        const float xl = readFrame (s, 0, p);
        const float xr = readFrame (s, 1, p);
        l += (xl * g.gainL) * w;
        r += (xr * g.gainR) * w;

        g.readPos += g.rate;
        if (++g.age >= g.length) g.active = false;
    }

    // keep loudness roughly constant as grains overlap
    float overlap;
    if (prm.mode == 1)
        overlap = prm.sizeMs * 0.001f * midiToHz (note);
    else
        overlap = prm.sizeMs * 0.001f * prm.density;
    const float norm = 1.0f / std::sqrt (juce::jmax (1.0f, overlap * 0.5f));
    outL = l * norm * 1.4f;
    outR = r * norm * 1.4f;
    juce::ignoreUnused (active);
}

void GranularOsc::writeViz (const SampleData& s, GrainViz& viz) const
{
    int slot = 0;
    for (auto& g : grains)
    {
        if (! g.active || slot >= GrainViz::kSlots) continue;
        viz.pos[(size_t) slot].store ((float) std::fmod (g.readPos / (double) s.length + 1.0, 1.0), std::memory_order_relaxed);
        viz.amp[(size_t) slot].store (1.0f - (float) g.age * g.invLength, std::memory_order_relaxed);
        ++slot;
    }
    for (; slot < GrainViz::kSlots; ++slot)
        viz.pos[(size_t) slot].store (-1.0f, std::memory_order_relaxed);
    viz.playhead.store (lastStart, std::memory_order_relaxed);
}
} // namespace pg
