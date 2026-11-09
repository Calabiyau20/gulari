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
    // ---- feature sweep: every spectral sound, sampler sound, filter FX type, seq direction, reverb ----
    auto renderCase = [&] (const juce::String& name, std::function<void (SynthgularityProcessor&)> setup)
    {
        SynthgularityProcessor proc;
        proc.setPlayConfigDetails (0, 2, sr, block);
        proc.resetToDefaults();
        setup (proc);
        proc.prepareToPlay (sr, block);
        const int total = (int) (sr * 1.5);
        juce::AudioBuffer<float> buf (2, block);
        float peak = 0.0f; bool finite = true;
        for (int pos = 0; pos < total; pos += block)
        {
            juce::MidiBuffer midi;
            if (pos == 0) { midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), 10); midi.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 10); }
            if (pos <= (int) (sr * 1.0) && pos + block > (int) (sr * 1.0)) { midi.addEvent (juce::MidiMessage::noteOff (1, 48), 0); midi.addEvent (juce::MidiMessage::noteOff (1, 64), 0); }
            buf.clear();
            proc.processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < block; ++i) { const float v = buf.getSample (ch, i); if (! std::isfinite (v)) finite = false; peak = std::max (peak, std::abs (v)); }
        }
        const bool ok = finite && peak > 0.005f && peak <= 1.0f;
        if (! ok) ++failures;
        std::cout << (ok ? "OK   " : "FAIL ") << name.paddedRight (' ', 34) << " peak " << juce::String (peak, 3)
                  << "  TP " << juce::String (proc.meter.tpMax.load(), 1) << " dBTP  M " << juce::String (proc.meter.lufsM.load(), 1) << " LUFS\n";
    };
    auto setP = [] (SynthgularityProcessor& pr, const char* id, float v) { if (auto* q = pr.apvts.getParameter (id)) q->setValueNotifyingHost (q->convertTo0to1 (v)); };
    for (int k = 0; k < 20; ++k)
        renderCase ("Spectral " + pg::choices::spectralSounds()[k], [&] (SynthgularityProcessor& pr)
        { setP (pr, "gr_on", 0); setP (pr, "sp_on", 1); setP (pr, "sp_sound", (float) k); setP (pr, "sp_scan", 0.5f); });
    for (int k = 0; k < 12; ++k)
        renderCase ("Sampler " + pg::choices::samplerSounds()[k], [&] (SynthgularityProcessor& pr)
        { setP (pr, "gr_on", 0); setP (pr, "smp_on", 1); setP (pr, "smp_sound", (float) k); setP (pr, "smp_decay", 1.0f); });
    for (int k = 0; k < 7; ++k)
        for (int poles : { 1, 4, 8 })
            renderCase ("Filter FX " + pg::choices::filterFxTypes()[k] + " p" + juce::String (poles), [&] (SynthgularityProcessor& pr)
            { setP (pr, "gr_on", 0); setP (pr, "wtA_on", 1); setP (pr, "ff1_on", 1); setP (pr, "ff1_type", (float) k); setP (pr, "ff1_poles", (float) poles);
              setP (pr, "ff1_res", 0.9f); setP (pr, "ff1_drive", 0.5f); });
    renderCase ("Reverb", [&] (SynthgularityProcessor& pr) { setP (pr, "gr_on", 0); setP (pr, "wtA_on", 1); setP (pr, "fx_rv_on", 1); setP (pr, "fx_rv_mix", 1.0f); setP (pr, "fx_rv_decay", 20.0f); });
    renderCase ("WT C + double warp", [&] (SynthgularityProcessor& pr) { setP (pr, "gr_on", 0); setP (pr, "wtC_on", 1); setP (pr, "wtC_warp", 9); setP (pr, "wtC_wamt", 0.8f); setP (pr, "wtC_warp2", 13); setP (pr, "wtC_wamt2", 0.7f); setP (pr, "fm_c_c", 0.5f); });
    for (int d = 0; d < 7; ++d)
        renderCase ("Seq dir " + pg::choices::seqDirections()[d], [&] (SynthgularityProcessor& pr)
        { setP (pr, "gr_on", 0); setP (pr, "wtA_on", 1); setP (pr, "seq8_dir", (float) d); setP (pr, "seq8_div", 0); setP (pr, "mtx01_src", (float) pg::MS_Seq8); setP (pr, "mtx01_dst", (float) pg::MD_WtALevel); setP (pr, "mtx01_amt", -1.0f); setP (pr, "wtA_level", 1.0f); });
    for (int c = 0; c < 4; ++c)
        renderCase ("Chaos 1 " + pg::choices::chaosTypes()[c] + " + Lorenz", [&] (SynthgularityProcessor& pr)
        { setP (pr, "gr_on", 0); setP (pr, "wtA_on", 1); setP (pr, "chaos1_type", (float) c); setP (pr, "chaos1_rate", 20.0f);
          setP (pr, "mtx01_src", (float) pg::MS_Chaos1); setP (pr, "mtx01_dst", (float) pg::MD_WtAPitch); setP (pr, "mtx01_amt", 0.5f);
          setP (pr, "mtx02_src", (float) pg::MS_Chaos2Z); setP (pr, "mtx02_dst", (float) pg::MD_Cutoff); setP (pr, "mtx02_amt", 0.5f); });
    const char* mbOn[] = { "", "mb_tp_on", "mb_eq_on", "mb_cp_on", "mb_ch_on", "mb_im_on", "mb_tg_on", "mb_dj_on", "mb_bc_on", "mb_tr_on", "mb_lm_on" };
    for (int k = 1; k <= 10; ++k)
        renderCase ("MixBus " + pg::choices::mixbusTypes()[k], [&] (SynthgularityProcessor& pr)
        { setP (pr, "gr_on", 0); setP (pr, "wtA_on", 1); setP (pr, mbOn[k], 1); setP (pr, "mb_dj_pos", -0.5f); setP (pr, "mb_eq_low", 6); setP (pr, "mb_lm_in", 12); });
    for (int o = 1; o <= 2; ++o)
        renderCase ("Oversampling " + pg::choices::oversamplingModes()[o], [&] (SynthgularityProcessor& pr)
        { setP (pr, "gr_on", 0); setP (pr, "wtA_on", 1); setP (pr, "wtA_warp", 6); setP (pr, "wtA_wamt", 0.8f); setP (pr, "bite_drive", 0.8f); setP (pr, "oversampling", (float) o); setP (pr, "sp_on", 1); setP (pr, "pl_on", 1); });
    for (int w = 1; w <= 8; ++w)
        renderCase ("Spectral warp " + pg::choices::spectralWarps()[w], [&] (SynthgularityProcessor& pr)
        { setP (pr, "gr_on", 0); setP (pr, "sp_on", 1); setP (pr, "sp_warp", (float) w); setP (pr, "sp_wamt", 0.7f); setP (pr, "sp_pdecay", 0.5f); setP (pr, "sp_jitter", 0.5f); });
    for (int t = 0; t < 5; ++t)
        renderCase ("FM A>Spectral " + pg::choices::fmModes()[t], [&] (SynthgularityProcessor& pr)
        { setP (pr, "gr_on", 0); setP (pr, "sp_on", 1); setP (pr, "wtA_on", 1); setP (pr, "wtA_level", 0); setP (pr, "fm_a_s", 0.7f); setP (pr, "fm_s_b", 0.5f); setP (pr, "wtB_on", 1); setP (pr, "fm_type_s", (float) t); });
    renderCase ("Routing: all bypass filter", [&] (SynthgularityProcessor& pr)
    { setP (pr, "gr_on", 0); setP (pr, "wtA_on", 1); setP (pr, "sp_on", 1); setP (pr, "flt_cutoff", 20); for (auto id : { "rt_flt_a", "rt_flt_sp" }) setP (pr, id, 0); setP (pr, "rt_order", 1); });
    renderCase ("Seq lane route + arp prob", [&] (SynthgularityProcessor& pr)
    { setP (pr, "gr_on", 0); setP (pr, "wtA_on", 1); setP (pr, "seq3_dst", (float) pg::MD_Cutoff); setP (pr, "seq3_amt", 1.0f); setP (pr, "seq5_dst", (float) pg::MD_MbDjFilter); setP (pr, "arp_on", 1);
      for (int k = 0; k < 32; ++k) { pr.synth.steps.arpProb[(size_t) k].store (0.5f); pr.synth.steps.arpGate[(size_t) k].store (0.3f); } });
    std::cout << (failures == 0 ? "ALL PASSED" : "FAILURES: " + juce::String (failures)) << "\n";
    return failures == 0 ? 0 : 1;
}
