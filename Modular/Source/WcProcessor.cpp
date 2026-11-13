#include "WcProcessor.h"
#include "modules/M259.h"
#include "modules/MidiInterface.h"
#include "modules/OutputModule.h"
#include "modules/M281.h"
#include "modules/M292.h"
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
    // MIDI Learn: each parameter, its module (by the longest id prefix) and whether it moves in steps
    for (auto& m : midiMap) m.store (-1);
    for (auto* prm : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm);
        int inst = -1;
        size_t best = 0;
        if (rp != nullptr)
            for (int i = 0; i < kNumInst; ++i)
            {
                const auto pre = instPrefix (i);
                if (pre.isNotEmpty() && (size_t) pre.length() > best && rp->getParameterID().startsWith (pre)) { inst = i; best = (size_t) pre.length(); }
            }
        if (inst == kInstMidi || inst == kInstOutput) inst = -1;
        mapParam.push_back (rp);
        mapInst.push_back (inst);
        mapStepped.push_back (rp != nullptr && rp->getNormalisableRange().interval > 0.0f);
    }
    license = wc::License::findAndValidate();
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
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm()) rack.m250()->hostBpm.store (*bpm, std::memory_order_relaxed);
    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    rack.beginBlock (n);
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    auto it = midi.cbegin();
    for (int i = 0; i < n; ++i)
    {
        // MIDI is applied at its exact sample position
        while (it != midi.cend() && (*it).samplePosition <= i)
        {
            const auto msg = (*it).getMessage();
            if (! applyMidiMap (msg)) rack.handleMidi (msg);   // a learned CC stops here
            ++it;
        }
        rack.tick();
        L[i] = rack.outL();
        if (R != nullptr) R[i] = rack.outR();
    }
    for (; it != midi.cend(); ++it)
    {
        const auto msg = (*it).getMessage();
        if (! applyMidiMap (msg)) rack.handleMidi (msg);
    }
}

// ------------------------------------------------------------------------------------------------
// MIDI Learn
// ------------------------------------------------------------------------------------------------
bool WcProcessor::applyMidiMap (const juce::MidiMessage& m) noexcept
{
    if (! m.isController()) return false;
    const int cc = m.getControllerNumber();
    if (! isLearnableCc (cc)) return false;
    lastLearnableCc.store (((lastLearnableCc.load (std::memory_order_relaxed) + 0x100u) & ~0xffu) | (juce::uint32) cc, std::memory_order_release);
    const int idx = midiMap[(size_t) cc].load (std::memory_order_acquire);
    if (idx < 0 || idx >= (int) mapParam.size() || mapParam[(size_t) idx] == nullptr) return false;
    const int inst = mapInst[(size_t) idx];
    if (inst >= 0 && ! rack.isLive (inst)) return false;     // asleep: its module is out of the rack
    auto* p = mapParam[(size_t) idx];
    float v = (float) m.getControllerValue() / 127.0f;        // linear over the knob, a jump (no pickup)
    if (mapStepped[(size_t) idx]) v = p->convertTo0to1 (p->convertFrom0to1 (v));
    p->setValueNotifyingHost (v);
    ccMoves[(size_t) cc].fetch_add (1, std::memory_order_relaxed);
    return true;
}

bool WcProcessor::assignCc (int cc, const juce::String& id, juce::String* movedFrom)
{
    if (! setCc (cc, id, movedFrom)) return false;
    midiMapChanged();
    return true;
}

bool WcProcessor::setCc (int cc, const juce::String& id, juce::String* movedFrom)
{
    if (movedFrom != nullptr) movedFrom->clear();
    auto* p = apvts.getParameter (id);
    if (! isLearnableCc (cc) || p == nullptr) return false;
    const int idx = p->getParameterIndex();
    if (idx < 0 || idx >= (int) mapParam.size()) return false;
    const int had = midiMap[(size_t) cc].load();
    if (movedFrom != nullptr && had >= 0 && had != idx && mapParam[(size_t) had] != nullptr) *movedFrom = mapParam[(size_t) had]->getParameterID();
    for (auto& slot : midiMap) if (slot.load() == idx) slot.store (-1);
    midiMap[(size_t) cc].store (idx, std::memory_order_release);
    return true;
}

void WcProcessor::forgetCc (const juce::String& id)
{
    const int cc = ccOf (id);
    if (cc < 0) return;
    midiMap[(size_t) cc].store (-1, std::memory_order_release);
    midiMapChanged();
}

int WcProcessor::ccOf (const juce::String& id) const
{
    auto* p = apvts.getParameter (id);
    if (p == nullptr) return -1;
    const int idx = p->getParameterIndex();
    for (int cc = 0; cc < 128; ++cc) if (midiMap[(size_t) cc].load() == idx) return cc;
    return -1;
}

juce::String WcProcessor::paramOfCc (int cc) const
{
    if (cc < 0 || cc >= 128) return {};
    const int idx = midiMap[(size_t) cc].load();
    return idx >= 0 && idx < (int) mapParam.size() && mapParam[(size_t) idx] != nullptr ? mapParam[(size_t) idx]->getParameterID() : juce::String();
}

bool WcProcessor::ccIsLive (int cc) const
{
    if (cc < 0 || cc >= 128) return false;
    const int idx = midiMap[(size_t) cc].load();
    if (idx < 0 || idx >= (int) mapInst.size()) return false;
    return mapInst[(size_t) idx] < 0 || present[(size_t) mapInst[(size_t) idx]];
}

bool WcProcessor::midiMapEmpty() const
{
    for (auto& slot : midiMap) if (slot.load() >= 0) return false;
    return true;
}

juce::ValueTree WcProcessor::midiMapToTree() const
{
    juce::ValueTree t ("MIDIMAP");
    for (int cc = 0; cc < 128; ++cc)
    {
        const auto id = paramOfCc (cc);
        if (id.isEmpty()) continue;
        juce::ValueTree m ("MAP");
        m.setProperty ("cc", cc, nullptr);
        m.setProperty ("param", id, nullptr);
        t.addChild (m, -1, nullptr);
    }
    return t;
}

void WcProcessor::midiMapFromTree (const juce::ValueTree& t)
{
    for (auto& slot : midiMap) slot.store (-1, std::memory_order_release);
    if (t.hasType ("MIDIMAP"))
        for (auto m : t)
            if (m.hasType ("MAP")) setCc ((int) m.getProperty ("cc", -1), m.getProperty ("param").toString());   // unknown ids and CCs are left out
}

