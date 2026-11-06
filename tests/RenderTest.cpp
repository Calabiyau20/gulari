// Offline render test: plays a short phrase through every factory preset and writes WAVs.
// Usage: SynthgularityRenderTest <outDir> [sampleFile]
#include "PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>

static bool writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double sr)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
    if (os == nullptr) return false;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, 2, 24, {}, 0));
    if (w == nullptr) return false;
    os.release();
    return w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();
    const double sr = 48000.0;
    const int block = 256;

    auto names = SynthgularityProcessor::factoryPresetNames();
    int failures = 0;
    for (int preset = 0; preset < names.size(); ++preset)
    {
        SynthgularityProcessor proc;
        proc.setPlayConfigDetails (0, 2, sr, block);
        if (argc > 2 && preset > 0 && preset <= 4)
        {
            juce::String err;
            if (! proc.loadSample (juce::File (juce::String (argv[2])), err)) { std::cout << "sample load failed: " << err << "\n"; ++failures; }
        }
        proc.loadFactoryPreset (preset);
        proc.prepareToPlay (sr, block);

        const int total = (int) (sr * 6.0);
        juce::AudioBuffer<float> out (2, total);
        juce::AudioBuffer<float> buf (2, block);
        const int notes[] = { 48, 55, 60, 63 };
        auto t0 = juce::Time::getMillisecondCounterHiRes();
        for (int pos = 0; pos < total; pos += block)
        {
            const int n = juce::jmin (block, total - pos);
            buf.setSize (2, n, false, false, true);
            juce::MidiBuffer midi;
            for (int k = 0; k < 4; ++k)
            {
                const int on = (int) (sr * (0.1 + 0.9 * k)), off = (int) (sr * (0.1 + 0.9 * k + 0.8));
                if (on >= pos && on < pos + n) midi.addEvent (juce::MidiMessage::noteOn (1, notes[k], (juce::uint8) 100), on - pos);
                if (off >= pos && off < pos + n) midi.addEvent (juce::MidiMessage::noteOff (1, notes[k]), off - pos);
            }
            // a held chord at the end
            const int chordOn = (int) (sr * 3.8), chordOff = (int) (sr * 5.0);
            for (int nn : { 48, 55, 60 })
            {
                if (chordOn >= pos && chordOn < pos + n) midi.addEvent (juce::MidiMessage::noteOn (1, nn, (juce::uint8) 90), chordOn - pos);
                if (chordOff >= pos && chordOff < pos + n) midi.addEvent (juce::MidiMessage::noteOff (1, nn), chordOff - pos);
            }
            proc.processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, buf, ch, 0, n);
        }
        const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
        const float peak = out.getMagnitude (0, total);
        const float rms = out.getRMSLevel (0, 0, total);
        bool finite = true;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < total; ++i) if (! std::isfinite (out.getSample (ch, i))) finite = false;
        const bool ok = finite && peak > 0.01f && peak <= 1.0f;
        if (! ok) ++failures;
        std::cout << (ok ? "OK   " : "FAIL ") << names[preset].paddedRight (' ', 26) << " peak " << juce::String (peak, 3)
                  << "  rms " << juce::String (rms, 4) << "  cpu x" << juce::String (6000.0 / ms, 1) << " realtime\n";
        writeWav (outDir.getChildFile (juce::String (preset).paddedLeft ('0', 2) + " " + names[preset] + ".wav"), out, sr);
    }
    std::cout << (failures == 0 ? "ALL PASSED" : "FAILURES: " + juce::String (failures)) << "\n";
    return failures == 0 ? 0 : 1;
}
