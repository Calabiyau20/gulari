// Headless checks for the West Coast rack and the CW-259:  WcRenderTest [outDir]
//  1. pitch tracking at 1.2 V/oct over 6 octaves (cents error of the principal sine)
//  2. Timbre sweep: level and aliasing (energy off the harmonic series) at 110 Hz and 1760 Hz
//  3. phase lock: frequency of a detuned 259 B with and without lock
//  4. CPU with four 259s
//  5. renders every built-in patch to WAV
#include "WcProcessor.h"
#include "modules/M259.h"
#include "modules/MidiInterface.h"
#include "modules/M281.h"
#include "modules/M292.h"
#include "modules/OutputModule.h"
#include "modules/M266.h"
#include "modules/M245.h"
#include "modules/M242.h"
#include "modules/M257.h"
#include "modules/M258.h"
#include "modules/M264.h"
#include "modules/M230.h"
#include "modules/M285.h"
#include "modules/M277.h"
#include "modules/M207.h"
#include "modules/M296.h"
#include "modules/M248.h"
#include "modules/M218.h"
#include "modules/M250.h"
#include "modules/M252.h"
#include "modules/M267.h"
#include "modules/M275.h"
#include "modules/M284.h"
#include "modules/M291.h"
#include "modules/M666.h"
#include "WcLicense.h"
#include <juce_dsp/juce_dsp.h>
#include <cstdio>
#include <set>

using namespace wc;
static constexpr double SR = 48000.0;

static void setP (WcProcessor& p, const juce::String& id, float v)
{
    if (auto* x = p.apvts.getParameter (id)) x->setValueNotifyingHost (x->convertTo0to1 (v));
}

// Two texts of the same parameter that show the same value: "1.00 s" and "1000 ms" are both one second, within
// half a unit of the last digit of the first one. Words and labels have to match exactly.
static bool sameShownValue (const juce::String& a, const juce::String& b)
{
    auto read = [] (const juce::String& t, double& value, double& halfDigit)
    {
        auto s = t.upToFirstOccurrenceOf ("(", false, false).removeCharacters (" ").toLowerCase();
        if (s.startsWithChar ('x')) s = s.substring (1);
        const auto num = s.initialSectionContainingOnly ("+-0123456789.");
        if (! num.containsAnyOf ("0123456789")) return false;
        const auto unit = s.substring (num.length());
        const double f = unit == "ms" ? 1.0e-3 : unit == "us" ? 1.0e-6 : unit == "khz" ? 1.0e3 : 1.0;
        const int decimals = num.containsChar ('.') ? num.length() - num.indexOfChar ('.') - 1 : 0;
        value = num.getDoubleValue() * f;
        halfDigit = 0.5 * std::pow (10.0, -decimals) * f;
        return true;
    };
    double va, ha, vb, hb;
    if (! read (a, va, ha) || ! read (b, vb, hb)) return false;
    return std::abs (va - vb) <= ha * 1.001 + 1.0e-12;
}

// run the rack for n samples, collecting one jack (or the main output when inst < 0)
static std::vector<float> run (WcProcessor& p, int n, int inst = -1, int jack = 0, int noteOn = -1)
{
    std::vector<float> out;
    out.reserve ((size_t) n);
    const int block = 256;
    for (int s = 0; s < n; s += block)
    {
        const int len = std::min (block, n - s);
        juce::AudioBuffer<float> buf (2, len);
        juce::MidiBuffer midi;
        if (s == 0 && noteOn >= 0) midi.addEvent (juce::MidiMessage::noteOn (1, noteOn, (juce::uint8) 100), 0);
        if (inst < 0) { p.processBlock (buf, midi); for (int i = 0; i < len; ++i) out.push_back (buf.getSample (0, i)); }
        else
        {
            // same as processBlock, sample by sample, reading the jack
            p.rack.beginBlock (len);
            auto it = midi.cbegin();
            for (int i = 0; i < len; ++i)
            {
                while (it != midi.cend() && (*it).samplePosition <= i) { p.rack.handleMidi ((*it).getMessage()); ++it; }
                p.rack.tick();
                out.push_back (p.rack.jackValue (inst, jack));
            }
        }
    }
    return out;
}

static double measureHz (const std::vector<float>& x, int skip)
{
    // rising zero crossings with linear interpolation
    double first = -1, last = -1; int count = 0;
    for (size_t i = (size_t) skip + 1; i < x.size(); ++i)
        if (x[i - 1] <= 0.0f && x[i] > 0.0f)
        {
            const double t = (double) (i - 1) + x[i - 1] / (double) (x[i - 1] - x[i]);
            if (first < 0) first = t; else { last = t; ++count; }
        }
    return count > 0 ? SR * count / (last - first) : 0.0;
}

// fraction of energy NOT on harmonics of f0 (aliasing + noise), in dB
static void spectrum (const std::vector<float>& x, double f0, double& rmsOut, double& offDb)
{
    const int order = 15, N = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> d ((size_t) N * 2, 0.0f);
    const size_t start = x.size() - (size_t) N;
    double rms = 0.0;
    for (int i = 0; i < N; ++i)
    {
        const float w = 0.5f - 0.5f * std::cos (2.0f * 3.14159265f * (float) i / (float) N);   // Hann
        d[(size_t) i] = x[start + (size_t) i] * w;
        rms += (double) x[start + (size_t) i] * x[start + (size_t) i];
    }
    rmsOut = std::sqrt (rms / N);
    fft.performFrequencyOnlyForwardTransform (d.data());
    double on = 0.0, off = 0.0;
    const double binHz = SR / N;
    for (int k = 2; k < N / 2; ++k)
    {
        const double f = k * binHz, e = (double) d[(size_t) k] * d[(size_t) k];
        const double h = f / f0, dist = std::fabs (h - std::round (h)) * f0;   // Hz from the nearest harmonic
        if (dist < 6.0 * binHz && std::round (h) >= 1.0) on += e; else off += e;
    }
    offDb = 10.0 * std::log10 ((off + 1e-30) / (on + off + 1e-30));
}

