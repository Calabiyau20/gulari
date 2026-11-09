// Offline render of every factory preset: level, CPU time, NaN check, optional WAVs.
#include "RProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <chrono>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir = argc > 1 ? juce::File (juce::String (argv[1])) : juce::File();
    ResonatorProcessor proc;
    const double sr = 48000.0;
    const int block = 256;
    proc.prepareToPlay (sr, block);
    auto names = ResonatorProcessor::factoryPresetNames();
    names.insert (0, "Init");
    bool ok = true;
    for (int i = 0; i < names.size(); ++i)
    {
        if (i == 0) proc.initPreset(); else proc.loadFactoryPreset (i - 1);
        proc.prepareToPlay (sr, block);
        const int total = (int) (sr * 4.0);
        juce::AudioBuffer<float> all (2, total);
        const bool mono = proc.value (cyr::poly) < 1.5f;
        const int notes[] = { 48, 55, 60, 63 };
        double peak = 0, sum = 0, winSum = 0, maxWin = 0;
        int winCount = 0;
        const int win = (int) (sr * 0.4);
        auto t0 = std::chrono::steady_clock::now();
        for (int pos = 0; pos < total; pos += block)
        {
            juce::AudioBuffer<float> buf (2, block);
            juce::MidiBuffer midi;
            if (pos == 0) for (int k = 0; k < (mono ? 1 : 4); ++k) midi.addEvent (juce::MidiMessage::noteOn (1, notes[k] - (mono ? 12 : 0), (juce::uint8) 100), 0);
            if (pos <= (int) (sr * 2.5) && pos + block > (int) (sr * 2.5))
                for (int k = 0; k < 4; ++k) midi.addEvent (juce::MidiMessage::noteOff (1, notes[k] - (mono ? 12 : 0)), 0);
            proc.processBlock (buf, midi);
            for (int c = 0; c < 2; ++c)
            {
                all.copyFrom (c, pos, buf, c, 0, juce::jmin (block, total - pos));
                for (int s = 0; s < block; ++s)
                {
                    const float v = buf.getSample (c, s);
                    if (! std::isfinite (v)) ok = false;
                    peak = std::max (peak, (double) std::abs (v));
                    sum += (double) v * v;
                    if (c == 0)
                    {
                        const float r2 = buf.getSample (1, s);
                        winSum += 0.5 * ((double) v * v + (double) r2 * r2);
                        if (++winCount == win) { maxWin = std::max (maxWin, winSum / win); winSum = 0; winCount = 0; }
                    }
                }
            }
        }
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        const double rms = std::sqrt (sum / (2.0 * total));
        std::printf ("%-28s peak %6.3f  rms %6.3f (%5.1f dB)  cpu %5.1f%%  voices %d\n", names[i].toRawUTF8(), peak, rms,
                     20.0 * std::log10 (rms + 1e-9), secs / 4.0 * 100.0, proc.synth.activeVoices.load());
        if (peak < 0.02) ok = false;
        // suggested master trim: loudest 400 ms window at -15 dB RMS
        const double target = -15.0, now = 10.0 * std::log10 (maxWin + 1e-12);
        std::printf ("    TRIM \"%s\" %.1f\n", names[i].toRawUTF8(), juce::jlimit (-36.0, 6.0, (double) proc.value (cyr::master) + (target - now)));
        if (outDir != juce::File())
        {
            outDir.createDirectory();
            auto f = outDir.getChildFile (juce::File::createLegalFileName (names[i]) + ".wav");
            f.deleteFile();
            juce::WavAudioFormat wav;
            if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0)))
                w->writeFromAudioSampleBuffer (all, 0, total);
        }
    }
    std::printf (ok ? "ALL OK\n" : "FAILED\n");
    return ok ? 0 : 1;
}