// A patch (file, factory, built-in) keeps the CCs the person has set up; only into an empty map does a patch
// bring its own.
void WcProcessor::adoptMidiMap (const juce::ValueTree& root)
{
    const auto t = root.getChildWithName ("MIDIMAP");
    if (t.isValid() && t.getNumChildren() > 0 && midiMapEmpty()) midiMapFromTree (t);
}

void WcProcessor::midiMapChanged()
{
    // the map is saved with the project: tell the host there is something new to save
    updateHostDisplay (juce::AudioProcessorListener::ChangeDetails().withNonParameterStateChanged (true));
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
    if (! isRemovable (inst)) return;
    if (on && ! canAddOscillator (inst)) return;      // demo mode: two oscillators
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
    return { "Init Voice", "FM Bell", "Folded Bass", "Order Sweep", "Timbre Wobble", "Ring Mod Drone", "Cross-FM Pair", "Phase Lock Unison",
             "Bongo (LPG ping)", "Function Pluck", "Quadrature Drift", "Uncertainty (generative)",
             "245 Sequence (LPG)", "242 Programmed Rhythm",
             "258 Morphing Duo", "264 Poly Round Robin",
             "285 Shifted Bells", "277 Delay Taps (207 mix)", "296 Spectral Sweep", "248 MARF Melody", "218 Touch Arpeggio", "250 Ring Sequence", "252 Polyrhythm 16:5:3" };
}

