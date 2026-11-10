#include "WcProcessor.h"
#include "modules/M259.h"
#include "modules/MidiInterface.h"
#include "modules/OutputModule.h"
#if ! WC_HEADLESS
 #include "WcEditor.h"
#endif

using namespace wc;

WcProcessor::WcProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout()),
      rack (apvts)
{
    present[(size_t) kInstMidi] = present[(size_t) kInstOutput] = true;
    loadBuiltinPatch (0);
}

WcProcessor::~WcProcessor() = default;

bool WcProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() || l.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

void WcProcessor::prepareToPlay (double sampleRate, int)
{
    rack.prepare (sampleRate);
    keyboardState.reset();
}

void WcProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    rack.beginBlock (n);
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    auto it = midi.cbegin();
    for (int i = 0; i < n; ++i)
    {
        // MIDI is applied at its exact sample position
        while (it != midi.cend() && (*it).samplePosition <= i) { rack.handleMidi ((*it).getMessage()); ++it; }
        rack.tick();
        L[i] = rack.outL();
        if (R != nullptr) R[i] = rack.outR();
    }
    for (; it != midi.cend(); ++it) rack.handleMidi ((*it).getMessage());
}

// ------------------------------------------------------------------------------------------------
// patching
// ------------------------------------------------------------------------------------------------
int WcProcessor::nextColour (bool banana)
{
    // banana cords cycle red, black, blue, yellow, green, white; mini cords dark grey / black
    juce::ignoreUnused (banana);
    return colourCounter++ % 6;
}

bool WcProcessor::connect (int srcInst, int srcJack, int dstInst, int dstJack, int colour)
{
    if (srcInst < 0 || dstInst < 0 || srcInst >= kNumInst || dstInst >= kNumInst) return false;
    const auto& ss = specFor (typeOfInst (srcInst));
    const auto& ds = specFor (typeOfInst (dstInst));
    if (srcJack < 0 || srcJack >= ss.numOut() || dstJack < 0 || dstJack >= ds.numIn()) return false;
    const auto& sj = ss.jacks[(size_t) ss.jackIndex (true, srcJack)];
    const auto& dj = ds.jacks[(size_t) ds.jackIndex (false, dstJack)];
    if (sj.kind != dj.kind) return false;          // banana to banana, mini to mini
    cables.erase (std::remove_if (cables.begin(), cables.end(), [&] (const Cable& c) { return c.dstInst == dstInst && c.dstJack == dstJack; }), cables.end());
    Cable c { srcInst, srcJack, dstInst, dstJack, colour >= 0 ? colour : nextColour (sj.kind == JackKind::Banana) };
    cables.push_back (c);
    commit();
    return true;
}

void WcProcessor::disconnectInput (int inst, int inJack)
{
    cables.erase (std::remove_if (cables.begin(), cables.end(), [&] (const Cable& c) { return c.dstInst == inst && c.dstJack == inJack; }), cables.end());
    commit();
}

void WcProcessor::disconnectOutput (int inst, int outJack)
{
    cables.erase (std::remove_if (cables.begin(), cables.end(), [&] (const Cable& c) { return c.srcInst == inst && c.srcJack == outJack; }), cables.end());
    commit();
}

void WcProcessor::setPresent (int inst, bool on)
{
    if (inst == kInstMidi || inst == kInstOutput) return;
    present[(size_t) inst] = on;
    if (! on)
        cables.erase (std::remove_if (cables.begin(), cables.end(), [&] (const Cable& c) { return c.srcInst == inst || c.dstInst == inst; }), cables.end());
    commit();
}

void WcProcessor::commit()
{
    rack.setPatch (cables, present);
    sendChangeMessage();
}

// ------------------------------------------------------------------------------------------------
// patches
// ------------------------------------------------------------------------------------------------
void WcProcessor::setParam (const juce::String& id, float plain)
{
    if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (plain));
}

void WcProcessor::resetParams()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p)) rp->setValueNotifyingHost (rp->getDefaultValue());
}

juce::StringArray WcProcessor::builtinPatchNames()
{
    return { "Init Voice", "FM Bell", "Folded Bass", "Order Sweep", "Timbre Wobble", "Ring Mod Drone", "Cross-FM Pair", "Phase Lock Unison" };
}

