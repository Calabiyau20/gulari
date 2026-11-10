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
#include <juce_dsp/juce_dsp.h>
#include <cstdio>
#include <set>

using namespace wc;
static constexpr double SR = 48000.0;

static void setP (WcProcessor& p, const juce::String& id, float v)
{
    if (auto* x = p.apvts.getParameter (id)) x->setValueNotifyingHost (x->convertTo0to1 (v));
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