void WcProcessor::loadBuiltinPatch (int index)
{
    resetParams();
    rack.m252()->clearAll();
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
            setParam (b + "po_fine", 0.25f); setParam (b + "lock", 0.8f); setParam (b + "lock_on", 1.0f);
            cables.push_back ({ A, M259::Out_Sine, B, M259::In_Lock, nextColour (false) });
            setParam (a + "timbre", 0.4f); setParam (b + "timbre", 0.4f); setParam (b + "order", 0.6f);
            break;
        case 8:   // Bongo: the keyboard's 5 ms pulse pings the vactrol of the lowpass gate (BOTH mode)
        {
            voice (A);
            present[(size_t) kInst292] = true;
            cables.push_back ({ A, M259::Out_Final1, kInst292, M292::inSig (0), nextColour (false) });
            cables.push_back ({ kInstMidi, MI::Out_Pulse, kInst292, M292::inCtl (0), nextColour (true) });
            cables.push_back ({ kInst292, M292::outSig (0), kInstOutput, OutputModule::In_Left, nextColour (false) });
            setParam (a + "timbre", 0.38f); setParam (a + "mo_freq", hz (65.406f * 2.0f)); setParam (a + "idx", 0.25f); setParam (a + "mod_pitch", 1.0f);
            setParam ("midi_trans", 1.0f);
            break;
        }
        case 9:   // Function Pluck: a 281 transient function opens the gate and the 259's Timbre at once
        {
            voice (A);
            present[(size_t) kInst281] = present[(size_t) kInst292] = true;
            const juce::String f (prefix281 (0)), l (prefix292 (0));
            cables.push_back ({ kInstMidi, MI::Out_Gate, kInst281, M281::inTrig (0), nextColour (true) });
            cables.push_back ({ kInst281, M281::outFunc (0), kInst292, M292::inCtl (0), nextColour (true) });
            cables.push_back ({ kInst281, M281::outFunc (0), A, M259::In_TimbreCv, nextColour (true) });     // stacked banana
            cables.push_back ({ A, M259::Out_Final1, kInst292, M292::inSig (0), nextColour (false) });
            cables.push_back ({ kInst292, M292::outSig (0), kInstOutput, OutputModule::In_Left, nextColour (false) });
            setParam (f + "att1", 0.08f); setParam (f + "dec1", 0.64f);
            setParam (a + "timbre", 0.08f); setParam (a + "timbre_cv", 0.7f); setParam (a + "sym", 0.58f);
            setParam (l + "mode1", 0.0f); setParam (l + "res1", 0.55f);
            break;
        }
        case 10:  // Quadrature Drift: 281 A cycles and fires B at its peak; A sweeps Timbre, B sweeps Order
        {
            voice (A);
            present[(size_t) kInst281] = present[(size_t) kInst292] = true;
            const juce::String f (prefix281 (0)), l (prefix292 (0));
            setParam (f + "mode1", 2.0f); setParam (f + "att1", 0.78f); setParam (f + "dec1", 0.82f);
            setParam (f + "att2", 0.7f); setParam (f + "dec2", 0.85f); setParam (f + "quad12", 1.0f);
            cables.push_back ({ kInst281, M281::outFunc (0), A, M259::In_TimbreCv, nextColour (true) });
            cables.push_back ({ kInst281, M281::outFunc (1), A, M259::In_OrderCv, nextColour (true) });
            cables.push_back ({ kInstMidi, MI::Out_Gate, kInst292, M292::inCtl (0), nextColour (true) });
            cables.push_back ({ A, M259::Out_Final1, kInst292, M292::inSig (0), nextColour (false) });
            cables.push_back ({ kInst292, M292::outSig (0), kInstOutput, OutputModule::In_Left, nextColour (false) });
            setParam (a + "timbre", 0.15f); setParam (a + "timbre_cv", 0.6f); setParam (a + "order", 0.0f);
            setParam (a + "mo_freq", hz (65.406f * 3.0f)); setParam (a + "idx", 0.18f); setParam (a + "mod_pitch", 1.0f);
            setParam (l + "mode1", 1.0f);
            break;
        }
        case 11:  // Uncertainty: a cycling 281 clocks the 266; quantized random -> integrator -> pitch, fluctuating -> timbre,
                  // every end-of-decay pulse pings the 292. Plays by itself.
        {
            present[(size_t) A] = present[(size_t) kInst266] = present[(size_t) kInst281] = present[(size_t) kInst292] = true;
            const juce::String f (prefix281 (0)), l (prefix292 (0));
            setParam (f + "mode1", 2.0f); setParam (f + "att1", 0.05f); setParam (f + "dec1", 0.6f);
            cables.push_back ({ kInst281, M281::outEod (0), kInst266, M266::In_QPulse, nextColour (true) });
            cables.push_back ({ kInst266, M266::Out_Qn1, kInst266, M266::In_IntIn, nextColour (true) });
            cables.push_back ({ kInst266, M266::Out_Int, A, M259::In_PoPitch, nextColour (true) });
            cables.push_back ({ kInst266, M266::Out_Int, A, M259::In_MoPitch, nextColour (true) });
            cables.push_back ({ kInst266, M266::Out_Fluct1, A, M259::In_TimbreCv, nextColour (true) });
            cables.push_back ({ kInst281, M281::outEod (0), kInst292, M292::inCtl (0), nextColour (true) });
            cables.push_back ({ A, M259::Out_Final1, kInst292, M292::inSig (0), nextColour (false) });
            cables.push_back ({ kInst292, M292::outSig (0), kInstOutput, OutputModule::In_Left, nextColour (false) });
            setParam ("ua_steps", 4.0f); setParam ("ua_smooth", 0.12f); setParam ("ua_rate", 0.35f);
            setParam (a + "po_freq", hz (130.81f)); setParam (a + "mo_freq", hz (130.81f * 2.0f)); setParam (a + "idx", 0.2f); setParam (a + "mod_pitch", 1.0f);
            setParam (a + "timbre", 0.1f); setParam (a + "timbre_cv", 0.45f);
            setParam (l + "mode1", 1.0f); setParam ("out_level", 0.9f);
            break;
        }
        case 12:  // 245 Sequence: the pulser steps five stages; row A plays the pitch (0.1 V = one semitone at 1.2 V/oct),
                  // row B the timbre, each pulse pings the 292
        {
            present[(size_t) A] = present[(size_t) kInst245] = present[(size_t) kInst292] = true;
            const juce::String l (prefix292 (0));
            const float notes[5] = { 0, 7, 12, 3, 10 }, timb[5] = { 0.2f, 0.5f, 0.8f, 0.35f, 0.65f };
            for (int k = 0; k < 5; ++k)
            {
                setParam ("sv_a" + juce::String (k + 1), notes[k] * 0.01f);
                setParam ("sv_b" + juce::String (k + 1), timb[k]);
            }
            setParam ("sv_time", 0.54f); setParam ("sv_len", 0.15f);
            cables.push_back ({ kInst245, M245::Out_A, A, M259::In_PoPitch, nextColour (true) });
            cables.push_back ({ kInst245, M245::Out_A, A, M259::In_MoPitch, nextColour (true) });
            cables.push_back ({ kInst245, M245::Out_B, A, M259::In_TimbreCv, nextColour (true) });
            cables.push_back ({ kInst245, M245::Out_Pulse, kInst292, M292::inCtl (0), nextColour (true) });
            cables.push_back ({ A, M259::Out_Final1, kInst292, M292::inSig (0), nextColour (false) });
            cables.push_back ({ kInst292, M292::outSig (0), kInstOutput, OutputModule::In_Left, nextColour (false) });
            setParam (a + "po_freq", hz (110.0f)); setParam (a + "mo_freq", hz (220.0f)); setParam (a + "idx", 0.15f); setParam (a + "mod_pitch", 1.0f);
            setParam (a + "timbre", 0.05f); setParam (a + "timbre_cv", 0.5f);
            setParam (l + "mode1", 1.0f); setParam ("out_level", 0.9f);
            break;
        }
        case 13:  // 242 Programmed Rhythm: the 242 clocks the 245 (ALL OUT -> advance); matrix A pings a low voice,
                  // matrix B a high one through the second 292 channel, matrix C retriggers a 281 sweeping the timbre
        {
            present[(size_t) A] = present[(size_t) B] = present[(size_t) kInst242] = present[(size_t) kInst245] = true;
            present[(size_t) kInst281] = present[(size_t) kInst292] = true;
            const juce::String l (prefix292 (0)), f (prefix281 (0));
            const int pa[16] = { 1, 4, 7, 10, 0 }, pb[16] = { 3, 9, 12, 0 }, pc[16] = { 1, 5, 9, 0 };
            for (int k = 0; pa[k] != 0; ++k) setParam ("pp_a" + juce::String (pa[k]), 1.0f);
            for (int k = 0; pb[k] != 0; ++k) setParam ("pp_b" + juce::String (pb[k]), 1.0f);
            for (int k = 0; pc[k] != 0; ++k) setParam ("pp_c" + juce::String (pc[k]), 1.0f);
            setParam ("pp_len", 12.0f); setParam ("pp_period", 0.447f);
            const float notes[5] = { 0, 3, 5, 7, 10 };
            for (int k = 0; k < 5; ++k) setParam ("sv_a" + juce::String (k + 1), notes[k] * 0.01f);
            setParam ("sv_sw4", 0.0f);   // four-stage loop
            cables.push_back ({ kInst242, M242::Out_All, kInst245, M245::In_Advance, nextColour (true) });
            cables.push_back ({ kInst245, M245::Out_A, A, M259::In_PoPitch, nextColour (true) });
            cables.push_back ({ kInst245, M245::Out_A, B, M259::In_PoPitch, nextColour (true) });
            cables.push_back ({ kInst242, M242::Out_A, kInst292, M292::inCtl (0), nextColour (true) });
            cables.push_back ({ kInst242, M242::Out_B, kInst292, M292::inCtl (1), nextColour (true) });
            cables.push_back ({ kInst242, M242::Out_C, kInst281, M281::inTrig (0), nextColour (true) });
            cables.push_back ({ kInst281, M281::outFunc (0), A, M259::In_TimbreCv, nextColour (true) });
            cables.push_back ({ A, M259::Out_Final1, kInst292, M292::inSig (0), nextColour (false) });
            cables.push_back ({ B, M259::Out_Final1, kInst292, M292::inSig (1), nextColour (false) });
            cables.push_back ({ kInst292, M292::outSig (0), kInstOutput, OutputModule::In_Left, nextColour (false) });
            cables.push_back ({ kInst292, M292::outSig (1), kInstOutput, OutputModule::In_Right, nextColour (false) });
            setParam (a + "po_freq", hz (55.0f)); setParam (a + "timbre", 0.2f); setParam (a + "timbre_cv", 0.6f);
            setParam (b + "po_freq", hz (440.0f)); setParam (b + "mo_freq", hz (440.0f * 1.5f)); setParam (b + "idx", 0.3f); setParam (b + "mod_pitch", 1.0f);
            setParam (f + "att1", 0.05f); setParam (f + "dec1", 0.5f);
            setParam (l + "mode1", 1.0f); setParam (l + "mode2", 1.0f); setParam ("out_level", 0.9f);
            break;
        }
        case 14:  // 258 Morphing Duo: the keyboard plays both 258 oscillators (a fifth apart); a cycling 281 pair, crossfaded
                  // by the 257, morphs the waveshapes; both through the 292, left and right
        {
            const int O = kInst258;
            present[(size_t) O] = present[(size_t) kInst257] = present[(size_t) kInst281] = present[(size_t) kInst292] = true;
            const juce::String o (prefix258 (0)), f (prefix281 (0)), l (prefix292 (0));
            cables.push_back ({ kInstMidi, MI::Out_Pitch, O, M258::inOf (0, 0), nextColour (true) });
            cables.push_back ({ kInstMidi, MI::Out_Pitch2, O, M258::inOf (1, 0), nextColour (true) });
            setParam (o + "freq1", std::log (65.406f / 5.0f) / std::log (4000.0f));
            setParam (o + "freq2", std::log (98.0f / 5.0f) / std::log (4000.0f));
            setParam (f + "mode1", 2.0f); setParam (f + "att1", 0.62f); setParam (f + "dec1", 0.7f);
            setParam (f + "mode2", 2.0f); setParam (f + "att2", 0.75f); setParam (f + "dec2", 0.62f);
            cables.push_back ({ kInst281, M281::outFunc (0), kInst257, M257::inOf (0, 1), nextColour (true) });
            cables.push_back ({ kInst281, M281::outFunc (1), kInst257, M257::inOf (0, 3), nextColour (true) });
            cables.push_back ({ kInst281, M281::outFunc (1), kInst257, M257::inOf (0, 2), nextColour (true) });
            cables.push_back ({ kInst257, M257::Out_1, O, M258::inOf (0, 3), nextColour (true) });
            cables.push_back ({ kInst281, M281::outFunc (0), O, M258::inOf (1, 3), nextColour (true) });
            setParam ("vp_k1", 0.0f); setParam ("vp_m1", 0.3f);
            setParam (o + "shape1", 0.1f); setParam (o + "shape2", 0.05f);
            cables.push_back ({ O, M258::Out_1, kInst292, M292::inSig (0), nextColour (false) });
            cables.push_back ({ O, M258::Out_2, kInst292, M292::inSig (1), nextColour (false) });
            cables.push_back ({ kInstMidi, MI::Out_Gate, kInst292, M292::inCtl (0), nextColour (true) });
            cables.push_back ({ kInstMidi, MI::Out_Gate, kInst292, M292::inCtl (1), nextColour (true) });
            cables.push_back ({ kInst292, M292::outSig (0), kInstOutput, OutputModule::In_Left, nextColour (false) });
            cables.push_back ({ kInst292, M292::outSig (1), kInstOutput, OutputModule::In_Right, nextColour (false) });
            setParam (l + "mode1", 1.0f); setParam (l + "mode2", 1.0f); setParam ("midi_trans", 1.0f); setParam ("out_level", 0.9f);
            break;
        }
        case 15:  // 264 Poly Round Robin: the 242 clocks the 266 quantized voltages and the 264 polyphonic adaptor, which deals
                  // the notes to three voices (258 1, 258 2, 259 A) and pings each one in its own 292 channel; the 230 follows
                  // the first voice and opens the 259 timbre. Plays by itself.
        {
            const int O = kInst258;
            for (int i : { kInst242, kInst266, kInst264, kInst230, O, (int) A, kInst292 }) present[(size_t) i] = true;
            const juce::String o (prefix258 (0)), l (prefix292 (0));
            setParam ("pp_len", 8.0f); setParam ("pp_period", 0.47f);
            cables.push_back ({ kInst242, M242::Out_All, kInst266, M266::In_QPulse, nextColour (true) });
            cables.push_back ({ kInst266, M266::Out_Qn1, kInst264, M264::In_PolyCv, nextColour (true) });
            cables.push_back ({ kInst242, M242::Out_All, kInst264, M264::In_Poly, nextColour (true) });
            setParam ("sh_voices", 3.0f); setParam ("ua_steps", 5.0f);
            cables.push_back ({ kInst264, M264::Out_Cv1, O, M258::inOf (0, 0), nextColour (true) });
            cables.push_back ({ kInst264, M264::Out_Cv2, O, M258::inOf (1, 0), nextColour (true) });
            cables.push_back ({ kInst264, M264::Out_Cv3, A, M259::In_PoPitch, nextColour (true) });
            cables.push_back ({ kInst264, M264::Out_Cv3, A, M259::In_MoPitch, nextColour (true) });
            cables.push_back ({ kInst264, M264::Out_Pulse1, kInst292, M292::inCtl (0), nextColour (true) });
            cables.push_back ({ kInst264, M264::Out_Pulse2, kInst292, M292::inCtl (1), nextColour (true) });
            cables.push_back ({ kInst264, M264::Out_Pulse3, kInst292, M292::inCtl (2), nextColour (true) });
            cables.push_back ({ O, M258::Out_1, kInst292, M292::inSig (0), nextColour (false) });
            cables.push_back ({ O, M258::Out_2, kInst292, M292::inSig (1), nextColour (false) });
            cables.push_back ({ A, M259::Out_Final1, kInst292, M292::inSig (2), nextColour (false) });
            cables.push_back ({ kInst292, M292::outSig (0), kInst230, M230::In_Audio1, nextColour (false) });
            cables.push_back ({ kInst230, M230::Out_Env1, A, M259::In_TimbreCv, nextColour (true) });
            cables.push_back ({ kInst292, M292::outAll, kInstOutput, OutputModule::In_Left, nextColour (false) });
            setParam (o + "freq1", std::log (130.81f / 5.0f) / std::log (4000.0f)); setParam (o + "shape1", 0.45f);
            setParam (o + "freq2", std::log (261.63f / 5.0f) / std::log (4000.0f)); setParam (o + "shape2", 0.3f);
            setParam (a + "po_freq", hz (196.0f)); setParam (a + "mo_freq", hz (392.0f)); setParam (a + "idx", 0.2f); setParam (a + "mod_pitch", 1.0f);
            setParam (a + "timbre", 0.05f); setParam (a + "timbre_cv", 0.5f);
            setParam ("ef_sens1", 0.7f); setParam ("ef_decay1", 0.45f);
            for (int k = 1; k <= 3; ++k) setParam (l + "mode" + juce::String (k), 1.0f);
            setParam ("out_level", 0.6f);
            break;
        }
        case 16:  // 285 Shifted Bells: a 259 bell through the frequency shifter; a slow 281 cycle sweeps the shift through zero,
                  // sum and difference to left and right
        {
            voice (A); gateToOut();
            present[(size_t) kInst285] = present[(size_t) kInst281] = true;
            const juce::String f (prefix281 (0));
            setParam (a + "mo_freq", hz (65.406f * 3.5f)); setParam (a + "idx", 0.35f); setParam (a + "mod_pitch", 1.0f); setParam (a + "timbre", 0.1f);
            cables.push_back ({ A, M259::Out_Final1, kInst285, M285::In_FsSignal, nextColour (false) });
            cables.push_back ({ kInst285, M285::Out_Sum, kInstOutput, OutputModule::In_Left, nextColour (false) });
            cables.push_back ({ kInst285, M285::Out_Diff, kInstOutput, OutputModule::In_Right, nextColour (false) });
            setParam (f + "mode1", 2.0f); setParam (f + "att1", 0.8f); setParam (f + "dec1", 0.8f);
            cables.push_back ({ kInst281, M281::outFunc (0), kInst285, M285::In_FsControl, nextColour (true) });
            setParam ("fs_ref", 0.08f); setParam ("fs_ctl", -0.25f);
            setParam ("out_lag", 220.0f); setParam ("midi_trans", 1.0f); setParam ("out_level", 0.55f);
            break;
        }
        case 17:  // 277 Delay Taps: the 245 sequence pings a 292; the 277 taps (1/4, 1/2, 3/4, t) come back through the 207,
                  // panned across the stereo field, with the dry voice in the middle
        {
            present[(size_t) A] = present[(size_t) kInst245] = present[(size_t) kInst292] = present[(size_t) kInst277] = present[(size_t) kInst207] = true;
            const juce::String l (prefix292 (0));
            const float notes[5] = { 0, 7, 10, 12, 3 };
            for (int k = 0; k < 5; ++k) setParam ("sv_a" + juce::String (k + 1), notes[k] * 0.01f);
            setParam ("sv_time", 0.6f); setParam ("sv_len", 0.1f);
            cables.push_back ({ kInst245, M245::Out_A, A, M259::In_PoPitch, nextColour (true) });
            cables.push_back ({ kInst245, M245::Out_Pulse, kInst292, M292::inCtl (0), nextColour (true) });
            cables.push_back ({ A, M259::Out_Final1, kInst292, M292::inSig (0), nextColour (false) });
            cables.push_back ({ kInst292, M292::outSig (0), kInst277, M277::In_A, nextColour (false) });
            cables.push_back ({ kInst292, M292::outSig (0), kInst207, M207::In_A + 2, nextColour (false) });
            cables.push_back ({ kInst277, M277::Out_Q1, kInst207, M207::In_A + 0, nextColour (false) });
            cables.push_back ({ kInst277, M277::Out_Q2, kInst207, M207::In_A + 1, nextColour (false) });
            cables.push_back ({ kInst277, M277::Out_Q3, kInst207, M207::In_A + 4, nextColour (false) });
            cables.push_back ({ kInst277, M277::Out_T, kInst207, M207::In_A + 5, nextColour (false) });
            cables.push_back ({ kInst207, M207::Out_L, kInstOutput, OutputModule::In_Left, nextColour (false) });
            cables.push_back ({ kInst207, M207::Out_R, kInstOutput, OutputModule::In_Right, nextColour (false) });
            setParam ("dl_time", 0.6f); setParam ("dl_long", 1.0f); setParam ("dl_lvla", 0.9f);
            for (auto c : { "a", "b", "e", "f" }) setParam (juce::String ("mx_lvl_") + c, 0.62f);
            setParam ("mx_lvl_c", 0.9f); setParam ("mx_lvl_d", 0.0f);
            setParam ("mx_pan1", 0.25f); setParam ("mx_pan2", 0.5f); setParam ("mx_pan4", 0.75f);
            setParam (a + "po_freq", hz (196.0f)); setParam (a + "timbre", 0.25f); setParam (a + "mo_freq", hz (392.0f)); setParam (a + "idx", 0.15f); setParam (a + "mod_pitch", 1.0f);
            setParam (l + "mode1", 1.0f); setParam ("out_level", 0.9f);
            break;
        }
        case 18:  // 296 Spectral Sweep: a bright 259 drone through the 296; a cycling 281 walks a narrow program window up and
                  // down the sixteen channels; the even and odd programmed outputs go left and right. Plays by itself.
        {
            present[(size_t) A] = present[(size_t) kInst296] = present[(size_t) kInst281] = true;
            const juce::String f (prefix281 (0));
            setParam (a + "po_freq", hz (55.0f)); setParam (a + "timbre", 0.55f); setParam (a + "sym", 0.6f);
            setParam (a + "mo_freq", hz (55.0f * 1.5f)); setParam (a + "idx", 0.25f); setParam (a + "mod_pitch", 1.0f);
            cables.push_back ({ A, M259::Out_Final1, kInst296, M296::In_SigAll, nextColour (false) });
            setParam (f + "mode1", 2.0f); setParam (f + "att1", 0.82f); setParam (f + "dec1", 0.82f);
            cables.push_back ({ kInst281, M281::outFunc (0), kInst296, M296::In_FreqCv, nextColour (true) });
            setParam ("sp_freq", 0.05f); setParam ("sp_fcv", 0.7f); setParam ("sp_width", 0.15f);
            cables.push_back ({ kInst296, M296::Out_ProgEven, kInstOutput, OutputModule::In_Left, nextColour (false) });
            cables.push_back ({ kInst296, M296::Out_ProgOdd, kInstOutput, OutputModule::In_Right, nextColour (false) });
            setParam ("out_level", 1.0f); setParam ("out_master", 3.0f);
            break;
        }
        case 19:  // 248 MARF Melody: a key starts function generator 1; eight quantized stages (D minor) loop, every stage fires
                  // pulse 1 into the 292, stages 4 and 8 glide (sloped); FG 2 runs the same stages slower (x4) on the timbre
        {
            present[(size_t) A] = present[(size_t) kInst248] = present[(size_t) kInst292] = true;
            const juce::String l (prefix292 (0));
            const float lv[8] = { 0.0f, 0.03f, 0.05f, 0.07f, 0.12f, 0.10f, 0.08f, 0.05f };   // 10 V full scale: 0.01 = 1 semitone
            const float tm[8] = { 0.5f, 0.5f, 0.3f, 0.7f, 0.5f, 0.5f, 0.3f, 0.8f };
            for (int k = 0; k < 8; ++k)
            {
                const juce::String n (k + 1);
                setParam ("mf_lvl" + n, lv[k]); setParam ("mf_tim" + n, tm[k]);
                setParam ("mf_p1_" + n, 1.0f); setParam ("mf_quant_" + n, 1.0f); setParam ("mf_trange_" + n, 1.0f);
            }
            setParam ("mf_slope_4", 1.0f); setParam ("mf_slope_8", 1.0f); setParam ("mf_last_8", 1.0f);
            setParam ("mf_key", 2.0f); setParam ("mf_scale", 0.0f);
            setParam ("mf_mult2", 1.0f);
            cables.push_back ({ kInstMidi, MI::Out_Gate, kInst248, M248::inJ (0, 2), nextColour (true) });
            cables.push_back ({ kInstMidi, MI::Out_Gate, kInst248, M248::inJ (1, 2), nextColour (true) });
            cables.push_back ({ kInst248, M248::outJ (0, 3), A, M259::In_PoPitch, nextColour (true) });
            cables.push_back ({ kInst248, M248::outJ (0, 5), kInst292, M292::inCtl (0), nextColour (true) });
            cables.push_back ({ kInst248, M248::outJ (1, 3), A, M259::In_TimbreCv, nextColour (true) });
            cables.push_back ({ A, M259::Out_Final1, kInst292, M292::inSig (0), nextColour (false) });
            cables.push_back ({ kInst292, M292::outSig (0), kInstOutput, OutputModule::In_Left, nextColour (false) });
            setParam (a + "po_freq", hz (146.83f)); setParam (a + "mo_freq", hz (293.66f)); setParam (a + "idx", 0.2f); setParam (a + "mod_pitch", 1.0f);
            setParam (a + "timbre", 0.05f); setParam (a + "timbre_cv", 0.3f);
            setParam (l + "mode1", 1.0f); setParam ("out_level", 0.9f);
            break;
        }
        case 20:  // 218 Touch Arpeggio: hold keys on the 218 (or MIDI): the ascending arpeggiator plays a 259 through a 292;
                  // pressure opens the timbre, the strip sweeps the modulation index
        {
            present[(size_t) A] = present[(size_t) kInst218] = present[(size_t) kInst292] = true;
            const juce::String l (prefix292 (0));
            setParam ("kb_arp", 0.0f); setParam ("kb_rate", 0.55f); setParam ("kb_porta", 0.1f); setParam ("kb_add", 0.0f);
            cables.push_back ({ kInst218, M218::Out_Pitch, A, M259::In_PoPitch, nextColour (true) });
            cables.push_back ({ kInst218, M218::Out_Pitch, A, M259::In_MoPitch, nextColour (true) });
            cables.push_back ({ kInst218, M218::Out_Pulse, kInst292, M292::inCtl (0), nextColour (true) });
            cables.push_back ({ kInst218, M218::Out_Pressure, A, M259::In_TimbreCv, nextColour (true) });
            cables.push_back ({ kInst218, M218::Out_Strip, A, M259::In_IndexCv, nextColour (true) });
            cables.push_back ({ A, M259::Out_Final1, kInst292, M292::inSig (0), nextColour (false) });
            cables.push_back ({ kInst292, M292::outSig (0), kInstOutput, OutputModule::In_Left, nextColour (false) });
            setParam (a + "po_freq", hz (65.406f)); setParam (a + "mo_freq", hz (130.81f)); setParam (a + "mod_pitch", 1.0f); setParam (a + "idx", 0.1f);
            setParam (a + "timbre", 0.05f); setParam (a + "timbre_cv", 0.4f);
            setParam (l + "mode1", 1.0f); setParam ("out_level", 0.9f);
            break;
        }
        case 21:  // 250 Ring Sequence: a key starts the 250; sixteen quantized stages (set 1 = pitch, set 2 = timbre), every stage
                  // pings the 292, stages 4, 8, 12 and 16 glide (interp); stages 1-8 loop twice before the second half plays
        {
            present[(size_t) A] = present[(size_t) kInst250] = present[(size_t) kInst292] = true;
            const juce::String l (prefix292 (0));
            const int notes[16] = { 0, 7, 12, 10, 3, 7, 5, 15, 0, 12, 10, 7, 17, 15, 12, 19 };
            for (int k = 0; k < 16; ++k)
            {
                const juce::String n (k + 1);
                setParam ("sq_v1_" + n, (float) notes[k] * 0.01f); setParam ("sq_v2_" + n, 0.2f + 0.05f * (float) (k % 8));
                setParam ("sq_t_" + n, (k % 4 == 3) ? 0.7f : 0.45f); setParam ("sq_p1_" + n, 1.0f); setParam ("sq_vq_" + n, 1.0f);
                if (k % 4 == 3) setParam ("sq_vi_" + n, 1.0f);
            }
            setParam ("sq_loopend", 8.0f); setParam ("sq_loopto", 1.0f); setParam ("sq_loops", 2.0f);
            cables.push_back ({ kInstMidi, MI::Out_Pulse, kInst250, M250::In_Start, nextColour (true) });
            cables.push_back ({ kInst250, M250::Out_Cv1, A, M259::In_PoPitch, nextColour (true) });
            cables.push_back ({ kInst250, M250::Out_Cv2, A, M259::In_TimbreCv, nextColour (true) });
            cables.push_back ({ kInst250, M250::Out_P1, kInst292, M292::inCtl (0), nextColour (true) });
            cables.push_back ({ A, M259::Out_Final1, kInst292, M292::inSig (0), nextColour (false) });
            cables.push_back ({ kInst292, M292::outSig (0), kInstOutput, OutputModule::In_Left, nextColour (false) });
            setParam (a + "po_freq", hz (110.0f)); setParam (a + "mo_freq", hz (220.0f)); setParam (a + "idx", 0.15f); setParam (a + "mod_pitch", 1.0f);
            setParam (a + "timbre", 0.0f); setParam (a + "timbre_cv", 0.5f);
            setParam (l + "mode1", 1.0f); setParam ("out_level", 0.9f);
            break;
        }
        case 22:  // 252 Polyrhythm 16:5:3: three rings meet on cell 1 every bar. Ring 16 (master): red Euclidean 7/16 with a
                  // melody on c.v. 1; ring 5 (clock 2): green pings a high voice; ring 3 (clock 3): blue fires a 281 on the timbre.
        {
            present[(size_t) A] = present[(size_t) B] = present[(size_t) kInst252] = present[(size_t) kInst292] = present[(size_t) kInst281] = true;
            const juce::String l (prefix292 (0)), f (prefix281 (0));
            auto* g = rack.m252();
            const int r16 = 10, r5 = 2, r3 = 0;
            const int notes[16] = { 0, 3, 7, 10, 12, 10, 7, 5, 0, 3, 7, 12, 15, 12, 10, 7 };
            for (int i = 0; i < 16; ++i)
            {
                const int c = M252::base (r16) + i;
                g->rgb[c].store (((i * 7) % 16) < 7 ? 1 : 0);
                g->cv[c][0].store (0.1f * (float) notes[i]);
            }
            for (int i = 0; i < 5; ++i) { const int c = M252::base (r5) + i; g->rgb[c].store (2); g->cv[c][1].store (i == 0 ? 2.4f : 1.9f + 0.1f * (float) i); }
            g->rgb[M252::base (r3)].store (4);
            setParam ("rg_clk1", (float) (r16 + 1)); setParam ("rg_clk2", (float) (r5 + 1)); setParam ("rg_clk3", (float) (r3 + 1));
            setParam ("rg_sync", 1.0f); setParam ("rg_bpm", 140.0f); setParam ("rg_ring", (float) r16);
            cables.push_back ({ kInst252, M252::Out_Cv1, A, M259::In_PoPitch, nextColour (true) });
            cables.push_back ({ kInst252, M252::Out_Cv2, B, M259::In_PoPitch, nextColour (true) });
            cables.push_back ({ kInst252, M252::Out_R, kInst292, M292::inCtl (0), nextColour (true) });
            cables.push_back ({ kInst252, M252::Out_G, kInst292, M292::inCtl (1), nextColour (true) });
            cables.push_back ({ kInst252, M252::Out_B, kInst281, M281::inTrig (0), nextColour (true) });
            cables.push_back ({ kInst281, M281::outFunc (0), A, M259::In_TimbreCv, nextColour (true) });
            cables.push_back ({ A, M259::Out_Final1, kInst292, M292::inSig (0), nextColour (false) });
            cables.push_back ({ B, M259::Out_Final1, kInst292, M292::inSig (1), nextColour (false) });
            cables.push_back ({ kInst292, M292::outSig (0), kInstOutput, OutputModule::In_Left, nextColour (false) });
            cables.push_back ({ kInst292, M292::outSig (1), kInstOutput, OutputModule::In_Right, nextColour (false) });
            setParam (a + "po_freq", hz (55.0f)); setParam (a + "timbre", 0.1f); setParam (a + "timbre_cv", 0.6f);
            setParam (b + "po_freq", hz (220.0f)); setParam (b + "mo_freq", hz (330.0f)); setParam (b + "idx", 0.3f); setParam (b + "mod_pitch", 1.0f);
            setParam (f + "att1", 0.1f); setParam (f + "dec1", 0.6f);
            setParam (l + "mode1", 1.0f); setParam (l + "mode2", 1.0f); setParam ("out_level", 0.9f);
            break;
        }
    }
    patchName = builtinPatchNames()[juce::jlimit (0, builtinPatchNames().size() - 1, index)];
    commit();
}