void WcProcessor::loadBuiltinPatch (int index)
{
    resetParams();
    cables.clear();
    present.fill (false);
    present[(size_t) kInstMidi] = present[(size_t) kInstOutput] = true;
    colourCounter = 0;
    const int A = kInst259, B = kInst259 + 1;
    using MI = MidiInterface;
    auto voice = [&] (int inst)
    {
        present[(size_t) inst] = true;
        cables.push_back ({ kInstMidi, MI::Out_Pitch, inst, M259::In_PoPitch, nextColour (true) });
        cables.push_back ({ kInstMidi, MI::Out_Pitch2, inst, M259::In_MoPitch, nextColour (true) });
    };
    auto gateToOut = [&] { cables.push_back ({ kInstMidi, MI::Out_Gate, kInstOutput, OutputModule::In_LevelCv, nextColour (true) }); };
    auto toOut = [&] (int inst, int outIn) { cables.push_back ({ inst, M259::Out_Final1, kInstOutput, outIn, nextColour (false) }); };
    const juce::String a (prefix259 (0)), b (prefix259 (1));
    auto hz = [] (float f) { return coarseNorm (f, false); };

    switch (index)
    {
        default:
        case 0:   // a plain voice: keyboard to both oscillators, gate to the output level
            voice (A); toOut (A, OutputModule::In_Left); gateToOut();
            setParam (a + "timbre", 0.25f);
            break;
        case 1:   // FM Bell: modulator at 3.5 : 1, slow release on the output lag
            voice (A); toOut (A, OutputModule::In_Left); gateToOut();
            setParam (a + "mo_freq", hz (65.406f * 3.5f)); setParam (a + "idx", 0.45f); setParam (a + "mod_pitch", 1.0f);
            setParam (a + "timbre", 0.12f); setParam ("out_lag", 300.0f); setParam ("midi_trans", 1.0f);
            break;
        case 2:   // Folded Bass: heavy Timbre, asymmetric folding, a little FM at the octave
            voice (A); toOut (A, OutputModule::In_Left); gateToOut();
            setParam (a + "timbre", 0.62f); setParam (a + "sym", 0.64f); setParam (a + "mo_freq", hz (65.406f * 2.0f));
            setParam (a + "idx", 0.12f); setParam (a + "mod_pitch", 1.0f); setParam ("midi_trans", -1.0f); setParam ("out_lag", 30.0f);
            break;
        case 3:   // Order Sweep: the modulation oscillator in LOW range sweeps Order through its CV output
            voice (A); toOut (A, OutputModule::In_Left); gateToOut();
            setParam (a + "mo_range", 1.0f); setParam (a + "mo_freq", coarseNorm (0.12f, true)); setParam (a + "mo_wave", 2.0f);
            setParam (a + "timbre", 0.45f); setParam (a + "sym", 0.3f);
            cables.erase (std::remove_if (cables.begin(), cables.end(), [&] (const Cable& c) { return c.dstInst == A && c.dstJack == M259::In_MoPitch; }), cables.end());
            cables.push_back ({ A, M259::Out_MoCv, A, M259::In_OrderCv, nextColour (true) });
            break;
        case 4:   // Timbre Wobble: LOW-range triangle modulating Timbre through the Mod Index
            voice (A); toOut (A, OutputModule::In_Left); gateToOut();
            setParam (a + "mo_range", 1.0f); setParam (a + "mo_freq", coarseNorm (2.2f, true)); setParam (a + "mo_wave", 2.0f);
            setParam (a + "timbre", 0.35f); setParam (a + "idx", 0.55f); setParam (a + "mod_timbre", 1.0f); setParam (a + "sym", 0.42f);
            cables.erase (std::remove_if (cables.begin(), cables.end(), [&] (const Cable& c) { return c.dstInst == A && c.dstJack == M259::In_MoPitch; }), cables.end());
            break;
        case 5:   // Ring Mod Drone: balanced modulator at full index, inharmonic ratio, no gate (drone)
            voice (A); toOut (A, OutputModule::In_Left);
            setParam (a + "mo_freq", hz (65.406f * 1.414f)); setParam (a + "idx", 0.85f); setParam (a + "mod_amp", 1.0f);
            setParam (a + "timbre", 0.3f); setParam (a + "order", 0.35f); setParam ("out_level", 0.6f);
            break;
        case 6:   // Cross-FM Pair: two 259s modulating each other's principal (feedback patch), stereo
            voice (A); voice (B);
            toOut (A, OutputModule::In_Left); toOut (B, OutputModule::In_Right); gateToOut();
            setParam (b + "po_freq", hz (65.406f * 1.5f));
            cables.push_back ({ B, M259::Out_Sine, A, M259::In_PoFm, nextColour (false) });
            cables.push_back ({ A, M259::Out_Sine, B, M259::In_PoFm, nextColour (false) });
            setParam (a + "po_fm", 0.18f); setParam (b + "po_fm", 0.1f);
            setParam (a + "timbre", 0.3f); setParam (b + "timbre", 0.2f); setParam ("out_lag", 120.0f);
            break;
        case 7:   // Phase Lock Unison: B detuned, then locked to A's sine - the beating disappears
            voice (A); voice (B);
            toOut (A, OutputModule::In_Left); toOut (B, OutputModule::In_Right); gateToOut();
            setParam (b + "po_fine", 0.25f); setParam (b + "lock", 0.8f);
            cables.push_back ({ A, M259::Out_Sine, B, M259::In_Lock, nextColour (false) });
            setParam (a + "timbre", 0.4f); setParam (b + "timbre", 0.4f); setParam (b + "order", 0.6f);
            break;
    }
    patchName = builtinPatchNames()[juce::jlimit (0, builtinPatchNames().size() - 1, index)];
    commit();
}