static void writeWav (const juce::File& f, const std::vector<float>& L, const std::vector<float>& R)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), SR, 2, 24, {}, 0));
    if (w == nullptr) return;
    juce::AudioBuffer<float> b (2, (int) L.size());
    for (int i = 0; i < (int) L.size(); ++i) { b.setSample (0, i, L[(size_t) i]); b.setSample (1, i, R[(size_t) i]); }
    w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (juce::String (argc > 1 ? argv[1] : "/tmp/wc_render"));
    outDir.createDirectory();
    bool ok = true;

    // ---- 1. tracking ----
    {
        WcProcessor p;
        p.prepareToPlay (SR, 256);
        p.loadBuiltinPatch (0);
        run (p, 4800);
        double worst = 0.0;
        std::printf ("TRACKING (principal sine, 1.2 V/oct)\n");
        for (int note : { 24, 36, 48, 60, 72, 84, 96 })
        {
            auto x = run (p, (int) SR, kInst259, M259::Out_Sine, note);
            const double hz = measureHz (x, 4800);
            const double want = 440.0 * std::pow (2.0, (note - 69) / 12.0);
            const double cents = 1200.0 * std::log2 (hz / want);
            worst = std::max (worst, std::fabs (cents));
            std::printf ("  note %3d  %9.3f Hz  (want %9.3f)  %+6.2f cents\n", note, hz, want, cents);
            juce::AudioBuffer<float> b (2, 16); juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOff (1, note), 0); p.processBlock (b, m);
        }
        std::printf ("  worst %.2f cents\n", worst);
        if (worst > 3.0) ok = false;
    }

    // ---- 2. timbre sweep ----
    {
        std::printf ("TIMBRE SWEEP (Final out; level dB re sine, energy off the harmonic series)\n");
        for (int note : { 45, 93 })   // A2 110 Hz, A6 1760 Hz
        {
            WcProcessor p;
            p.prepareToPlay (SR, 256);
            p.loadBuiltinPatch (0);
            setP (p, "a_timbre", 0.0f);
            run (p, 2400);
            const double f0 = 440.0 * std::pow (2.0, (note - 69) / 12.0);
            double sineRms = 0.0;
            for (float t : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
            {
                setP (p, "a_timbre", t);
                auto x = run (p, 48000, kInst259, M259::Out_Final1, t == 0.0f ? note : -1);
                double rms, off;
                spectrum (x, f0, rms, off);
                if (t == 0.0f) sineRms = rms;
                std::printf ("  %6.0f Hz  timbre %.2f  level %+5.1f dB  off-harmonic %6.1f dB\n", f0, t, 20.0 * std::log10 (rms / sineRms), off);
                if (off > -45.0) ok = false;
            }
            for (float o : { 1.0f })
            {
                setP (p, "a_order", o); setP (p, "a_timbre", 1.0f);
                for (float s : { 0.0f, 1.0f })
                {
                    setP (p, "a_sym", s);
                    auto x = run (p, 48000, kInst259, M259::Out_Final1);
                    double rms, off;
                    spectrum (x, f0, rms, off);
                    std::printf ("  %6.0f Hz  order 1 sym %.0f (%s)  level %+5.1f dB  off-harmonic %6.1f dB\n", f0, s, s < 0.5f ? "saw" : "M", 20.0 * std::log10 (rms / sineRms), off);
                }
            }
        }
    }

    // ---- 3. phase lock ----
    {
        std::printf ("PHASE LOCK\n");
        WcProcessor p;
        p.prepareToPlay (SR, 256);
        p.loadBuiltinPatch (7);
        for (float lock : { 0.0f, 0.8f })
        {
            setP (p, "b_lock", lock); setP (p, "b_lock_on", lock > 0.0f ? 1.0f : 0.0f);
            run (p, 2400);
            auto a = run (p, (int) SR * 2, kInst259, M259::Out_Sine, 48);
            auto b = run (p, (int) SR * 2, kInst259 + 1, M259::Out_Sine);
            const double fa = measureHz (a, 24000), fb = measureHz (b, 24000);
            std::printf ("  lock %.1f: A %.3f Hz  B %.3f Hz  (%+.2f cents)\n", lock, fa, fb, 1200.0 * std::log2 (fb / fa));
            juce::AudioBuffer<float> bb (2, 16); juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOff (1, 48), 0); p.processBlock (bb, m);
            if (lock > 0.5f && std::fabs (1200.0 * std::log2 (fb / fa)) > 1.0) ok = false;
        }
    }

    // ---- 3b. 281 timing ----
    {
        std::printf ("281 FUNCTION TIMING\n");
        WcProcessor p;
        p.prepareToPlay (SR, 256);
        p.loadBuiltinPatch (9);   // gate -> 281 A ch1
        for (float a : { 0.25f, 0.5f })
        {
            setP (p, "fa_att1", a); setP (p, "fa_dec1", a);
            run (p, 4800);
            auto x = run (p, (int) SR * 3, kInst281, M281::outFunc (0), 48);
            int peakAt = 0; for (int i = 0; i < (int) x.size(); ++i) if (x[(size_t) i] > x[(size_t) peakAt]) peakAt = i;
            int endAt = peakAt; while (endAt < (int) x.size() - 1 && x[(size_t) endAt] > 0.0f) ++endAt;
            const double want = M281::timeSeconds (a);
            std::printf ("  knob %.2f: attack %.4f s, decay %.4f s (want %.4f), peak %.2f V\n", a, peakAt / SR, (endAt - peakAt) / SR, want, x[(size_t) peakAt]);
            if (std::fabs (peakAt / SR - want) > 0.002 + 0.01 * want) ok = false;
            juce::AudioBuffer<float> bb (2, 16); juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOff (1, 48), 0); p.processBlock (bb, m);
        }
    }

    // ---- 3c. 292 gate: level and brightness vs control voltage, vactrol release ----
    {
        std::printf ("292 LOWPASS GATE (input: 110 Hz saw; level re input, spectral centroid)\n");
        const char* modes[3] = { "LOPASS", "BOTH", "VCA" };
        for (int mode = 0; mode < 3; ++mode)
        {
            WcProcessor p;
            p.prepareToPlay (SR, 256);
            setP (p, "la_mode1", (float) mode);
            setP (p, "la_res1", 0.0f);
            auto& g = *p.rack.m292 (0);
            float in[24] {}, out[24] {};
            for (float v : { 10.0f, 5.0f, 2.0f, 0.5f })
            {
                double ein = 0, eout = 0, cIn = 0, cOut = 0, dIn = 0, dOut = 0;
                double ph = 0, prevIn = 0, prevOut = 0;
                const int N = (int) SR;
                for (int i = 0; i < N; ++i)
                {
                    if (i % 256 == 0) g.beginBlock (256);
                    ph += 110.0 / SR; ph -= std::floor (ph);
                    in[M292::inSig (0)] = (float) (2.0 * ph - 1.0);
                    in[M292::inCtl (0)] = v;
                    g.tick (in, out);
                    if (i > N / 2)
                    {
                        const double x = in[M292::inSig (0)], y = out[M292::outSig (0)];
                        ein += x * x; eout += y * y;
                        // derivative energy / signal energy ~ brightness
                        cIn += (x - prevIn) * (x - prevIn); cOut += (y - prevOut) * (y - prevOut);
                        juce::ignoreUnused (dIn, dOut);
                    }
                    prevIn = in[M292::inSig (0)]; prevOut = out[M292::outSig (0)];
                }
                const double lvl = 10.0 * std::log10 (eout / ein + 1e-30);
                const double brightIn = std::sqrt (cIn / ein), brightOut = eout > 0 ? std::sqrt (cOut / eout) : 0.0;
                std::printf ("  %-6s  CV %4.1f V   level %+6.1f dB   brightness %5.1f %%\n", modes[mode], v, lvl, 100.0 * brightOut / brightIn);
            }
            // release: open at 10 V for 300 ms, then 0 V; time until the output is 40 dB down
            {
                float in2[24] {}, out2[24] {};
                double ph = 0;
                std::vector<float> y;
                for (int i = 0; i < (int) (SR * 2.5); ++i)
                {
                    if (i % 256 == 0) g.beginBlock (256);
                    ph += 110.0 / SR; ph -= std::floor (ph);
                    in2[M292::inSig (0)] = (float) (2.0 * ph - 1.0);
                    in2[M292::inCtl (0)] = i < (int) (SR * 0.3) ? 10.0f : 0.0f;
                    g.tick (in2, out2);
                    y.push_back (out2[M292::outSig (0)]);
                }
                auto rms = [&] (int at) { double e = 0; for (int k = 0; k < 2400; ++k) e += (double) y[(size_t) (at + k)] * y[(size_t) (at + k)]; return std::sqrt (e / 2400); };
                const double ref = rms ((int) (SR * 0.2));
                int t = (int) (SR * 0.3);
                while (t < (int) y.size() - 2400 && rms (t) > ref * 0.01) t += 240;
                std::printf ("  %-6s  release to -40 dB: %.0f ms\n", modes[mode], 1000.0 * (t - SR * 0.3) / SR);
            }
        }
    }

    // ---- 3c2. 266 Source of Uncertainty ----
    {
        std::printf ("266 SOURCE OF UNCERTAINTY\n");
        WcProcessor p;
        p.prepareToPlay (SR, 256);
        auto& u = *p.rack.m266();
        float in[24] {}, out[24] {};
        // noise colour: energy ratio of the 8-16 kHz band vs the 250-500 Hz band
        {
            const int order = 14, N = 1 << order;
            juce::dsp::FFT fft (order);
            const char* names[3] = { "pink", "white", "blue" };
            const int outs[3] = { M266::Out_Pink, M266::Out_White, M266::Out_Blue };
            std::vector<std::vector<float>> bufs (3, std::vector<float> ((size_t) N * 2, 0.0f));
            u.beginBlock (256);
            std::vector<double> spec[3];
            for (int k = 0; k < 3; ++k) spec[k].assign ((size_t) N / 2, 0.0);
            for (int rep = 0; rep < 16; ++rep)
            {
                for (int i = 0; i < N; ++i)
                {
                    u.tick (in, out);
                    for (int k = 0; k < 3; ++k) bufs[(size_t) k][(size_t) i] = out[outs[k]];
                }
                for (int k = 0; k < 3; ++k)
                {
                    fft.performFrequencyOnlyForwardTransform (bufs[(size_t) k].data());
                    for (int b = 0; b < N / 2; ++b) spec[k][(size_t) b] += (double) bufs[(size_t) k][(size_t) b] * bufs[(size_t) k][(size_t) b];
                    std::fill (bufs[(size_t) k].begin(), bufs[(size_t) k].end(), 0.0f);
                }
            }
            auto band = [&] (int k, double f0, double f1) { double e = 0; for (int b = (int) (f0 * N / SR); b < (int) (f1 * N / SR); ++b) e += spec[k][(size_t) b]; return e / (f1 - f0); };
            for (int k = 0; k < 3; ++k)
            {
                // slope in dB per octave between ~350 Hz and ~11 kHz (5 octaves)
                const double slope = 10.0 * std::log10 (band (k, 8000, 16000) / band (k, 250, 500)) / 5.0;
                double e = 0; for (auto v : spec[k]) e += v;
                std::printf ("  %-5s noise: %+5.1f dB/oct  (level %+5.1f dB re white)\n", names[k], slope, 10.0 * std::log10 (e / [&] { double w = 0; for (auto v : spec[1]) w += v; return w; }()));
                if (std::fabs (slope - (k == 0 ? -3.0 : (k == 1 ? 0.0 : 3.0))) > 0.8) ok = false;
            }
        }
        // quantized: count distinct levels on both outputs (n = 3: 2^n = 8 levels in 10 V, n+1 = 4 levels in 5 V)
        {
            setP (p, "ua_steps", 3.0f);
            u.beginBlock (256);
            std::set<int> l2n, ln1;
            for (int i = 0; i < 4000; ++i)
            {
                in[M266::In_QPulse] = (i % 2) ? 10.0f : 0.0f;
                u.tick (in, out);
                if (i > 2) { l2n.insert ((int) std::lround (out[M266::Out_Q2n] * 1000.0f)); ln1.insert ((int) std::lround (out[M266::Out_Qn1] * 1000.0f)); }
            }
            std::printf ("  quantized n=3: 2^n out %d levels (max %.2f V), n+1 out %d levels (max %.2f V)\n",
                         (int) l2n.size(), *l2n.rbegin() / 1000.0, (int) ln1.size(), *ln1.rbegin() / 1000.0);
            if (l2n.size() != 8 || ln1.size() != 4) ok = false;
        }
        in[M266::In_QPulse] = 0.0f;
        // sample & hold alternation
        {
            float lastA = -1, lastB = -1; int changesA = 0, changesB = 0;
            for (int i = 0; i < 400; ++i)
            {
                in[M266::In_ShPulse] = (i % 20) < 2 ? 10.0f : 0.0f;
                in[M266::In_ShCv] = (float) (i / 20);
                u.tick (in, out);
                if (out[M266::Out_ShAltA] != lastA) { lastA = out[M266::Out_ShAltA]; ++changesA; }
                if (out[M266::Out_ShAltB] != lastB) { lastB = out[M266::Out_ShAltB]; ++changesB; }
            }
            std::printf ("  s&h alternating outputs: A changed %d times, B %d times (20 pulses)\n", changesA, changesB);
            if (std::abs (changesA - changesB) > 1) ok = false;
        }
    }

    // ---- 3c3. 245 Sequential Voltage Source ----
    {
        std::printf ("245 SEQUENTIAL VOLTAGE SOURCE\n");
        WcProcessor p;
        p.prepareToPlay (SR, 256);
        auto& m = *p.rack.m245();
        float in[24] {}, out[24] {};
        for (int k = 1; k <= 5; ++k) setP (p, "sv_a" + juce::String (k), 0.1f * (float) k);
        setP (p, "sv_time", 0.5f); setP (p, "sv_len", 0.25f);
        m.reset(); m.beginBlock (256);
        // pulser period and the stage order 1 2 3 4 5 1 ...
        int pulses = 0; bool was = false; std::vector<int> order; int lastStage = -1;
        const int N = (int) SR * 3;
        int gateSamples = 0;
        for (int i = 0; i < N; ++i)
        {
            m.tick (in, out);
            const bool hi = out[M245::Out_Pulse] > 1.0f;
            if (hi && ! was) ++pulses;
            if (hi) ++gateSamples;
            was = hi;
            int st = -1; for (int k = 0; k < 5; ++k) if (out[M245::Out_Gate1 + k] > 5.0f) st = k;
            if (st != lastStage) { order.push_back (st); lastStage = st; }
        }
        const double per = M245::periodSec (0.5f), want = 3.0 / per;
        std::printf ("  pulser: %d pulses in 3 s (expected %.1f, period %.3f s), pulse length %.0f %% of the period\n", pulses, want, per, 100.0 * gateSamples / pulses / (per * SR));
        if (std::abs (pulses - want) > 1.5) ok = false;
        bool seqOk = order.size() > 6;
        for (size_t k = 1; k < order.size(); ++k) if (order[k] != (order[k - 1] + 1) % 5) seqOk = false;
        std::printf ("  stage order: "); for (size_t k = 0; k < juce::jmin<size_t> (order.size(), 12); ++k) std::printf ("%d ", order[k] + 1); std::printf ("%s\n", seqOk ? "ok" : "WRONG");
        if (! seqOk) ok = false;
        // stage switch 2 off: the sequence returns to 1 after stage 2
        setP (p, "sv_sw2", 0.0f); m.reset(); m.beginBlock (256);
        std::set<int> seen;
        for (int i = 0; i < N; ++i) { m.tick (in, out); for (int k = 0; k < 5; ++k) if (out[M245::Out_Gate1 + k] > 5.0f) seen.insert (k); }
        std::printf ("  switch 2 off: stages visited %d (expected 2)\n", (int) seen.size());
        if (seen.size() != 2) ok = false;
        setP (p, "sv_sw2", 1.0f); m.beginBlock (256);
        // analog addressing: 5 V -> stage 3, row A = 3 V
        in[M245::In_Analog] = 5.0f;
        for (int i = 0; i < 100; ++i) m.tick (in, out);
        std::printf ("  analog in 5 V: stage %d, row A %.2f V\n", [&] { for (int k = 0; k < 5; ++k) if (out[M245::Out_Gate1 + k] > 5.0f) return k + 1; return 0; }(), out[M245::Out_A]);
        if (out[M245::Out_Gate3] < 5.0f || std::fabs (out[M245::Out_A] - 3.0f) > 0.01f) ok = false;
        in[M245::In_Analog] = 0.0f;
        // stop, then stop again = one single pulse
        m.btnStop.store (true); for (int i = 0; i < (int) SR; ++i) m.tick (in, out); m.btnStop.store (false);   // let the last pulse end
        int after = 0; was = false;
        for (int i = 0; i < (int) SR * 2; ++i)
        {
            if (i == 1000) m.btnStop.store (true);
            if (i == 1100) m.btnStop.store (false);
            m.tick (in, out);
            const bool hi = out[M245::Out_Pulse] > 1.0f; if (hi && ! was && i >= 1000) ++after; was = hi;
        }
        std::printf ("  stopped + stop button: %d pulse (expected 1)\n", after);
        if (after != 1) ok = false;
    }

    // ---- 3c4. 242 Programmable Pulser ----
    {
        std::printf ("242 PROGRAMMABLE PULSER\n");
        WcProcessor p;
        p.prepareToPlay (SR, 256);
        auto& m = *p.rack.m242();
        float in[24] {}, out[24] {};
        setP (p, "pp_len", 5.0f); setP (p, "pp_a1", 1.0f); setP (p, "pp_a3", 1.0f); setP (p, "pp_b2", 1.0f); setP (p, "pp_period", 0.3f);
        m.reset(); m.beginBlock (256);
        int all = 0, a = 0, b = 0, alt1 = 0, alt2 = 0;
        bool wAll = false, wA = false, wB = false, w1 = false, w2 = false;
        auto edge = [] (float v, bool& w) { const bool h = v > 5.0f; const bool r = h && ! w; w = h; return r; };
        const int N = (int) SR * 4;
        for (int i = 0; i < N; ++i)
        {
            m.tick (in, out);
            all += edge (out[M242::Out_All], wAll); a += edge (out[M242::Out_A], wA); b += edge (out[M242::Out_B], wB);
            alt1 += edge (out[M242::Out_Alt1], w1); alt2 += edge (out[M242::Out_Alt2], w2);
        }
        const int cycles = all / 5;
        std::printf ("  %d clock pulses (period %.3f s): matrix A %d (2 per 5), B %d (1 per 5), alter %d / %d\n", all, M242::periodSec (0.3f), a, b, alt1, alt2);
        if (std::abs (a - 2 * cycles) > 2 || std::abs (b - cycles) > 1 || std::abs (alt1 - alt2) > 1 || std::abs (all - 4.0 / M242::periodSec (0.3f)) > 1.5) ok = false;
        // external clock drives the matrix only
        bool patchedIn[24] {}; patchedIn[M242::In_Clk] = true;
        const bool* saved = m.patched; m.patched = patchedIn;
        setP (p, "pp_period", 1.0f); m.reset(); m.beginBlock (256);   // internal clock off
        a = 0; all = 0; wA = wAll = false;
        for (int i = 0; i < 10 * 200; ++i)
        {
            in[M242::In_Clk] = (i % 200) < 20 ? 10.0f : 0.0f;
            m.tick (in, out);
            a += edge (out[M242::Out_A], wA); all += edge (out[M242::Out_All], wAll);
        }
        m.patched = saved;
        std::printf ("  external clock, 10 pulses: matrix A %d (expected 4), all out %d (expected 0, internal off)\n", a, all);
        if (a != 4 || all != 0) ok = false;
    }

    // ---- 3c5. 257 / 258 / 264 / 230 ----
    {
        std::printf ("257 DUAL VOLTAGE PROCESSOR\n");
        WcProcessor p; p.prepareToPlay (SR, 256);
        auto& m = *p.rack.m257();
        float in[24] {}, out[24] {};
        setP (p, "vp_k1", -0.5f); setP (p, "vp_m1", 0.25f); setP (p, "vp_p51", 1.0f); setP (p, "vp_off1", 0.1f);
        m.beginBlock (256);
        in[M257::inOf (0, 0)] = 4.0f; in[M257::inOf (0, 1)] = 2.0f; in[M257::inOf (0, 2)] = 2.5f; in[M257::inOf (0, 3)] = 8.0f;
        m.tick (in, out);
        const float want = 4.0f * -0.5f + (2.0f + 5.0f) * 0.5f + 8.0f * 0.5f + 1.0f;   // M' = 0.25 + 0.25
        std::printf ("  Va K + Vb (1-M) + Vc M + off = %.3f V (expected %.3f)\n", out[M257::Out_1], want);
        if (std::fabs (out[M257::Out_1] - want) > 1e-4f) ok = false;
    }
    {
        std::printf ("258 DUAL OSCILLATOR\n");
        WcProcessor p; p.prepareToPlay (SR, 256);
        auto& m = *p.rack.m258 (0);
        float in[24] {}, out[24] {};
        setP (p, "oa_freq1", std::log (110.0f / 5.0f) / std::log (4000.0f)); setP (p, "oa_shape1", 0.0f);
        auto hzOf = [&] (float kbd)
        {
            in[M258::inOf (0, 0)] = kbd; m.reset(); m.beginBlock (256);
            int z = 0; float last = 0.0f; const int N = (int) SR * 2;
            for (int i = 0; i < N; ++i) { m.tick (in, out); if (last < 0.0f && out[M258::Out_1] >= 0.0f) ++z; last = out[M258::Out_1]; }
            return z / 2.0;
        };
        const double f0 = hzOf (0.0f), f1 = hzOf (1.2f), f2 = hzOf (2.4f);
        std::printf ("  tracking at 1.2 V/oct: %.1f Hz, %.1f Hz, %.1f Hz (110 / 220 / 440)\n", f0, f1, f2);
        if (std::fabs (f0 - 110.0) > 1.0 || std::fabs (f1 - 220.0) > 1.5 || std::fabs (f2 - 440.0) > 2.5) ok = false;
        // waveshape: 2nd harmonic energy grows from sine to saw
        auto h2 = [&] (float shape)
        {
            setP (p, "oa_shape1", shape); in[M258::inOf (0, 0)] = 0.0f; m.reset(); m.beginBlock (256);
            double re1 = 0, im1 = 0, re2 = 0, im2 = 0; const int N = (int) SR;
            for (int i = 0; i < N; ++i) { m.tick (in, out); const double t = 6.283185307 * 110.0 * i / SR; re1 += out[0] * std::cos (t); im1 += out[0] * std::sin (t); re2 += out[0] * std::cos (2 * t); im2 += out[0] * std::sin (2 * t); }
            return 20.0 * std::log10 (std::sqrt (re2 * re2 + im2 * im2) / std::sqrt (re1 * re1 + im1 * im1) + 1e-9);
        };
        const double a = h2 (0.0f), b = h2 (1.0f);
        std::printf ("  2nd harmonic re fundamental: sine %.1f dB, saw %.1f dB (saw ideal -6.0)\n", a, b);
        if (a > -40.0 || std::fabs (b + 6.0) > 1.5) ok = false;
    }
    {
        std::printf ("264 QUAD SAMPLE & HOLD\n");
        WcProcessor p; p.prepareToPlay (SR, 256);
        auto& m = *p.rack.m264();
        float in[24] {}, out[24] {};
        setP (p, "sh_track2", 1.0f); m.reset(); m.beginBlock (256);
        // channel 1 samples, channel 2 tracks: both see a ramp with a long pulse
        in[M264::In_Pulse1] = in[M264::In_Pulse2] = 10.0f;
        float o1 = 0, o2 = 0;
        for (int i = 0; i < 100; ++i) { in[M264::In_Cv1] = in[M264::In_Cv2] = 0.01f * i; m.tick (in, out); o1 = out[M264::Out_Cv1]; o2 = out[M264::Out_Cv2]; }
        std::printf ("  long pulse over a ramp: sample holds %.2f V, track follows to %.2f V\n", o1, o2);
        if (std::fabs (o1) > 1e-4f || std::fabs (o2 - 0.99f) > 1e-4f) ok = false;
        in[M264::In_Pulse1] = in[M264::In_Pulse2] = 0.0f;
        // poly: 3 voices round robin from the common input
        setP (p, "sh_voices", 3.0f); m.reset(); m.beginBlock (256);
        float vals[4] {};
        for (int k = 0; k < 4; ++k)
        {
            in[M264::In_PolyCv] = (float) (k + 1);
            in[M264::In_Poly] = 10.0f; m.tick (in, out); in[M264::In_Poly] = 0.0f; m.tick (in, out);
        }
        for (int c = 0; c < 4; ++c) vals[c] = out[M264::Out_Cv1 + c];
        std::printf ("  poly, 3 voices, notes 1 2 3 4: voices hold %.0f %.0f %.0f %.0f (expected 4 2 3 0)\n", vals[0], vals[1], vals[2], vals[3]);
        if (vals[0] != 4.0f || vals[1] != 2.0f || vals[2] != 3.0f || vals[3] != 0.0f) ok = false;
    }
    {
        std::printf ("230 TRIPLE ENVELOPE FOLLOWER\n");
        WcProcessor p; p.prepareToPlay (SR, 256);
        auto& m = *p.rack.m230();
        float in[24] {}, out[24] {};
        setP (p, "ef_sens1", 0.4f); setP (p, "ef_thr1", 0.3f); m.reset(); m.beginBlock (256);
        int pulses = 0; bool was = false; float peak = 0.0f;
        const int N = (int) SR * 2;
        for (int i = 0; i < N; ++i)
        {
            const bool burst = (i % (int) (SR / 4)) < (int) (SR / 16);          // 4 bursts per second
            in[M230::In_Audio1] = burst ? 0.5f * std::sin (6.283185f * 440.0f * i / (float) SR) : 0.0f;
            m.tick (in, out);
            if (i > (int) SR) peak = juce::jmax (peak, out[M230::Out_Env1]);
            const bool hi = out[M230::Out_Pulse1] > 5.0f; if (hi && ! was) ++pulses; was = hi;
        }
        std::printf ("  0.5 amplitude bursts at 0 dB: envelope peak %.2f V (expected ~5), %d pulses in 2 s (expected 8)\n", peak, pulses);
        if (std::fabs (peak - 5.0f) > 0.3f || pulses != 8) ok = false;
    }

    {
        std::printf ("264 ART QUANTIZER\n");
        const float a = M264::quantize (0.33f, 2, 0), b = M264::quantize (0.33f, 0, 0), c = M264::quantize (0.61f, 2, 2);
        std::printf ("  0.33 V -> major C %.1f V, minor C %.1f V; 0.61 V -> major D %.1f V\n", a, b, c);
        if (std::fabs (a - 0.2f) > 1e-4f || std::fabs (b - 0.3f) > 1e-4f || std::fabs (c - 0.6f) > 1e-4f) ok = false;   // d# -> d (ties down) / d# / f#
    }
    {
        std::printf ("285 FREQUENCY SHIFTER\n");
        WcProcessor p; p.prepareToPlay (SR, 256);
        auto& m = *p.rack.m285();
        float in[24] {}, out[24] {};
        setP (p, "fs_ref", std::sqrt (0.1f)); m.reset(); m.beginBlock (256);   // 100 Hz shift
        const int N = (int) SR;
        auto mag = [] (const std::vector<float>& x, double f) { double re = 0, im = 0; for (size_t i = 0; i < x.size(); ++i) { const double t = 6.283185307 * f * (double) i / SR; re += x[i] * std::cos (t); im += x[i] * std::sin (t); } return std::sqrt (re * re + im * im) / (double) x.size(); };
        std::vector<float> su, di;
        for (int i = 0; i < N; ++i) { in[M285::In_FsSignal] = 0.5f * std::sin (6.283185f * 1000.0f * i / (float) SR); m.tick (in, out); if (i > 2000) { su.push_back (out[M285::Out_Sum]); di.push_back (out[M285::Out_Diff]); } }
        const double up = mag (su, 1100), upImg = mag (su, 900), dn = mag (di, 900), dnImg = mag (di, 1100);
        const double rej1 = 20 * std::log10 (upImg / up), rej2 = 20 * std::log10 (dnImg / dn);
        std::printf ("  1 kHz shifted by 100 Hz: sum at 1100 Hz (image %.1f dB), difference at 900 Hz (image %.1f dB)\n", rej1, rej2);
        if (rej1 > -35.0 || rej2 > -35.0 || up < 0.2) ok = false;
    }
    {
        std::printf ("277 SIGNAL DELAY\n");
        WcProcessor p; p.prepareToPlay (SR, 256);
        auto& m = *p.rack.m277();
        float in[24] {}, out[24] {};
        setP (p, "dl_time", 1.0f / 3.0f); setP (p, "dl_lvla", 1.0f); m.reset(); m.beginBlock (256);   // t = .10 s
        int at[4] = { -1, -1, -1, -1 };
        for (int i = 0; i < (int) SR / 2; ++i)
        {
            in[M277::In_A] = i == 100 ? 0.5f : 0.0f;
            m.tick (in, out);
            for (int k = 0; k < 4; ++k) if (at[k] < 0 && std::fabs (out[k]) > 0.02f) at[k] = i - 100;
        }
        std::printf ("  impulse taps at %.1f / %.1f / %.1f / %.1f ms (25 / 50 / 75 / 100)\n", at[0] * 1000.0 / SR, at[1] * 1000.0 / SR, at[2] * 1000.0 / SR, at[3] * 1000.0 / SR);
        for (int k = 0; k < 4; ++k) if (std::fabs (at[k] * 1000.0 / SR - 25.0 * (k + 1)) > 1.0) ok = false;
    }
    {
        std::printf ("207 MIXER\n");
        WcProcessor p; p.prepareToPlay (SR, 256);
        auto& m = *p.rack.m207();
        float in[24] {}, out[24] {};
        setP (p, "mx_lvl_a", 1.0f); setP (p, "mx_lvl_f", 1.0f); setP (p, "mx_lvl_c", 1.0f); setP (p, "mx_pan2", 0.5f); m.beginBlock (256);
        in[M207::In_A] = 0.5f; in[M207::In_F] = 0.25f; in[M207::In_C] = 0.4f;
        m.tick (in, out);
        for (int i = 0; i < 2000; ++i) m.tick (in, out);   // let the assignment c.v. lag settle
        std::printf ("  A 0.5 -> L, F 0.25 -> R, C 0.4 centred: L %.3f, R %.3f\n", out[M207::Out_L], out[M207::Out_R]);
        if (std::fabs (out[M207::Out_L] - (0.5f + 0.4f * 0.7071f)) > 1e-3f || std::fabs (out[M207::Out_R] - (0.25f + 0.4f * 0.7071f)) > 1e-3f) ok = false;
    }

    {
        std::printf ("296 SPECTRAL PROCESSOR\n");
        WcProcessor p; p.prepareToPlay (SR, 256);
        auto& m = *p.rack.m296();
        float in[48] {}, out[48] {};
        m.reset(); m.beginBlock (256);
        float e[16] {};
        for (int i = 0; i < (int) SR / 2; ++i)
        {
            in[M296::In_SigAll] = 0.5f * std::sin (6.283185f * 1000.0f * i / (float) SR);
            m.tick (in, out);
            if (i > (int) SR / 4) for (int b = 0; b < 16; ++b) e[b] = juce::jmax (e[b], out[M296::Out_Env0 + b]);
        }
        std::printf ("  1 kHz at 0.5: envelope 800 Hz %.2f V, 1k %.2f V, 1.2k %.2f V, 250 Hz %.3f V, 5k %.3f V\n", e[6], e[7], e[8], e[2], e[13]);
        if (std::fabs (e[7] - 5.0f) > 0.8f || e[2] > 0.3f || e[13] > 0.3f) ok = false;
        // programmed window: a narrow window around channel 2 (250 Hz) closes the 1 kHz output
        setP (p, "sp_freq", 2.0f / 15.0f); setP (p, "sp_width", 1.5f / 16.0f); m.beginBlock (256);
        double pa = 0, pp = 0;
        for (int i = 0; i < (int) SR / 2; ++i)
        {
            in[M296::In_SigAll] = 0.5f * std::sin (6.283185f * 1000.0f * i / (float) SR);
            m.tick (in, out);
            if (i > (int) SR / 4) { pa += out[M296::Out_AttAll] * out[M296::Out_AttAll]; pp += out[M296::Out_ProgAll] * out[M296::Out_ProgAll]; }
        }
        const double rej = 10.0 * std::log10 (pp / pa);
        std::printf ("  window on 250 Hz: programmed output %.1f dB re attenuator output\n", rej);
        if (rej > -30.0) ok = false;
    }

    {
        std::printf ("248 MARF\n");
        WcProcessor p; p.prepareToPlay (SR, 256);
        auto& m = *p.rack.m248();
        float in[48] {}, out[48] {};
        for (int k = 1; k <= 4; ++k) { setP (p, "mf_lvl" + juce::String (k), 0.1f * (float) k); setP (p, "mf_tim" + juce::String (k), 0.0f); setP (p, "mf_trange_" + juce::String (k), 1.0f); }
        setP (p, "mf_p1_1", 1.0f); setP (p, "mf_last_4", 1.0f);
        m.reset(); m.beginBlock (256);
        // stopped until a start pulse; then 1 2 3 4 1 2 .. at 20 ms per stage (range .02-.3, slider at the bottom)
        std::vector<float> seq; float lastV = -1.0f; int p1 = 0; bool was = false;
        for (int i = 0; i < (int) (SR * 0.25); ++i)
        {
            in[M248::inJ (0, 2)] = (i > 100 && i < 200) ? 10.0f : 0.0f;
            m.tick (in, out);
            const float v = out[M248::outJ (0, 3)];
            if (std::fabs (v - lastV) > 0.01f) { seq.push_back (v); lastV = v; }
            const bool hi = out[M248::outJ (0, 5)] > 5.0f; if (hi && ! was) ++p1; was = hi;
        }
        std::printf ("  voltages: "); for (size_t k = 0; k < juce::jmin<size_t> (seq.size(), 10); ++k) std::printf ("%.1f ", seq[k]); std::printf ("| pulse 1 x%d\n", p1);
        // expected 0 (before start = stage 1 at 1 V? stage 1 level .1 -> 1 V), then 2 3 4 1 2 3 4 ...
        bool good = seq.size() >= 8;
        for (size_t k = 1; good && k < seq.size(); ++k) { const float w = (float) ((int) std::lround (seq[k - 1]) % 4 + 1); if (std::fabs (seq[k] - w) > 0.05f) good = false; }
        if (! good || p1 < 2) ok = false;
        // quantize D minor: 0.33 V -> 0.3 V (d#? no: c d d# f g g# a# around d: nearest of d e f g a a# c)
        setP (p, "mf_quant_1", 1.0f); setP (p, "mf_key", 2.0f); setP (p, "mf_scale", 0.0f); setP (p, "mf_lvl1", 0.033f); setP (p, "mf_last_4", 0.0f);
        m.beginBlock (256); m.btnReset[0].store (true); m.tick (in, out); m.btnReset[0].store (false); m.tick (in, out);
        std::printf ("  stage 1 quantized to D minor: 0.33 V -> %.2f V\n", out[M248::outJ (0, 3)]);
        if (std::fabs (out[M248::outJ (0, 3)] - 0.2f) > 0.01f) ok = false;   // 3 st = d# is not in D minor: down to d
    }

    {
        std::printf ("218 TOUCH KEYBOARD\n");
        WcProcessor p; p.prepareToPlay (SR, 256);
        auto& m = *p.rack.m218();
        float in[48] {}, out[48] {};
        m.reset(); m.beginBlock (256);
        m.touchPressure.store (0.8f); m.touchKey.store (7);                 // G above the lowest C
        for (int i = 0; i < 2000; ++i) m.tick (in, out);
        std::printf ("  key 8 (G): pitch %.2f V (expected 1.90), pressure %.1f V\n", out[M218::Out_Pitch], out[M218::Out_Pressure]);
        if (std::fabs (out[M218::Out_Pitch] - 1.9f) > 0.01f || out[M218::Out_Pressure] < 7.0f) ok = false;
        m.touchKey.store (-1); m.tick (in, out);
        // arpeggiator over three MIDI notes, ascending, 10 steps per second
        setP (p, "kb_arp", 0.0f); setP (p, "kb_rate", std::log (40.0f) / std::log (60.0f)); m.beginBlock (256);
        int pulses = 0; bool was = false;
        m.reset(); for (int n : { 48, 52, 55 }) m.handleMidi (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100));
        std::set<int> pitches;
        for (int i = 0; i < (int) SR; ++i) { m.tick (in, out); const bool hi = out[M218::Out_Pulse] > 5.0f; if (hi && ! was) { ++pulses; } was = hi; if (i % 480 == 0) pitches.insert ((int) std::lround (out[M218::Out_Pitch] * 10.0f)); }
        std::printf ("  arpeggio of C E G at 20 steps/s: %d pulses in 1 s, %d distinct pitches\n", pulses, (int) pitches.size());
        if (std::abs (pulses - 20) > 1 || pitches.size() < 3) ok = false;
    }

    {
        std::printf ("250 DUAL ARBITRARY FUNCTION GENERATOR\n");
        WcProcessor p; p.prepareToPlay (SR, 256);
        auto& m = *p.rack.m250();
        float in[48] {}, out[48] {};
        for (int k = 1; k <= 16; ++k) { setP (p, "sq_v1_" + juce::String (k), 0.05f * (float) k); setP (p, "sq_t_" + juce::String (k), 0.0f); setP (p, "sq_trng_" + juce::String (k), 0.0f); }
        setP (p, "sq_loopend", 3.0f); setP (p, "sq_loopto", 2.0f); setP (p, "sq_loops", 2.0f);
        m.reset(); m.beginBlock (256);
        m.btnStart.store (true); m.tick (in, out); m.btnStart.store (false);
        std::vector<int> st; int last = -1;
        for (int i = 0; i < (int) (SR * 0.12); ++i) { m.tick (in, out); const int s = (int) std::lround (out[M250::Out_Cv1] / 0.5f); if (s != last) { st.push_back (s); last = s; } }
        std::printf ("  stages (10 ms each, loop 3 -> 2 twice): "); for (int s : st) std::printf ("%d ", s); std::printf ("\n");
        const int want[9] = { 1, 2, 3, 2, 3, 2, 3, 4, 5 };
        bool good = st.size() >= 9; for (int k = 0; good && k < 9; ++k) if (st[(size_t) k] != want[k]) good = false;
        if (! good) ok = false;
    }

    {
        std::printf ("252 POLYPHONIC RHYTHM GENERATOR\n");
        WcProcessor p; p.prepareToPlay (SR, 256);
        auto& m = *p.rack.m252();
        float in[48] {}, out[48] {};
        m.clearAll();
        for (int i = 0; i < 16; ++i) m.rgb[M252::base (10) + i].store (1);    // ring 16, red on every cell
        for (int i = 0; i < 3; ++i) m.rgb[M252::base (0) + i].store (2);      // ring 3, green on every cell
        setP (p, "rg_clk1", 11.0f); setP (p, "rg_clk2", 1.0f); setP (p, "rg_sync", 1.0f); setP (p, "rg_bpm", 120.0f);
        m.reset(); m.beginBlock (256);
        int r = 0, g = 0, c1 = 0; bool wr = false, wg = false, wc = false;
        for (int i = 0; i < (int) (SR * 8.0) - 10; ++i)   // one 16-cell master cycle at 120 BPM = 8 s
        {
            m.tick (in, out);
            const bool hr = out[M252::Out_R] > 5.0f, hg = out[M252::Out_G] > 5.0f, hc = out[M252::Out_Cell1] > 5.0f;
            r += hr && ! wr; g += hg && ! wg; c1 += hc && ! wc; wr = hr; wg = hg; wc = hc;
        }
        std::printf ("  sync on 1, 8 s: ring 16 red %d (16), ring 3 green %d (3), cell 1 %d (1)\n", r, g, c1);
        if (r != 16 || g != 3 || c1 != 1) ok = false;
    }

    // ---- 3d. a 0.1.0 patch still loads (slot remap) ----
    {
        WcProcessor p;
        juce::ValueTree t ("WCPATCH");
        t.setProperty ("version", 1, nullptr);
        juce::ValueTree mods ("MODULES");
        for (int i = 0; i < 6; ++i) mods.setProperty ("m" + juce::String (i), i <= 2 || i == 5, nullptr);
        t.addChild (mods, -1, nullptr);
        juce::ValueTree cs ("CABLES");
        auto cable = [&] (int si, int sj, int di, int dj) { juce::ValueTree c ("CABLE"); c.setProperty ("si", si, nullptr); c.setProperty ("sj", sj, nullptr); c.setProperty ("di", di, nullptr); c.setProperty ("dj", dj, nullptr); c.setProperty ("col", 0, nullptr); cs.addChild (c, -1, nullptr); };
        cable (0, 0, 1, M259::In_PoPitch);       // MIDI pitch -> old 259 A
        cable (2, M259::Out_Final1, 5, 0);       // old 259 B -> old OUTPUT L
        t.addChild (cs, -1, nullptr);
        p.patchFromTree (t);
        bool good = p.isPresent (kInst259) && p.isPresent (kInst259 + 1) && ! p.isPresent (kInst281);
        for (auto& c : p.getCables())
            good = good && (c.dstInst == kInst259 || (c.srcInst == kInst259 + 1 && c.dstInst == kInstOutput));
        std::printf ("0.1.0 PATCH COMPAT: %s (%d cables)\n", good ? "ok" : "BROKEN", (int) p.getCables().size());
        if (! good || p.getCables().size() != 2) ok = false;
    }

    // ---- 3e. 284 Quad Voltage Controlled Envelope Generator ----
    {
        std::printf ("284 QUAD ENVELOPE GENERATOR\n");
        WcProcessor p;
        p.prepareToPlay (SR, 256);
        auto& e = *p.rack.m284();
        float in[48] {}, out[48] {};
        // delay -> attack -> duration -> decay, each read off the knob
        auto kn = [] (float secs) { return std::log (secs / 0.002f) / std::log (5000.0f); };   // inverse of knobTime
        setP (p, "ev_del1", kn (0.05f)); setP (p, "ev_att1", kn (0.05f));
        setP (p, "ev_dur1", kn (0.10f)); setP (p, "ev_dec1", kn (0.10f));
        setP (p, "ev_ctl1", 0.0f);
        e.beginBlock (256);
        {
            double tRise = -1, tPeak = -1, tFall = -1, tEnd = -1;
            int pulses = 0; bool pWas = false;
            float peak = 0.0f, prev = 0.0f;
            for (int i = 0; i < (int) (SR * 0.6); ++i)
            {
                in[M284::inTrig (0)] = i < 48 ? 10.0f : 0.0f;                 // 1 ms start pulse
                e.tick (in, out);
                const float v = out[M284::outEnv (0, 0)];
                const double t = i / SR;
                if (tRise < 0 && v > 0.05f) tRise = t;                        // end of the delay
                if (v > peak) { peak = v; tPeak = t; }                        // top of the attack
                if (tPeak > 0 && tFall < 0 && v < peak - 0.05f) tFall = t;    // end of the duration
                if (tFall > 0 && tEnd < 0 && v < 0.05f) tEnd = t;             // end of the decay
                const bool pulse = out[M284::outPulse (0)] > 3.0f;
                if (pulse && ! pWas) ++pulses;
                pWas = pulse; prev = v;
            }
            juce::ignoreUnused (prev);
            std::printf ("  ch1 delay %.3f s (0.050)  attack ends %.3f s (0.100)  duration ends %.3f s (0.200)  decay ends %.3f s (0.300)  peak %.2f V  end pulses %d\n",
                         tRise, tPeak, tFall, tEnd, peak, pulses);
            if (std::fabs (tRise - 0.05) > 0.012 || std::fabs (tPeak - 0.10) > 0.015
                || std::fabs (tFall - 0.20) > 0.02 || std::fabs (tEnd - 0.30) > 0.03) ok = false;
            if (peak < 9.0f || peak > 10.5f || pulses != 1) ok = false;
        }
        // sustain: the duration holds while the input stays high
        {
            setP (p, "ev_sus1", 1.0f);
            e.beginBlock (256); e.reset();
            double tEnd = -1;
            float peak = 0.0f;
            const int gate = (int) (SR * 0.5);
            for (int i = 0; i < (int) (SR * 1.2); ++i)
            {
                in[M284::inTrig (0)] = i < gate ? 10.0f : 0.0f;
                e.tick (in, out);
                const float v = out[M284::outEnv (0, 0)];
                peak = std::max (peak, v);
                if (i > gate && tEnd < 0 && v < 0.05f) tEnd = i / SR;
            }
            std::printf ("  ch1 sustain: a 0.500 s gate holds, envelope ends %.3f s (0.600 = gate + decay)  peak %.2f V\n", tEnd, peak);
            if (std::fabs (tEnd - 0.60) > 0.05 || peak < 9.0f) ok = false;
            setP (p, "ev_sus1", 0.0f);
        }
        // the control input scales the times of its own channel only
        {
            setP (p, "ev_ctl2", 1.0f);                                        // full right: the c.v. shortens
            setP (p, "ev_del2", 0.0f); setP (p, "ev_att2", kn (0.1f));
            setP (p, "ev_dur2", 0.0f); setP (p, "ev_dec2", kn (0.1f));
            auto attackTime = [&] (float cv)
            {
                e.beginBlock (256); e.reset();
                for (auto& v : in) v = 0.0f;
                in[M284::inCtl (1)] = cv;
                float peak = 0.0f; double tPeak = 0.0;
                for (int i = 0; i < (int) (SR * 0.8); ++i)
                {
                    in[M284::inTrig (1)] = i < 48 ? 10.0f : 0.0f;
                    e.tick (in, out);
                    const float v = out[M284::outEnv (1, 0)];
                    if (v > peak) { peak = v; tPeak = i / SR; }
                }
                return tPeak;
            };
            const double t0 = attackTime (0.0f), t5 = attackTime (5.0f);
            std::printf ("  ch2 control input: attack %.3f s at 0 V, %.3f s at +5 V (shorter)\n", t0, t5);
            if (! (t5 < t0 * 0.75)) ok = false;
        }
    }

    // ---- 3f. 267 Uncertainty Source / Dual Bandpass Filter ----
    {
        std::printf ("267 UNCERTAINTY SOURCE / DUAL FILTER\n");
        WcProcessor p;
        p.prepareToPlay (SR, 256);
        auto& u = *p.rack.m267();
        float in[48] {}, out[48] {};
        // noise colour: low -3 dB/oct, flat 0, high +3 dB/oct
        {
            const int order = 14, N = 1 << order;
            juce::dsp::FFT fft (order);
            const char* names[3] = { "low", "flat", "high" };
            const int outs[3] = { M267::Out_Low, M267::Out_Flat1, M267::Out_High };
            std::vector<std::vector<float>> bufs (3, std::vector<float> ((size_t) N * 2, 0.0f));
            std::vector<double> spec[3];
            for (int k = 0; k < 3; ++k) spec[k].assign ((size_t) N / 2, 0.0);
            u.beginBlock (256);
            for (int rep = 0; rep < 16; ++rep)
            {
                for (int i = 0; i < N; ++i)
                {
                    u.tick (in, out);
                    for (int k = 0; k < 3; ++k) bufs[(size_t) k][(size_t) i] = out[outs[k]];
                }
                for (int k = 0; k < 3; ++k)
                {
                    fft.performFrequencyOnlyForwardTransform (bufs[(size_t) k].data());
                    for (int b = 0; b < N / 2; ++b) spec[k][(size_t) b] += (double) bufs[(size_t) k][(size_t) b] * bufs[(size_t) k][(size_t) b];
                    std::fill (bufs[(size_t) k].begin(), bufs[(size_t) k].end(), 0.0f);
                }
            }
            auto band = [&] (int k, double f0, double f1) { double e = 0; for (int b = (int) (f0 * N / SR); b < (int) (f1 * N / SR); ++b) e += spec[k][(size_t) b]; return e / (f1 - f0); };
            for (int k = 0; k < 3; ++k)
            {
                const double slope = 10.0 * std::log10 (band (k, 8000, 16000) / band (k, 250, 500)) / 5.0;
                std::printf ("  %-4s noise: %+5.1f dB/oct\n", names[k], slope);
                if (std::fabs (slope - (k == 0 ? -3.0 : (k == 1 ? 0.0 : 3.0))) > 0.8) ok = false;
            }
        }
        // the two flat outputs are the same source on bridged jacks
        {
            u.beginBlock (256);
            double diff = 0.0;
            for (int i = 0; i < 2000; ++i) { u.tick (in, out); diff += std::fabs (out[M267::Out_Flat1] - out[M267::Out_Flat2]); }
            std::printf ("  flat jacks bridged: total difference %.1e\n", diff);
            if (diff > 1.0e-5) ok = false;
        }
        // random source A: the number of new values follows the rate knob
        {
            auto changes = [&] (float rateKnob, double secs)
            {
                setP (p, "us_ratea", rateKnob);
                setP (p, "us_randa", 1.0f);                                   // fully random at the mix output
                u.beginBlock (256); u.reset();
                int n = 0; float last = -99.0f;
                for (int i = 0; i < (int) (SR * secs); ++i)
                {
                    in[M267::In_SmplA] = (i % 480) < 24 ? 10.0f : 0.0f;       // sample at 100 Hz
                    u.tick (in, out);
                    const float v = out[M267::Out_ShA];
                    if (std::fabs (v - last) > 0.01f) { last = v; ++n; }
                }
                return n / secs;
            };
            const double slow = changes (0.2f, 2.0), fast = changes (0.8f, 2.0);
            std::printf ("  random A: %.1f new values/s at rate 0.2 (0.6 Hz), %.1f at 0.8 (20 Hz)\n", slow, fast);
            if (! (slow < fast * 0.5) || fast < 5.0) ok = false;
            in[M267::In_SmplA] = 0.0f;
        }
        // filter A: a bandpass peak at the knob frequency, narrower as the bandwidth closes
        {
            auto peakHz = [&] (float fKnob, float bwKnob, double& q)
            {
                setP (p, "us_freqa", fKnob); setP (p, "us_bwa", bwKnob); setP (p, "us_idxa", 0.0f);
                u.beginBlock (256); u.reset();
                const int order = 14, N = 1 << order;
                juce::dsp::FFT fft (order);
                std::vector<double> spec ((size_t) N / 2, 0.0);
                std::vector<float> buf ((size_t) N * 2, 0.0f);
                for (int rep = 0; rep < 12; ++rep)
                {
                    for (int i = 0; i < N; ++i) { u.tick (in, out); buf[(size_t) i] = out[M267::Out_SigA1]; }
                    fft.performFrequencyOnlyForwardTransform (buf.data());
                    for (int b = 0; b < N / 2; ++b) spec[(size_t) b] += (double) buf[(size_t) b] * buf[(size_t) b];
                    std::fill (buf.begin(), buf.end(), 0.0f);
                }
                // smooth before looking: one noise bin must not decide the peak
                std::vector<double> sm ((size_t) N / 2, 0.0);
                for (int b = 4; b < N / 2 - 4; ++b) { double e = 0; for (int k = -4; k <= 4; ++k) e += spec[(size_t) (b + k)]; sm[(size_t) b] = e / 9.0; }
                int top = 4; for (int b = 5; b < N / 2 - 4; ++b) if (sm[(size_t) b] > sm[(size_t) top]) top = b;
                const double f = top * SR / N;
                // "how narrow": the share of the energy that sits within a third of an octave of the peak
                double in3 = 0.0, all = 0.0;
                for (int b = 4; b < N / 2 - 4; ++b)
                {
                    const double hz = b * SR / N;
                    all += sm[(size_t) b];
                    if (hz > f / 1.26 && hz < f * 1.26) in3 += sm[(size_t) b];
                }
                q = in3 / std::max (1e-30, all);
                return f;
            };
            double qNarrow = 0, qWide = 0;
            const double f1 = peakHz (0.5f, 0.0f, qNarrow);                   // narrow
            const double f2 = peakHz (0.5f, 1.0f, qWide);                     // wide
            const double want = M267::freqHz (0.5f);
            std::printf ("  filter A at knob 0.5: peak %.0f Hz, nominal %.0f Hz;  energy within a third of an octave: %.0f %% narrow, %.0f %% wide\n",
                         f1, want, 100.0 * qNarrow, 100.0 * qWide);
            if (std::fabs (f1 - want) / want > 0.25 || std::fabs (f2 - want) / want > 0.35) ok = false;
            if (! (qNarrow > qWide * 1.2)) ok = false;
        }
    }

    // ---- 3g. 275 Dual Equalizer / Reverb ----
    {
        std::printf ("275 DUAL EQUALIZER / REVERB\n");
        WcProcessor p;
        p.prepareToPlay (SR, 256);
        auto& r = *p.rack.m275();
        float in[48] {}, out[48] {};
        // shelving equalizer: +-10 dB at 50 Hz and 10 kHz, read at the "equalized only" output
        {
            auto levelAt = [&] (double hz, float treble, float bass)
            {
                setP (p, "rv_treb1", treble); setP (p, "rv_bass1", bass); setP (p, "rv_mix1", 0.0f);
                r.beginBlock (256); r.reset();
                double acc = 0.0;
                const int n = (int) (SR * 0.5);
                for (int i = 0; i < n; ++i)
                {
                    in[M275::In_Audio1] = 2.0f * std::sin (2.0 * 3.14159265358979 * hz * i / SR);
                    r.tick (in, out);
                    if (i > n / 2) acc += (double) out[M275::Out_Eq1] * out[M275::Out_Eq1];
                }
                return 20.0 * std::log10 (std::sqrt (acc / (n / 2)) / 1.4142 + 1e-12);
            };
            const double flat1k = levelAt (1000.0, 0.0f, 0.0f);
            const double hiBoost = levelAt (10000.0, 10.0f, 0.0f) - levelAt (10000.0, 0.0f, 0.0f);
            const double loCut = levelAt (50.0, 0.0f, -10.0f) - levelAt (50.0, 0.0f, 0.0f);
            std::printf ("  1 kHz flat %+.2f dB   treble +10 at 10 kHz: %+.1f dB   bass -10 at 50 Hz: %+.1f dB\n", flat1k, hiBoost, loCut);
            if (std::fabs (flat1k) > 0.6 || std::fabs (hiBoost - 10.0) > 2.5 || std::fabs (loCut + 10.0) > 2.5) ok = false;
        }
        // the tank rings on after the input stops, and the mix knob sets how much of it reaches the mix output
        {
            auto tail = [&] (float mix, float recede, double& dryPeak)
            {
                setP (p, "rv_treb1", 0.0f); setP (p, "rv_bass1", 0.0f);
                setP (p, "rv_mix1", mix); setP (p, "rv_rec1", recede);
                r.beginBlock (256); r.reset();
                double late = 0.0; dryPeak = 0.0;
                const int burst = (int) (SR * 0.05), total = (int) (SR * 1.5);
                for (int i = 0; i < total; ++i)
                {
                    in[M275::In_Audio1] = i < burst ? 3.0f * std::sin (2.0 * 3.14159265358979 * 440.0 * i / SR) : 0.0f;
                    r.tick (in, out);
                    if (i < burst) dryPeak = std::max (dryPeak, (double) std::fabs (out[M275::Out_Mix1a]));
                    if (i > burst + (int) (SR * 0.2)) late += (double) out[M275::Out_Mix1a] * out[M275::Out_Mix1a];
                }
                return std::sqrt (late / (total - burst - (int) (SR * 0.2)));
            };
            double dryDrySide = 0, dryWetSide = 0, dryRecede = 0;
            const double dry = tail (0.0f, 0.0f, dryDrySide);
            const double wet = tail (0.8f, 0.0f, dryWetSide);
            const double rec = tail (0.8f, 1.0f, dryRecede);
            std::printf ("  tail after 0.2 s: dry %.2e, wet %.2e;  dry level near %.2f vs recede %.2f\n", dry, wet, dryDrySide, dryRecede);
            if (! (wet > dry * 20.0 + 1e-6) || ! (dryRecede < dryWetSide * 0.9)) ok = false;
            // "reverberated only" carries the tank alone, so it keeps ringing with the mix knob down
            if (! std::isfinite (rec)) ok = false;
        }
    }

    // ---- 3h. 291 Triple Morphing Filter ----
    {
        std::printf ("291 TRIPLE MORPHING FILTER\n");
        WcProcessor p;
        p.prepareToPlay (SR, 256);
        auto& m = *p.rack.m291();
        float in[48] {}, out[48] {};
        auto spectrumPeak = [&] (int jack, int reps)
        {
            const int order = 14, N = 1 << order;
            juce::dsp::FFT fft (order);
            std::vector<double> spec ((size_t) N / 2, 0.0);
            std::vector<float> buf ((size_t) N * 2, 0.0f);
            juce::Random rng (7);
            for (int rep = 0; rep < reps; ++rep)
            {
                for (int i = 0; i < N; ++i)
                {
                    in[M291::In_SigAll] = 3.0f * (rng.nextFloat() * 2.0f - 1.0f);
                    m.tick (in, out);
                    buf[(size_t) i] = out[jack];
                }
                fft.performFrequencyOnlyForwardTransform (buf.data());
                for (int b = 0; b < N / 2; ++b) spec[(size_t) b] += (double) buf[(size_t) b] * buf[(size_t) b];
                std::fill (buf.begin(), buf.end(), 0.0f);
            }
            int top = 1; for (int b = 2; b < N / 2; ++b) if (spec[(size_t) b] > spec[(size_t) top]) top = b;
            return top * SR / N;
        };
        // each node filters at its own stage-1 frequency
        {
            setP (p, "tm_play", 0.0f); setP (p, "tm_edit", 1.0f); setP (p, "tm_solo", 0.0f);
            const float f[3] = { 0.30f, 0.55f, 0.80f };
            for (int nd = 0; nd < 3; ++nd)
            {
                const juce::String c = juce::String::charToString ((juce::juce_wchar) ('a' + nd));
                setP (p, "tm_f_1_" + c, f[nd]);
                setP (p, "tm_bw_1_" + c, 0.1f);
                setP (p, "tm_amp_1_" + c, 1.0f);
            }
            m.beginBlock (256); m.reset();
            for (int i = 0; i < 4096; ++i) { in[M291::In_SigAll] = 0.0f; m.tick (in, out); }
            const int outs[3] = { M291::Out_A, M291::Out_B, M291::Out_C };
            for (int nd = 0; nd < 3; ++nd)
            {
                const double got = spectrumPeak (outs[nd], 8), want = M291::freqHz (f[nd]);
                std::printf ("  node %c peak %.0f Hz (nominal %.0f Hz)\n", 'A' + nd, got, want);
                if (std::fabs (got - want) / want > 0.3) ok = false;
            }
        }
        // play: the morph walks from stage 1 to stage 2 over the stage time
        {
            setP (p, "tm_f_1_a", 0.2f); setP (p, "tm_f_2_a", 0.9f);
            setP (p, "tm_time_1", 0.5f);                                       // ~0.3 s
            setP (p, "tm_play", 1.0f);
            m.beginBlock (256); m.reset();
            const float p0 = m.position.load();
            for (int i = 0; i < (int) (SR * 0.25); ++i) { in[M291::In_SigAll] = 0.0f; m.tick (in, out); }
            const float p1 = m.position.load();
            for (int i = 0; i < (int) (SR * 1.0); ++i) { in[M291::In_SigAll] = 0.0f; m.tick (in, out); }
            const float p2 = m.position.load();
            std::printf ("  morph position: %.2f -> %.2f after 0.25 s -> %.2f after 1.25 s (0..8 stages)\n", p0, p1, p2);
            if (! (p1 > p0 + 0.05f) || ! (p2 > p1)) ok = false;
            setP (p, "tm_play", 0.0f);
        }
        // solo sends only the selected node to "all"; the two all jacks are bridged
        {
            setP (p, "tm_solo", 1.0f); setP (p, "tm_node", 0.0f);
            m.beginBlock (256); m.reset();
            double diff = 0.0;
            juce::Random rng (3);
            for (int i = 0; i < 4000; ++i)
            {
                in[M291::In_SigAll] = 3.0f * (rng.nextFloat() * 2.0f - 1.0f);
                m.tick (in, out);
                diff += std::fabs (out[M291::Out_All1] - out[M291::Out_All2]);
            }
            std::printf ("  all jacks bridged under solo: total difference %.1e\n", diff);
            if (diff > 1.0e-4) ok = false;
            setP (p, "tm_solo", 0.0f);
        }
    }

    // ---- 3i. SC-666 oscilloscope ----
    {
        std::printf ("SC-666 OSCILLOSCOPE\n");
        WcProcessor p;
        p.prepareToPlay (SR, 256);
        auto& s = *p.rack.m666 (0);
        float in[48] {}, out[48] {};
        const juce::String pre (prefix666 (0));
        setP (p, pre + "power", 1.0f);
        setP (p, pre + "src", 0.0f);                                           // INT
        setP (p, pre + "mode", 0.0f);                                          // AUTO
        setP (p, pre + "coupling", 1.0f);                                      // DC
        setP (p, pre + "level", 0.0f);
        setP (p, pre + "time", 0.5f);
        // a 100 Hz sine on A: frames arrive, the sweep ramps, and A passes through untouched
        {
            s.beginBlock (256); s.reset();
            M666::Frame fr; int seq = -1; int frames = 0;
            double sweepMin = 1e9, sweepMax = -1e9, thruErr = 0.0;
            for (int i = 0; i < (int) (SR * 0.8); ++i)
            {
                const float v = 5.0f * std::sin (2.0 * 3.14159265358979 * 100.0 * i / SR);
                in[M666::In_A] = v;
                s.tick (in, out);
                thruErr += std::fabs (out[M666::Out_AThru] - v);
                sweepMin = std::min (sweepMin, (double) out[M666::Out_Sweep]);
                sweepMax = std::max (sweepMax, (double) out[M666::Out_Sweep]);
                if (s.latest (fr, seq)) ++frames;
            }
            std::printf ("  frames in 0.8 s: %d   sweep %.1f .. %.1f V   A thru error %.1e   points %d\n",
                         frames, sweepMin, sweepMax, thruErr, fr.n);
            if (frames < 2 || fr.n < 16 || thruErr > 1.0e-3) ok = false;
            if (sweepMax < 9.0 || sweepMin > 0.5) ok = false;
            // the captured trace must carry the sine, not silence
            float lo = 1e9f, hi = -1e9f;
            for (int i = 0; i < fr.n; ++i) { lo = std::min (lo, fr.a[i]); hi = std::max (hi, fr.a[i]); }
            std::printf ("  trace A spans %.2f .. %.2f\n", lo, hi);
            if (hi - lo < 0.2f) ok = false;
        }
        // GND coupling parks the trace, power off stops the frames
        {
            setP (p, pre + "coupling", 2.0f);                                  // GND
            s.beginBlock (256);
            M666::Frame fr; int seq = -1;
            for (int i = 0; i < (int) (SR * 0.4); ++i)
            {
                in[M666::In_A] = 5.0f * std::sin (2.0 * 3.14159265358979 * 100.0 * i / SR);
                s.tick (in, out);
                s.latest (fr, seq);
            }
            float lo = 1e9f, hi = -1e9f;
            for (int i = 0; i < fr.n; ++i) { lo = std::min (lo, fr.a[i]); hi = std::max (hi, fr.a[i]); }
            std::printf ("  GND coupling: trace spans %.3f .. %.3f (flat)\n", lo, hi);
            if (fr.n > 0 && hi - lo > 0.02f) ok = false;
            setP (p, pre + "coupling", 1.0f);
        }
        // four independent instances
        {
            int n = 0;
            for (int i = 0; i < kMax666; ++i) if (p.rack.m666 (i) != nullptr) ++n;
            std::printf ("  instances available: %d\n", n);
            if (n != 4) ok = false;
        }
    }

    // ---- 3j. licensing: only a signed key file unlocks the rack ----
    {
        std::printf ("LICENSE\n");
        auto demo = [] (const juce::String& blob, const char* what)
        {
            const auto l = wc::License::validate (blob);
            std::printf ("  %-28s -> %s\n", what, l.licensed() ? "LICENSED (wrong!)" : "demo");
            return ! l.licensed();
        };
        if (! demo ("", "empty")) ok = false;
        if (! demo ("hello", "not a key file")) ok = false;
        if (! demo ("name=Someone\nver=1\nsig=00\n", "zero signature")) ok = false;
        if (! demo ("name=Someone\nemail=a@b.c\norder=X\nissued=20260101\nver=1\n"
                    "sig=deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef\n", "made-up signature")) ok = false;
        // a key file signed with a key that is not Calabi Yau's must not validate either
        {
            juce::RSAKey pub, priv;
            juce::RSAKey::createKeyPair (pub, priv, 512);
            const auto blob = wc::License::makeKeyFile ("Impostor", "x@y.z", "NONE", priv.toString());
            if (! demo (blob, "signed with another key")) ok = false;
        }
        std::printf ("  demo mode allows %d oscillators and no preset load / save\n", wc::License::kDemoMaxOscillators);
    }

    // ---- a knob that jumps glides 12 ms, as the 292: no step in the output ----
    // Each module straight, a steady input, the knob jumps between two blocks: the largest change from one sample
    // to the next has to be a small part of the whole change (a jump puts all of it, or most, in one sample).
    {
        std::printf ("KNOB GLIDE\n");
        struct Case { int inst; const char* id; float from, to; int in; float x; int out; std::vector<std::pair<const char*, float>> also; };
        const std::vector<Case> cases = {
            { kInst207, "mx_lvl_a", 0.0f, 1.0f, M207::In_A, 1.0f, M207::Out_L, { { "mx_prog_a", 1.0f } } },
            { kInst207, "mx_pan1", 0.0f, 1.0f, M207::In_B, 1.0f, M207::Out_R, { { "mx_prog_b", 1.0f }, { "mx_lvl_b", 1.0f } } },
            { kInst207, "mx_gain", 0.0f, 1.0f, M207::In_Pre, 0.001f, M207::Out_Pre, {} },
            { kInst296, "sp_att0", 0.5f, 1.0f, M296::In_SigAll, 1.0f, M296::Out_Band0, {} },
            { kInst285, "fs_amp", 0.0f, 1.0f, M285::In_BmSignal, 1.0f, M285::Out_Var, {} },
            { kInst277, "dl_lvla", 0.0f, 1.0f, M277::In_A, 0.1f, M277::Out_Q1, {} },
            { kInst257, "vp_k1", 0.0f, 1.0f, M257::In_Va1, 1.0f, M257::Out_1, {} },
            { kInst257, "vp_off1", 0.0f, 1.0f, M257::In_Va1, 0.0f, M257::Out_1, {} },
            { kInst275, "rv_bass1", -10.0f, 10.0f, M275::In_Audio1, 1.0f, M275::Out_Eq1, {} },
        };
        for (auto& c : cases)
        {
            WcProcessor p;
            p.prepareToPlay (SR, 64);
            for (auto& a : c.also) setP (p, a.first, a.second);
            setP (p, c.id, c.from);
            auto* m = p.rack.module (c.inst);
            m->prepare (SR);
            float in[64] {}, out[64] {};
            in[c.in] = c.x;
            auto run = [&] (int blocks, std::vector<float>* rec)
            {
                for (int b = 0; b < blocks; ++b)
                {
                    m->beginBlock (64);
                    for (int i = 0; i < 64; ++i) { m->tick (in, out); if (rec != nullptr) rec->push_back (out[c.out]); }
                }
            };
            run ((int) SR / 64, nullptr);                    // settle
            const float before = out[c.out];
            setP (p, c.id, c.to);
            std::vector<float> y { before };
            run ((int) SR / 128, &y);
            float step = 0.0f;
            for (size_t i = 1; i < y.size(); ++i) step = std::max (step, std::abs (y[i] - y[i - 1]));
            const float whole = std::abs (y.back() - before);
            const bool good = whole > 0.0f && step < 0.005f * whole;     // a 12 ms ramp: 0.17 %
            std::printf ("  %-9s %g -> %g: whole change %.4g, largest sample step %.2f %% of it %s\n", c.id, c.from, c.to, whole, 100.0f * step / std::max (whole, 1.0e-12f), good ? "ok" : "BAD");
            if (! good) ok = false;
        }
    }

    // ---- the ramp lands in 12 ms with short blocks re-sending the same target, and it is curved ----
    {
        dsp::Ramp r; r.snap (0.0f);
        const int len = 576;                              // 12 ms at 48 kHz
        float quarter = 0.0f;
        for (int n = 0; n < len; n += 32)
        {
            r.to (1.0f, len);                             // every block re-sends the knob, unchanged
            for (int i = 0; i < 32; ++i) { const float v = r.next(); if (n + i + 1 == len / 4) quarter = v; }
        }
        // a raised cosine is at (1 - cos (pi / 4)) / 2 = 0.146 a quarter of the way; a straight line at 0.25
        const bool good = r.v == 1.0f && r.left == 0 && std::abs (quarter - 0.1464f) < 0.002f;
        std::printf ("RAMP blocks of 32, same target: %.6f after %d samples (want 1), %.4f at a quarter (want 0.1464) %s\n", r.v, len, quarter, good ? "ok" : "BAD");
        if (! good) ok = false;
    }

    // ---- rows on screen: LAYOUT comes back as it went, a hand-edited one is cleaned up, every state carries one ----
    {
        auto text = [] (const WcProcessor& p)
        {
            juce::String t;
            for (auto& r : p.layout()) { t << "["; for (int i : r) t << instKey (i) << " "; t = t.trimEnd() + "] "; }
            return t.trimEnd();
        };
        auto fromXml = [] (WcProcessor& p, const char* xml) { if (auto x = juce::parseXML (xml)) p.patchFromTree (juce::ValueTree::fromXml (*x)); };
        WcProcessor p;
        p.license.status = License::Status::Licensed;
        p.loadBuiltinPatch (12);                               // 245 Sequence: five modules in two rows
        const auto before = text (p);
        p.moveModule (kInstOutput, 0, 0);                     // OUTPUT first
        p.moveModule (kInst259, (int) p.layout().size(), 0);  // the 259 alone in a new third row
        WcProcessor q;
        q.license.status = License::Status::Licensed;
        q.patchFromTree (p.patchToTree());
        const bool trip = q.layout() == p.layout() && text (q) == "[out midi sv] [la] [a]";
        // by hand: an unknown key, a repeat, a module not in the rack, one in the rack left out, a row past 4008
        fromXml (q, "<WCPATCH name=\"x\" version=\"3\"><MODULES midi=\"1\" a=\"1\" fa=\"1\" la=\"1\" out=\"1\" kb=\"1\" mf=\"1\" s1=\"1\" s2=\"1\" sv=\"1\"/>"
                    "<CABLES/><LAYOUT><ROW m=\"zz la a la b\"/><ROW m=\"kb mf s1 s2 sv\"/></LAYOUT></WCPATCH>");
        const auto edited = text (q);
        fromXml (q, "<WCPATCH name=\"y\" version=\"3\"><MODULES midi=\"1\" a=\"1\" out=\"1\"/><CABLES/><LAYOUT><ROW m=\"out\"/></LAYOUT></WCPATCH>");
        const auto oneRow = text (q);
        WcProcessor demo;                                       // the state of a demo session carries the rows too
        juce::MemoryBlock st;
        demo.getStateInformation (st);
        const auto xml = demo.getXmlFromBinary (st.getData(), (int) st.getSize());
        const bool saved = xml != nullptr && xml->getChildByName ("WCPATCH") != nullptr && xml->getChildByName ("WCPATCH")->getChildByName ("LAYOUT") != nullptr;
        const bool good = trip && edited == "[la a] [kb mf s1 s2 sv midi fa out]" && oneRow == "[out midi a] []" && saved && before == "[midi sv a] [la out]";
        std::printf ("ROWS %s -> %s, back %s; by hand %s; %s; demo state with LAYOUT %s %s\n", before.toRawUTF8(), text (p).toRawUTF8(),
                     trip ? "the same" : "DIFFERENT", edited.toRawUTF8(), oneRow.toRawUTF8(), saved ? "yes" : "NO", good ? "ok" : "BAD");
        if (! good) ok = false;
    }

    // ---- MIDI Learn: a learned CC moves its parameter and stops there; a sleeping one goes on ----
    {
        std::printf ("MIDI LEARN\n");
        auto send = [] (WcProcessor& p, std::initializer_list<juce::MidiMessage> msgs)
        {
            juce::AudioBuffer<float> buf (2, 64);
            juce::MidiBuffer midi;
            int at = 0;
            for (auto& m : msgs) midi.addEvent (m, at++);
            p.processBlock (buf, midi);
            p.flushMidiMoves();                          // what the processor's timer does
        };
        auto norm = [] (WcProcessor& p, const char* id) { return p.apvts.getParameter (id)->getValue(); };
        auto cc = [] (int ch, int n, int v) { return juce::MidiMessage::controllerEvent (ch, n, v); };
        auto mod = [] (WcProcessor& p) { return p.rack.jackValue (kInstMidi, MidiInterface::Out_Mod); };

        // CC 1 mapped to out_level: CC 1 = 127 (on channel 3: omni) takes the level to 1 and the MOD output holds
        {
            WcProcessor p;
            p.prepareToPlay (SR, 64);
            p.loadBuiltinPatch (0);
            setP (p, "out_level", 0.0f);
            send (p, { cc (1, 1, 64) });                 // not mapped yet: it reaches the MIDI module
            const float before = mod (p);
            const bool mapped = p.assignCc (1, "out_level");
            send (p, { cc (3, 1, 127) });
            const bool good = mapped && std::abs (before - 10.0f * 64.0f / 127.0f) < 1.0e-4f && mod (p) == before && std::abs (norm (p, "out_level") - 1.0f) < 1.0e-6f;
            std::printf ("  CC 1 -> out_level, CC 1 = 127: level %.3f (1), MOD out %.2f V (holds %.2f) %s\n", norm (p, "out_level"), mod (p), before, good ? "ok" : "BAD");
            if (! good) ok = false;
        }
        // the audio thread only writes the raw value (heard this block) and marks it: no listener, host or knob
        // hears of it until the timer. Then the parameter, the knob, the host (one gesture, closed 250 ms after the
        // last CC) and the saved state all have it, also when the state had been saved before (the adapter then
        // has nothing to flush) and when the host saves before the timer.
        {
            struct Host : juce::AudioProcessorListener
            {
                int changes = 0, begins = 0, ends = 0;
                void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override { ++changes; }
                void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override {}
                void audioProcessorParameterChangeGestureBegin (juce::AudioProcessor*, int) override { ++begins; }
                void audioProcessorParameterChangeGestureEnd (juce::AudioProcessor*, int) override { ++ends; }
            } host;
            WcProcessor p;
            p.prepareToPlay (SR, 64);
            p.loadBuiltinPatch (0);
            setP (p, "out_level", 0.0f);
            p.assignCc (1, "out_level");
            juce::Slider knob;
            juce::AudioProcessorValueTreeState::SliderAttachment att (p.apvts, "out_level", knob);
            juce::MemoryBlock saved;
            p.getStateInformation (saved);               // a save before: nothing left for the adapter to flush
            p.addListener (&host);
            auto block = [&] (int v) { juce::AudioBuffer<float> buf (2, 64); juce::MidiBuffer m; m.addEvent (cc (1, 1, v), 0); p.processBlock (buf, m); };
            auto savedLevel = [&]
            {
                juce::MemoryBlock st;
                p.getStateInformation (st);
                WcProcessor q;
                q.setStateInformation (st.getData(), (int) st.getSize());
                return q.apvts.getRawParameterValue ("out_level")->load();
            };
            block (127);
            const bool audioOnly = host.changes == 0 && p.apvts.getRawParameterValue ("out_level")->load() == 1.0f && knob.getValue() == 0.0;
            const float early = savedLevel();            // the host saves before the timer
            p.flushMidiMoves();
            const bool passed = host.changes == 1 && host.begins == 1 && host.ends == 0 && norm (p, "out_level") == 1.0f && knob.getValue() == 1.0;
            const float late = savedLevel();
            block (0);
            p.flushMidiMoves();
            const bool oneGesture = host.begins == 1 && host.ends == 0 && savedLevel() == 0.0f;
            juce::Thread::sleep (300);
            p.flushMidiMoves();
            const bool closed = host.ends == 1;
            p.removeListener (&host);
            std::printf ("  CC from the audio thread: raw value at once, nobody told %s; after the timer: parameter, knob, host %s; saved %.2f before the timer, %.2f after (1) %s; one gesture %s, closed after 250 ms %s\n",
                         audioOnly ? "ok" : "BAD", passed ? "ok" : "BAD", early, late, early == 1.0f && late == 1.0f ? "ok" : "BAD", oneGesture ? "ok" : "BAD", closed ? "ok" : "BAD");
            if (! audioOnly || ! passed || early != 1.0f || late != 1.0f || ! oneGesture || ! closed) ok = false;

            // a 14-bit controller sends n and n + 32: a learn is offered n; CC 33 on its own is offered
            auto offered = [&] (std::initializer_list<juce::MidiMessage> msgs) { send (p, msgs); return (int) (p.lastLearnableCc.load() & 0xffu); };
            const int pair = offered ({ cc (1, 7, 64), cc (1, 39, 3) });
            const int lone = offered ({ cc (1, 74, 1), cc (1, 33, 5) });
            const int lone2 = offered ({ cc (1, 33, 6) });
            const bool fourteen = pair == 7 && lone2 == 33;
            std::printf ("  14-bit CC 7 + 39: offered %d (7); CC 33 alone after CC 74: %d then %d (33) %s\n", pair, lone, lone2, fourteen ? "ok" : "BAD");
            if (! fourteen) ok = false;
        }
        // a mapping to a module out of the rack sleeps: CC 1 still reaches MOD, and the parameter stays put; the
        // module back in the rack, the mapping wakes up. A 259 C mapping sleeps in demo (no third oscillator).
        {
            WcProcessor p;
            p.prepareToPlay (SR, 64);
            p.loadBuiltinPatch (0);
            setP (p, "fa_att1", 500.0f);
            const float att = norm (p, "fa_att1");
            const bool absent = ! p.isPresent (kInst281);
            p.assignCc (1, "fa_att1");
            send (p, { cc (1, 1, 127) });
            const bool asleep = absent && std::abs (mod (p) - 10.0f) < 1.0e-4f && norm (p, "fa_att1") == att && ! p.ccIsLive (1);
            p.setPresent (kInst281, true);
            send (p, { cc (1, 1, 0) });
            const bool awake = p.ccIsLive (1) && norm (p, "fa_att1") == 0.0f && std::abs (mod (p) - 10.0f) < 1.0e-4f;
            bool demoAsleep = true;
            if (p.demo())
            {
                setP (p, "c_timbre", 0.0f);
                p.setPresent (kInst259 + 1, true);       // two oscillators: the most demo mode allows
                p.setPresent (kInst259 + 2, true);       // refused
                p.assignCc (2, "c_timbre");
                send (p, { cc (1, 2, 127) });
                demoAsleep = ! p.isPresent (kInst259 + 2) && norm (p, "c_timbre") == 0.0f && p.ccOf ("c_timbre") == 2;
            }
            std::printf ("  asleep (281 out of the rack): MOD out %.2f V, attack kept %s; 281 in: CC 1 = 0 moves it %s; 259 C in demo asleep %s\n",
                         mod (p), asleep ? "ok" : "BAD", awake ? "ok" : "BAD", demoAsleep ? "ok" : "BAD");
            if (! asleep || ! awake || ! demoAsleep) ok = false;
        }
        // only CC 1-119, without 6, 32, 38 and 98-101 (data entry, bank LSB, NRPN / RPN); one CC per parameter
        {
            WcProcessor p;
            juce::StringArray wrong;
            for (int n : { 0, 6, 32, 38, 98, 99, 100, 101, 120, 121, 123, 127, -1, 128 })
                if (p.assignCc (n, "a_timbre")) wrong.add (juce::String (n));
            for (int n : { 1, 7, 31, 33, 37, 39, 97, 102, 119 })
                if (! WcProcessor::isLearnableCc (n)) wrong.add ("not " + juce::String (n));
            const bool none = wrong.isEmpty() && p.midiMapEmpty();
            // CC 0 and 120 coming in are not offered to a learn either
            p.prepareToPlay (SR, 64);
            const auto last = p.lastLearnableCc.load();
            send (p, { cc (1, 0, 5), cc (1, 120, 0) });
            const bool notOffered = p.lastLearnableCc.load() == last;
            send (p, { cc (1, 74, 5) });
            const bool offered = (p.lastLearnableCc.load() & 0xffu) == 74 && p.lastLearnableCc.load() != last;
            std::printf ("  CCs that cannot be learned: %s; not offered to a learn %s, CC 74 offered %s\n",
                         none ? "refused ok" : ("TAKEN " + wrong.joinIntoString (" ")).toRawUTF8(), notOffered ? "ok" : "BAD", offered ? "ok" : "BAD");
            if (! none || ! notOffered || ! offered) ok = false;

            // CC 74 was 259 A Timbre's: learned on 245 Time, it moves there; and a parameter keeps one CC only
            juce::String moved;
            p.assignCc (74, "a_timbre");
            p.assignCc (74, "sv_time", &moved);
            const bool moves = moved == "a_timbre" && p.ccOf ("a_timbre") < 0 && p.ccOf ("sv_time") == 74;
            const auto movedFrom = moved;
            p.assignCc (75, "sv_time", &moved);
            const bool single = moved.isEmpty() && p.ccOf ("sv_time") == 75 && p.paramOfCc (74).isEmpty();
            std::printf ("  CC 74 learned on another control: moved from \"%s\" %s; one CC per parameter %s\n",
                         movedFrom.toRawUTF8(), moves ? "ok" : "BAD", single ? "ok" : "BAD");
            if (! moves || ! single) ok = false;
            // a stepped parameter lands on a step: CC 64 on a three-way lever is its middle
            p.assignCc (20, "la_mode1");
            p.setPresent (kInst292, true);
            send (p, { cc (1, 20, 70) });
            const bool step = norm (p, "la_mode1") == 0.5f;
            std::printf ("  CC 70 on a three-way lever: %.3f (0.5) %s\n", norm (p, "la_mode1"), step ? "ok" : "BAD");
            if (! step) ok = false;
        }
        // saved: with the project (it replaces the map; a project without MIDIMAP leaves it empty) and in a patch
        // (a patch keeps the map there is; only an empty map takes the patch's)
        {
            WcProcessor a;
            a.assignCc (74, "sv_time");
            a.assignCc (1, "out_level");
            juce::MemoryBlock state;
            a.getStateInformation (state);
            WcProcessor b;
            b.assignCc (20, "a_timbre");
            b.setStateInformation (state.getData(), (int) state.getSize());
            const bool project = b.ccOf ("sv_time") == 74 && b.ccOf ("out_level") == 1 && b.ccOf ("a_timbre") < 0;
            WcProcessor none;
            juce::MemoryBlock plain;
            none.getStateInformation (plain);
            const bool noNode = ! juce::String::fromUTF8 ((const char*) plain.getData(), (int) plain.getSize()).contains ("MIDIMAP");
            b.setStateInformation (plain.getData(), (int) plain.getSize());
            const bool emptied = b.midiMapEmpty();
            std::printf ("  project: map comes back %s, replaces the one there was %s; without MIDIMAP: empty %s (and none written %s)\n",
                         project ? "ok" : "BAD", project ? "ok" : "BAD", emptied ? "ok" : "BAD", noNode ? "ok" : "BAD");
            if (! project || ! emptied || ! noNode) ok = false;

            // patch files need a license; this test has no key, so it lends the processors one
            const auto file = outDir.getChildFile ("midimap.wcpatch");
            a.license.status = wc::License::Status::Licensed;
            const bool saved = a.savePatchFile (file) && file.loadFileAsString().contains ("<MAP cc=\"74\" param=\"sv_time\"/>");
            WcProcessor c;
            c.license.status = wc::License::Status::Licensed;
            const bool loaded = c.loadPatchFile (file);
            const bool adopted = loaded && c.ccOf ("sv_time") == 74 && c.ccOf ("out_level") == 1;
            WcProcessor d;
            d.license.status = wc::License::Status::Licensed;
            d.assignCc (30, "a_timbre");
            d.loadPatchFile (file);
            const bool kept = d.ccOf ("a_timbre") == 30 && d.ccOf ("sv_time") < 0 && d.paramOfCc (74).isEmpty();
            d.loadBuiltinPatch (3);
            const bool builtinKeeps = d.ccOf ("a_timbre") == 30;
            std::printf ("  patch: saved %s, into an empty map adopted %s, an existing map kept %s, built-in patch keeps it %s\n",
                         saved ? "ok" : "BAD", adopted ? "ok" : "BAD", kept ? "ok" : "BAD", builtinKeeps ? "ok" : "BAD");
            if (! saved || ! adopted || ! kept || ! builtinKeeps) ok = false;
            file.deleteFile();
        }
    }

    // ---- typed values: every float parameter reads its own text back ----
    // "440 Hz" typed in the host or in the value editor must land on 440 Hz. For each float parameter and 101
    // positions, getValueForText (getText (v)) has to come back to v within what the text shows.
    {
        WcProcessor p;
        int floats = 0, bad = 0;
        juce::StringArray fails;
        for (auto* par : p.getParameters())
        {
            if (dynamic_cast<juce::AudioParameterFloat*> (par) == nullptr) continue;
            auto* f = dynamic_cast<juce::RangedAudioParameter*> (par);   // getText / getValueForText are public here
            ++floats;
            for (int i = 0; i <= 100; ++i)
            {
                const float v = f->convertTo0to1 (f->convertFrom0to1 ((float) i / 100.0f));   // a value the parameter can hold
                const auto text = f->getText (v, 64);
                const float back = f->getValueForText (text);
                if (f->getText (back, 64) == text || std::abs (back - v) < 1.0e-5f || sameShownValue (text, f->getText (back, 64))) continue;
                if (++bad <= 8) fails.add (f->getParameterID() + " \"" + text + "\" -> \"" + f->getText (back, 64) + "\"");
            }
        }
        std::printf ("typed values: %d float parameters x 101 positions read their own text back, %d misses %s\n",
                     floats, bad, bad == 0 ? "ok" : "BAD");
        for (auto& s : fails) std::printf ("  %s\n", s.toRawUTF8());
        if (bad != 0) ok = false;

        // and the texts are rounded: juce::String (x, 0) does not round, so "141 ms" came out "141.421 ms". Two
        // decimals at most; 277 Time Scale is the one text with three ("0.050 s (x4 long)").
        int ugly = 0;
        juce::StringArray uglies;
        for (auto* par : p.getParameters())
        {
            auto* f = dynamic_cast<juce::RangedAudioParameter*> (par);   // getText is public here
            if (dynamic_cast<juce::AudioParameterFloat*> (par) == nullptr || f->getParameterID() == "dl_time") continue;
            for (int i = 0; i <= 100; ++i)
            {
                const auto text = f->getText ((float) i / 100.0f, 64);
                const auto frac = text.fromFirstOccurrenceOf (".", false, false).initialSectionContainingOnly ("0123456789");
                if (frac.length() <= 2) continue;
                if (++ugly <= 8) uglies.add (f->getParameterID() + " \"" + text + "\"");
            }
        }
        std::printf ("value texts: %d with more than two decimals %s\n", ugly, ugly == 0 ? "ok" : "BAD");
        for (auto& s : uglies) std::printf ("  %s\n", s.toRawUTF8());
        if (ugly != 0) ok = false;

        // fixed cases: what a person types
        struct Case { const char* id; const char* text; const char* shown; float want; float tol; int limited; };   // want: plain; NAN = not a value
        const Case cases[] = {
            { "a_po_freq", "1,2k",     "",         coarseNorm (1200.0f, false), 1.0e-4f, 0 },   // comma decimal, k = x1000, on Hz
            { "a_po_freq", "440 Hz",   "",         coarseNorm (440.0f, false),  1.0e-4f, 0 },
            { "a_po_freq", "1.2 KHZ",  "",         coarseNorm (1200.0f, false), 1.0e-4f, 0 },   // unit capitals do not matter
            { "fa_att1",   "250",      "120 ms",   0.25f * (float) std::log10 (250.0),  1.0e-4f, 0 },   // bare number = the unit shown: 250 ms
            { "fa_att1",   "2",        "1.20 s",   0.25f * (float) std::log10 (2000.0), 1.0e-4f, 0 },   // ... 2 s
            { "fa_att1",   "50 %",     "",         0.5f,  1.0e-6f, 0 },                     // % on a time = position of the knob
            { "fa_att1",   "440 Hz",   "",         NAN,   0.0f,    0 },                     // a unit of another kind
            { "fa_att1",   "1 h",      "",         NAN,   0.0f,    0 },
            { "fa_att1",   "",         "",         NAN,   0.0f,    0 },
            { "fa_att1",   "1 min",    "",         NAN,   0.0f,    0 },
            { "fa_att1",   "1 ms",     "",         0.0f,  1.0e-6f, 0 },
            { "fa_att1",   "0.5 ms",   "",         0.0f,  1.0e-6f, -1 },                    // below the range: limited
            { "a_timbre",  "200 %",    "",         1.0f,  1.0e-6f, 1 },                     // limited to the maximum
            { "a_timbre",  "37",       "50 %",     0.37f, 1.0e-6f, 0 },
            { "a_mo_cv",   "+50 %",    "",         0.5f,  1.0e-6f, 0 },                     // shown in %: the shown value (75 % of the knob)
            { "a_mo_cv",   "\xe2\x88\x92" "25 %", "", -0.25f, 1.0e-6f, 0 },                  // typographic minus
            { "pp_period", "off",      "",         1.0f,  1.0e-6f, 0 },
            { "pp_period", "OFF",      "",         1.0f,  1.0e-6f, 0 },
            { "sp_att0",   "off",      "",         0.0f,  1.0e-6f, 0 },
            { "sp_att0",   "-6 dB",    "",         30.0f / 42.0f, 1.0e-5f, 0 },
            { "pp_len",    "7.6",      "",         8.0f,  0.0f,    0 },                     // stepped: rounds to the step
            { "rg_bpm",    "96 bpm",   "",         96.0f, 0.0f,    0 },
            { "rg_clk1",   "16",       "",         11.0f, 0.0f,    0 },                     // a label that is a number
            { "tm_node",   "c",        "",         2.0f,  0.0f,    0 },
            { "mf_mult1",  "x2",       "",         (float) (std::log (4.0) / std::log (8.0)), 1.0e-5f, 0 },
            { "mf_mult1",  "2x",       "",         (float) (std::log (4.0) / std::log (8.0)), 1.0e-5f, 0 },
            { "out_lag",   "0.1 s",    "",         100.0f, 1.0e-3f, 0 },
            { "fa_mode1",  "cyc",      "",         2.0f,  0.0f,    0 },                     // choice: start of the label
            { "fa_mode1",  "2",        "",         1.0f,  0.0f,    0 },                     // ... or its position
            { "fa_mode1",  "banana",   "",         NAN,   0.0f,    0 },
        };
        int caseBad = 0;
        for (auto& c : cases)
        {
            auto* par = p.apvts.getParameter (c.id);
            const auto r = par != nullptr ? wc::parseValueText (*par, juce::String (juce::CharPointer_UTF8 (c.text)), c.shown) : std::nullopt;
            const bool want = ! std::isnan (c.want);
            const bool good = par != nullptr && r.has_value() == want && (! want || (std::abs (r->plain - c.want) <= c.tol && r->limited == c.limited));
            if (! good)
            {
                ++caseBad;
                std::printf ("  typed \"%s\" on %s: got %s, want %s  BAD\n", c.text, c.id,
                             r ? (juce::String (r->plain, 6) + " limited " + juce::String (r->limited)).toRawUTF8() : "nothing",
                             want ? (juce::String (c.want, 6) + " limited " + juce::String (c.limited)).toRawUTF8() : "nothing");
            }
        }
        // the host path: before, "440 Hz" read as 440 and pinned the knob to its maximum
        auto* po = p.apvts.getParameter ("a_po_freq");
        const float host = coarseHz (po->convertFrom0to1 (po->getValueForText ("440 Hz")), false);
        if (std::abs (host - 440.0f) > 0.05f) { ++caseBad; std::printf ("  host \"440 Hz\" -> %.2f Hz  BAD\n", host); }
        std::printf ("typed values: %d fixed cases, %s\n", (int) (sizeof (cases) / sizeof (cases[0])) + 1, caseBad == 0 ? "ok" : "BAD");
        if (caseBad != 0) ok = false;
    }

    // ---- 4. CPU ----
    {
        WcProcessor p;
        p.prepareToPlay (SR, 256);
        p.loadBuiltinPatch (6);
        for (int i = 2; i < kMax259; ++i) p.setPresent (kInst259 + i, true);
        run (p, 256);
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        run (p, (int) SR * 10, -1, 0, 48);
        const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
        std::printf ("CPU: four 259s, 10 s of audio in %.0f ms (%.1f %% of one core)\n", ms, ms / 100.0);
    }

    // ---- 5. renders ----
    {
        const auto names = WcProcessor::builtinPatchNames();
        for (int k = 0; k < names.size(); ++k)
        {
            WcProcessor p;
            p.prepareToPlay (SR, 256);
            p.loadBuiltinPatch (k);
            std::vector<float> L, R;
            const int notes[] = { 36, 43, 48, 51, 55, 60, 48, 41 };
            bool bad = false;
            float peak = 0.0f;
            for (int n = 0; n < 8; ++n)
            {
                for (int s = 0; s < (int) (SR * 0.5); s += 256)
                {
                    juce::AudioBuffer<float> buf (2, 256);
                    juce::MidiBuffer midi;
                    if (s == 0) midi.addEvent (juce::MidiMessage::noteOn (1, notes[n], (juce::uint8) 100), 0);
                    if (s == 256 * 70) midi.addEvent (juce::MidiMessage::noteOff (1, notes[n]), 0);
                    p.processBlock (buf, midi);
                    for (int i = 0; i < 256; ++i)
                    {
                        const float l = buf.getSample (0, i), r = buf.getSample (1, i);
                        if (! std::isfinite (l) || ! std::isfinite (r)) bad = true;
                        peak = std::max (peak, std::max (std::fabs (l), std::fabs (r)));
                        L.push_back (l); R.push_back (r);
                    }
                }
            }
            writeWav (outDir.getChildFile (juce::String (k) + " " + names[k] + ".wav"), L, R);
            std::printf ("RENDER %-20s peak %.3f %s\n", names[k].toRawUTF8(), peak, bad ? "NaN!" : "");
            if (bad || peak < 0.01f) ok = false;
        }
    }
    std::printf ("%s\n", ok ? "ALL OK" : "FAILED");
    return ok ? 0 : 1;
}
