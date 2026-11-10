// Headless checks for the West Coast rack and the CW-259:  WcRenderTest [outDir]
//  1. pitch tracking at 1.2 V/oct over 6 octaves (cents error of the principal sine)
//  2. Timbre sweep: level and aliasing (energy off the harmonic series) at 110 Hz and 1760 Hz
//  3. phase lock: frequency of a detuned 259 B with and without lock
//  4. CPU with four 259s
//  5. renders every built-in patch to WAV
#include "WcProcessor.h"
#include "modules/M259.h"
#include "modules/MidiInterface.h"
#include <juce_dsp/juce_dsp.h>
#include <cstdio>

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
            setP (p, "b_lock", lock);
            run (p, 2400);
            auto a = run (p, (int) SR * 2, kInst259, M259::Out_Sine, 48);
            auto b = run (p, (int) SR * 2, kInst259 + 1, M259::Out_Sine);
            const double fa = measureHz (a, 24000), fb = measureHz (b, 24000);
            std::printf ("  lock %.1f: A %.3f Hz  B %.3f Hz  (%+.2f cents)\n", lock, fa, fb, 1200.0 * std::log2 (fb / fa));
            juce::AudioBuffer<float> bb (2, 16); juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOff (1, 48), 0); p.processBlock (bb, m);
            if (lock > 0.5f && std::fabs (1200.0 * std::log2 (fb / fa)) > 1.0) ok = false;
        }
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