juce::ValueTree WcProcessor::patchToTree() const
{
    // version 3: modules and cable ends are stored by slot name, so new slots never shift old patches
    juce::ValueTree t ("WCPATCH");
    t.setProperty ("name", patchName, nullptr);
    t.setProperty ("version", 3, nullptr);
    juce::ValueTree mods ("MODULES");
    for (int i = 0; i < kNumInst; ++i) mods.setProperty (instKey (i), present[(size_t) i], nullptr);
    t.addChild (mods, -1, nullptr);
    juce::ValueTree cs ("CABLES");
    for (auto& c : cables)
    {
        juce::ValueTree ct ("CABLE");
        ct.setProperty ("s", instKey (c.srcInst), nullptr); ct.setProperty ("sj", c.srcJack, nullptr);
        ct.setProperty ("d", instKey (c.dstInst), nullptr); ct.setProperty ("dj", c.dstJack, nullptr);
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
    const int version = (int) t.getProperty ("version", 1);
    // older patches stored slot numbers:
    //   version 1 (0.1.0): MIDI, 259 A-D, OUTPUT
    //   version 2 (0.2.0): MIDI, 281 A-B, 259 A-D, 292 A-B, OUTPUT
    auto oldSlot = [version] (int old) -> int
    {
        if (version == 1)
        {
            if (old == 0) return kInstMidi;
            if (old >= 1 && old <= 4) return kInst259 + old - 1;
            if (old == 5) return kInstOutput;
            return -1;
        }
        if (old == 0) return kInstMidi;
        if (old >= 1 && old <= 2) return kInst281 + old - 1;
        if (old >= 3 && old <= 6) return kInst259 + old - 3;
        if (old >= 7 && old <= 8) return kInst292 + old - 7;
        if (old == 9) return kInstOutput;
        return -1;
    };
    if (version < 3)   // the phase lock toggle is new: older patches that used the lock get it switched on
        for (int i = 0; i < kMax259; ++i)
        {
            const juce::String p (prefix259 (i));
            if (auto* l = apvts.getRawParameterValue (p + "lock")) if (l->load() > 0.001f) setParam (p + "lock_on", 1.0f);
        }
    present.fill (false);
    auto mods = t.getChildWithName ("MODULES");
    if (version >= 3)
        for (int i = 0; i < kNumInst; ++i) present[(size_t) i] = (bool) mods.getProperty (instKey (i), false);
    else
        for (int i = 0; i < (version == 1 ? 6 : 10); ++i)
        {
            const int s = oldSlot (i);
            if (s >= 0) present[(size_t) s] = (bool) mods.getProperty ("m" + juce::String (i), false);
        }
    present[(size_t) kInstMidi] = present[(size_t) kInstOutput] = true;
    if (demo())   // demo mode keeps the first two oscillators of a patch and leaves the rest out
    {
        int osc = 0;
        for (int i = 0; i < kNumInst; ++i)
            if (present[(size_t) i] && (typeOfInst (i) == MT_259 || typeOfInst (i) == MT_258))
                if (++osc > wc::License::kDemoMaxOscillators) present[(size_t) i] = false;
    }
    cables.clear();
    for (auto ct : t.getChildWithName ("CABLES"))
    {
        int si, di, sj = (int) ct["sj"], dj = (int) ct["dj"];
        if (version >= 3) { si = instFromKey (ct["s"].toString()); di = instFromKey (ct["d"].toString()); }
        else
        {
            si = oldSlot ((int) ct["si"]); di = oldSlot ((int) ct["di"]);
            if (version == 2 && di >= 0 && typeOfInst (di) == MT_281) dj = M281::inFromV2 (dj);   // 281 gained CYCLE inputs
        }
        if (si >= 0 && di >= 0 && present[(size_t) si] && present[(size_t) di])
            cables.push_back ({ si, sj, di, dj, (int) ct["col"] });
    }
    colourCounter = (int) cables.size();
    commit();
}

juce::ValueTree WcProcessor::presetToTree (const juce::String& name, const juce::String& category) const
{
    juce::ValueTree t ("P");
    t.setProperty ("name", name, nullptr);
    t.setProperty ("cat", category, nullptr);
    juce::ValueTree pv ("V");
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (std::fabs (rp->getValue() - rp->getDefaultValue()) > 1.0e-5f)
                pv.setProperty (rp->getParameterID(), juce::String (rp->convertFrom0to1 (rp->getValue()), 5), nullptr);
    t.addChild (pv, -1, nullptr);
    auto pt = patchToTree();
    pt.setProperty ("name", name, nullptr);
    t.addChild (pt, -1, nullptr);
    if (present[(size_t) wc::kInst252]) t.addChild (rack.m252()->toTree(), -1, nullptr);
    return t;
}

void WcProcessor::resetAll()
{
    resetParams();
    rack.m252()->clearAll();
    cables.clear();
    present.fill (false);
    present[(size_t) wc::kInstMidi] = present[(size_t) wc::kInstOutput] = true;
    colourCounter = 0;
    commit();
}

void WcProcessor::loadPresetTree (const juce::ValueTree& t)
{
    if (! t.isValid() || demo()) return;
    resetParams();
    rack.m252()->clearAll();
    auto pv = t.getChildWithName ("V");
    for (int i = 0; i < pv.getNumProperties(); ++i)
    {
        const auto id = pv.getPropertyName (i).toString();
        if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 ((float) pv[pv.getPropertyName (i)]));
    }
    patchFromTree (t.getChildWithName ("WCPATCH"));
    auto rg = t.getChildWithName ("RG252");
    if (rg.isValid()) rack.m252()->fromTree (rg);
    adoptMidiMap (t);
    patchName = t["name"].toString();
    sendChangeMessage();
}

