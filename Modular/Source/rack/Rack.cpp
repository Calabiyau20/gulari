#include "Rack.h"
#include "../modules/M259.h"
#include "../modules/MidiInterface.h"
#include "../modules/OutputModule.h"
#include "../modules/M281.h"
#include "../modules/M292.h"
#include "../modules/M266.h"
#include "../modules/M245.h"
#include "../modules/M242.h"
#include "../modules/M257.h"
#include "../modules/M258.h"
#include "../modules/M264.h"
#include "../modules/M230.h"
#include "../modules/M285.h"
#include "../modules/M277.h"
#include "../modules/M207.h"
#include "../modules/M296.h"
#include "../modules/M248.h"
#include "../modules/M218.h"
#include "../modules/M250.h"
#include "../modules/M252.h"
#include "../modules/M267.h"
#include "../modules/M275.h"
#include "../modules/M284.h"
#include "../modules/M291.h"
#include "../modules/M666.h"

namespace wc
{
juce::String instName (int inst)
{
    if (inst == kInstMidi) return "MIDI";
    if (inst == kInstOutput) return "OUTPUT";
    const auto letter = juce::String::charToString ((juce::juce_wchar) ('A' + subIndex (inst)));
    switch (typeOfInst (inst)) { case MT_218: return "218"; case MT_242: return "242"; case MT_245: return "245"; case MT_248: return "248"; case MT_250: return "250"; case MT_252: return "252"; case MT_266: return "266"; case MT_230: return "230"; case MT_264: return "264"; case MT_257: return "257"; case MT_258: return "258 " + letter; case MT_285: return "285"; case MT_296: return "296"; case MT_277: return "277"; case MT_207: return "207"; case MT_267: return "267"; case MT_275: return "275"; case MT_284: return "284"; case MT_291: return "291"; case MT_666: return "SC-666 " + juce::String (subIndex (inst) + 1); case MT_281: return "281 " + letter; case MT_292: return "292 " + letter; default: return "259 " + letter; }
}

Rack::Rack (juce::AudioProcessorValueTreeState& apvts)
{
    mods[(size_t) kInstMidi] = std::make_unique<MidiInterface> (apvts);
    mods[(size_t) kInst218] = std::make_unique<M218> (apvts, "kb_");
    mods[(size_t) kInst242] = std::make_unique<M242> (apvts, "pp_");
    mods[(size_t) kInst245] = std::make_unique<M245> (apvts, "sv_");
    mods[(size_t) kInst248] = std::make_unique<M248> (apvts, "mf_");
    mods[(size_t) kInst250] = std::make_unique<M250> (apvts, "sq_");
    mods[(size_t) kInst252] = std::make_unique<M252> (apvts, "rg_");
    mods[(size_t) kInst266] = std::make_unique<M266> (apvts, "ua_");
    mods[(size_t) kInst230] = std::make_unique<M230> (apvts, "ef_");
    mods[(size_t) kInst264] = std::make_unique<M264> (apvts, "sh_");
    mods[(size_t) kInst257] = std::make_unique<M257> (apvts, "vp_");
    for (int i = 0; i < kMax258; ++i) mods[(size_t) (kInst258 + i)] = std::make_unique<M258> (apvts, prefix258 (i));
    for (int i = 0; i < kMax281; ++i) mods[(size_t) (kInst281 + i)] = std::make_unique<M281> (apvts, prefix281 (i));
    for (int i = 0; i < kMax259; ++i) mods[(size_t) (kInst259 + i)] = std::make_unique<M259> (apvts, prefix259 (i));
    for (int i = 0; i < kMax292; ++i) mods[(size_t) (kInst292 + i)] = std::make_unique<M292> (apvts, prefix292 (i));
    mods[(size_t) kInst285] = std::make_unique<M285> (apvts, "fs_");
    mods[(size_t) kInst296] = std::make_unique<M296> (apvts, "sp_");
    mods[(size_t) kInst277] = std::make_unique<M277> (apvts, "dl_");
    mods[(size_t) kInst207] = std::make_unique<M207> (apvts, "mx_");
    mods[(size_t) kInst267] = std::make_unique<M267> (apvts, "us_");
    mods[(size_t) kInst275] = std::make_unique<M275> (apvts, "rv_");
    mods[(size_t) kInst284] = std::make_unique<M284> (apvts, "ev_");
    mods[(size_t) kInst291] = std::make_unique<M291> (apvts, "tm_");
    for (int i = 0; i < kMax666; ++i) mods[(size_t) (kInst666 + i)] = std::make_unique<M666> (apvts, prefix666 (i));
    mods[(size_t) kInstOutput] = std::make_unique<OutputModule> (apvts);
    for (int i = 0; i < kNumInst; ++i)
    {
        mods[(size_t) i]->patched = patched[(size_t) i].data();
        numIn[(size_t) i] = juce::jmin (kMaxJacks, specFor (typeOfInst (i)).numIn());
        live.srcInst[(size_t) i].fill (-1);
        pending.srcInst[(size_t) i].fill (-1);
    }
    live.present[(size_t) kInstMidi] = live.present[(size_t) kInstOutput] = true;
}

Rack::~Rack() = default;

M259* Rack::m259 (int i) const { return static_cast<M259*> (mods[(size_t) (kInst259 + i)].get()); }
M281* Rack::m281 (int i) const { return static_cast<M281*> (mods[(size_t) (kInst281 + i)].get()); }
M292* Rack::m292 (int i) const { return static_cast<M292*> (mods[(size_t) (kInst292 + i)].get()); }
M266* Rack::m266() const { return static_cast<M266*> (mods[(size_t) kInst266].get()); }
M245* Rack::m245() const { return static_cast<M245*> (mods[(size_t) kInst245].get()); }
M257* Rack::m257() const { return static_cast<M257*> (mods[(size_t) kInst257].get()); }
M258* Rack::m258 (int i) const { return static_cast<M258*> (mods[(size_t) (kInst258 + i)].get()); }
M264* Rack::m264() const { return static_cast<M264*> (mods[(size_t) kInst264].get()); }
M285* Rack::m285() const { return static_cast<M285*> (mods[(size_t) kInst285].get()); }
M277* Rack::m277() const { return static_cast<M277*> (mods[(size_t) kInst277].get()); }
M252* Rack::m252() const { return static_cast<M252*> (mods[(size_t) kInst252].get()); }
M250* Rack::m250() const { return static_cast<M250*> (mods[(size_t) kInst250].get()); }
M218* Rack::m218() const { return static_cast<M218*> (mods[(size_t) kInst218].get()); }
M248* Rack::m248() const { return static_cast<M248*> (mods[(size_t) kInst248].get()); }
M296* Rack::m296() const { return static_cast<M296*> (mods[(size_t) kInst296].get()); }
M207* Rack::m207() const { return static_cast<M207*> (mods[(size_t) kInst207].get()); }
M230* Rack::m230() const { return static_cast<M230*> (mods[(size_t) kInst230].get()); }
M267* Rack::m267() const { return static_cast<M267*> (mods[(size_t) kInst267].get()); }
M275* Rack::m275() const { return static_cast<M275*> (mods[(size_t) kInst275].get()); }
M284* Rack::m284() const { return static_cast<M284*> (mods[(size_t) kInst284].get()); }
M291* Rack::m291() const { return static_cast<M291*> (mods[(size_t) kInst291].get()); }
M666* Rack::m666 (int i) const { return static_cast<M666*> (mods[(size_t) (kInst666 + i)].get()); }
M242* Rack::m242() const { return static_cast<M242*> (mods[(size_t) kInst242].get()); }
MidiInterface& Rack::midi() const { return *static_cast<MidiInterface*> (mods[(size_t) kInstMidi].get()); }
OutputModule& Rack::output() const { return *static_cast<OutputModule*> (mods[(size_t) kInstOutput].get()); }

void Rack::prepare (double sampleRate)
{
    for (auto& m : mods) m->prepare (sampleRate);
    for (auto& o : outVal) o.fill (0.0f);
}

void Rack::setPatch (const std::vector<Cable>& cables, const std::array<bool, kNumInst>& present)
{
    Patch p;
    for (int i = 0; i < kNumInst; ++i) { p.srcInst[(size_t) i].fill (-1); p.srcJack[(size_t) i].fill (-1); }
    p.present = present;
    p.present[(size_t) kInstMidi] = p.present[(size_t) kInstOutput] = true;
    for (auto& c : cables)
    {
        if (c.srcInst < 0 || c.dstInst < 0 || c.srcInst >= kNumInst || c.dstInst >= kNumInst) continue;
        if (! p.present[(size_t) c.srcInst] || ! p.present[(size_t) c.dstInst]) continue;
        if (c.dstJack < 0 || c.dstJack >= kMaxJacks || c.srcJack < 0 || c.srcJack >= kMaxJacks) continue;
        p.srcInst[(size_t) c.dstInst][(size_t) c.dstJack] = c.srcInst;
        p.srcJack[(size_t) c.dstInst][(size_t) c.dstJack] = c.srcJack;
    }
    {
        const juce::SpinLock::ScopedLockType sl (patchLock);
        pending = p;
    }
    patchDirty.store (true);
}

void Rack::applyPending() noexcept
{
    if (! patchDirty.load()) return;
    const juce::SpinLock::ScopedTryLockType tl (patchLock);
    if (! tl.isLocked()) return;    // the UI is writing: try again next block
    for (int i = 0; i < kNumInst; ++i)
        if (live.present[(size_t) i] && ! pending.present[(size_t) i])
        {
            outVal[(size_t) i].fill (0.0f);   // a module leaving the rack goes silent
            mods[(size_t) i]->reset();
        }
        else if (! live.present[(size_t) i] && pending.present[(size_t) i])
            mods[(size_t) i]->reset();
    live = pending;
    for (int i = 0; i < kNumInst; ++i)
        for (int j = 0; j < kMaxJacks; ++j) patched[(size_t) i][(size_t) j] = live.srcInst[(size_t) i][(size_t) j] >= 0;
    patchDirty.store (false);
}

void Rack::beginBlock (int numSamples)
{
    applyPending();
    for (int i = 0; i < kNumInst; ++i)
        if (live.present[(size_t) i]) mods[(size_t) i]->beginBlock (numSamples);
}

void Rack::handleMidi (const juce::MidiMessage& m)
{
    midi().handle (m);
    if (live.present[(size_t) kInst218]) m218()->handleMidi (m);   // the 218 plays from MIDI too
}

void Rack::tick() noexcept
{
    float in[kMaxJacks];
    for (int i = 0; i < kNumInst; ++i)
    {
        if (! live.present[(size_t) i]) continue;
        const auto& si = live.srcInst[(size_t) i];
        const auto& sj = live.srcJack[(size_t) i];
        for (int j = 0, n = numIn[(size_t) i]; j < n; ++j)
            in[j] = si[(size_t) j] >= 0 ? outVal[(size_t) si[(size_t) j]][(size_t) sj[(size_t) j]] : 0.0f;
        mods[(size_t) i]->tick (in, outVal[(size_t) i].data());
    }
}

float Rack::outL() const noexcept { return output().left; }
float Rack::outR() const noexcept { return output().right; }
} // namespace wc