juce::ValueTree WcProcessor::patchToTree() const
{
    juce::ValueTree t ("WCPATCH");
    t.setProperty ("name", patchName, nullptr);
    t.setProperty ("version", 1, nullptr);
    juce::ValueTree mods ("MODULES");
    for (int i = 0; i < kNumInst; ++i) mods.setProperty ("m" + juce::String (i), present[(size_t) i], nullptr);
    t.addChild (mods, -1, nullptr);
    juce::ValueTree cs ("CABLES");
    for (auto& c : cables)
    {
        juce::ValueTree ct ("CABLE");
        ct.setProperty ("si", c.srcInst, nullptr); ct.setProperty ("sj", c.srcJack, nullptr);
        ct.setProperty ("di", c.dstInst, nullptr); ct.setProperty ("dj", c.dstJack, nullptr);
        ct.setProperty ("col", c.colour, nullptr);
        cs.addChild (ct, -1, nullptr);
    }
    t.addChild (cs, -1, nullptr);
    return t;
}

void WcProcessor::patchFromTree (const juce::ValueTree& t)
{
    if (! t.hasType ("WCPATCH")) return;
    patchName = t.getProperty ("name", "Patch").toString();
    present.fill (false);
    auto mods = t.getChildWithName ("MODULES");
    for (int i = 0; i < kNumInst; ++i) present[(size_t) i] = (bool) mods.getProperty ("m" + juce::String (i), false);
    present[(size_t) kInstMidi] = present[(size_t) kInstOutput] = true;
    cables.clear();
    for (auto ct : t.getChildWithName ("CABLES"))
        cables.push_back ({ (int) ct["si"], (int) ct["sj"], (int) ct["di"], (int) ct["dj"], (int) ct["col"] });
    colourCounter = (int) cables.size();
    commit();
}

void WcProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("WCSTATE");
    root.setProperty ("zoom", uiZoom, nullptr);
    root.addChild (apvts.copyState(), -1, nullptr);
    root.addChild (patchToTree(), -1, nullptr);
    if (auto xml = root.createXml()) copyXmlToBinary (*xml, destData);
}

void WcProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("WCSTATE")) return;
    uiZoom = (float) root.getProperty ("zoom", 1.0f);
    auto params = root.getChildWithName (apvts.state.getType());
    if (params.isValid()) apvts.replaceState (params);
    patchFromTree (root.getChildWithName ("WCPATCH"));
}

juce::File WcProcessor::patchFolder() const
{
    auto d = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                 .getChildFile ("Calabi Yau").getChildFile ("West Coast Modular").getChildFile ("Patches");
    d.createDirectory();
    return d;
}

bool WcProcessor::savePatchFile (const juce::File& f)
{
    juce::ValueTree root ("WCSTATE");
    patchName = f.getFileNameWithoutExtension();
    root.addChild (apvts.copyState(), -1, nullptr);
    root.addChild (patchToTree(), -1, nullptr);
    auto xml = root.createXml();
    return xml != nullptr && xml->writeTo (f);
}

bool WcProcessor::loadPatchFile (const juce::File& f)
{
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr) return false;
    auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("WCSTATE")) return false;
    auto params = root.getChildWithName (apvts.state.getType());
    if (params.isValid()) apvts.replaceState (params);
    patchFromTree (root.getChildWithName ("WCPATCH"));
    patchName = f.getFileNameWithoutExtension();
    sendChangeMessage();
    return true;
}

#if ! WC_HEADLESS
juce::AudioProcessorEditor* WcProcessor::createEditor() { return new WcEditor (*this); }
#else
juce::AudioProcessorEditor* WcProcessor::createEditor() { return nullptr; }
#endif

#if ! WC_NO_PLUGIN_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WcProcessor(); }
#endif