void WcProcessor::setFactoryData (const void* data, int size)
{
    if (factory.getNumChildren() > 0) return;
    if (auto xml = juce::parseXML (juce::String::fromUTF8 ((const char*) data, size)))
        factory = juce::ValueTree::fromXml (*xml);
}

static bool isMarfStageParam (const juce::String& id)
{
    if (! id.startsWith ("mf_")) return false;
    const auto rest = id.substring (3);
    return rest.startsWith ("lvl") || rest.startsWith ("tim") || rest.containsChar ('_');
}

void WcProcessor::marfSave (int slot)
{
    juce::ValueTree pr ("P");
    pr.setProperty ("slot", slot, nullptr);
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (isMarfStageParam (rp->getParameterID())) pr.setProperty (rp->getParameterID(), rp->getValue(), nullptr);
    for (int i = marfPresets.getNumChildren(); --i >= 0;)
        if ((int) marfPresets.getChild (i)["slot"] == slot) marfPresets.removeChild (i, nullptr);
    marfPresets.addChild (pr, -1, nullptr);
}

void WcProcessor::marfLoad (int slot)
{
    for (auto pr : marfPresets)
        if ((int) pr["slot"] == slot)
            for (auto* p : getParameters())
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                    if (pr.hasProperty (rp->getParameterID())) rp->setValueNotifyingHost ((float) pr[rp->getParameterID()]);
}

// ------------------------------------------------------------------------------------------------
// licensing: without a key file the rack holds two oscillators and keeps its presets to itself
// ------------------------------------------------------------------------------------------------
int WcProcessor::oscillatorsInRack() const
{
    int n = 0;
    for (int i = 0; i < kNumInst; ++i)
        if (present[(size_t) i] && (typeOfInst (i) == MT_259 || typeOfInst (i) == MT_258)) ++n;
    return n;
}

bool WcProcessor::canAddOscillator (int inst) const
{
    if (! demo()) return true;
    const auto t = typeOfInst (inst);
    if (t != MT_259 && t != MT_258) return true;
    return oscillatorsInRack() < wc::License::kDemoMaxOscillators;
}

void WcProcessor::recheckLicense (const juce::File& extra)
{
    license = wc::License::findAndValidate (extra);
    sendChangeMessage();
}

void WcProcessor::loadFactoryAt (int index)
{
    if (demo() || factory.getNumChildren() == 0) return;
    const int i = juce::jlimit (0, factory.getNumChildren() - 1, index);
    factoryIndex = i;
    loadPresetTree (factory.getChild (i));
}

void WcProcessor::stepFactory (int delta)
{
    if (demo() || factory.getNumChildren() == 0) return;
    const int n = factory.getNumChildren();
    loadFactoryAt (((factoryIndex < 0 ? (delta > 0 ? -1 : 0) : factoryIndex) + delta % n + n) % n);
}

void WcProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("WCSTATE");
    root.setProperty ("zoom", uiZoom, nullptr);
    root.setProperty ("skin", uiSkin, nullptr);
    root.addChild (apvts.copyState(), -1, nullptr);
    root.addChild (patchToTree(), -1, nullptr);
    root.addChild (marfPresets.createCopy(), -1, nullptr);
    root.addChild (rack.m252()->toTree(), -1, nullptr);
    if (! midiMapEmpty()) root.addChild (midiMapToTree(), -1, nullptr);
    if (auto xml = root.createXml()) copyXmlToBinary (*xml, destData);
}

void WcProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("WCSTATE")) return;
    uiZoom = juce::jlimit (0.5f, 1.5f, (float) root.getProperty ("zoom", 1.0f));
    uiSkin = juce::jlimit (0, 1, (int) root.getProperty ("skin", 0));
    auto params = root.getChildWithName (apvts.state.getType());
    if (params.isValid()) apvts.replaceState (params);
    patchFromTree (root.getChildWithName ("WCPATCH"));
    auto mp = root.getChildWithName ("MARFPRESETS");
    if (mp.isValid()) marfPresets = mp.createCopy();
    auto rg = root.getChildWithName ("RG252");
    if (rg.isValid()) rack.m252()->fromTree (rg);
    midiMapFromTree (root.getChildWithName ("MIDIMAP"));   // the project's map replaces ours; none in it: empty
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
    if (demo()) return false;
    juce::ValueTree root ("WCSTATE");
    patchName = f.getFileNameWithoutExtension();
    root.addChild (apvts.copyState(), -1, nullptr);
    root.addChild (patchToTree(), -1, nullptr);
    root.addChild (rack.m252()->toTree(), -1, nullptr);
    if (! midiMapEmpty()) root.addChild (midiMapToTree(), -1, nullptr);
    auto xml = root.createXml();
    return xml != nullptr && xml->writeTo (f);
}

bool WcProcessor::loadPatchFile (const juce::File& f)
{
    if (demo()) return false;
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr) return false;
    auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("WCSTATE")) return false;
    auto params = root.getChildWithName (apvts.state.getType());
    if (params.isValid()) apvts.replaceState (params);
    patchFromTree (root.getChildWithName ("WCPATCH"));
    { auto rg = root.getChildWithName ("RG252"); if (rg.isValid()) rack.m252()->fromTree (rg); else rack.m252()->clearAll(); }
    adoptMidiMap (root);
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
