#include "Rack.h"
#include "../modules/M259.h"
#include "../modules/MidiInterface.h"
#include "../modules/OutputModule.h"

namespace wc
{
juce::String instName (int inst)
{
    if (inst == kInstMidi) return "MIDI";
    if (inst == kInstOutput) return "OUTPUT";
    return "259 " + juce::String::charToString ((juce::juce_wchar) ('A' + inst - kInst259));
}

Rack::Rack (juce::AudioProcessorValueTreeState& apvts)
{
    mods[(size_t) kInstMidi] = std::make_unique<MidiInterface> (apvts);
    for (int i = 0; i < kMax259; ++i) mods[(size_t) (kInst259 + i)] = std::make_unique<M259> (apvts, prefix259 (i));
    mods[(size_t) kInstOutput] = std::make_unique<OutputModule> (apvts);
    for (int i = 0; i < kNumInst; ++i)
    {
        mods[(size_t) i]->patched = patched[(size_t) i].data();
        live.srcInst[(size_t) i].fill (-1);
        pending.srcInst[(size_t) i].fill (-1);
    }
    live.present[(size_t) kInstMidi] = live.present[(size_t) kInstOutput] = true;
}

Rack::~Rack() = default;

M259* Rack::m259 (int i) const { return static_cast<M259*> (mods[(size_t) (kInst259 + i)].get()); }
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

void Rack::handleMidi (const juce::MidiMessage& m) { midi().handle (m); }

void Rack::tick() noexcept
{
    float in[kMaxJacks];
    for (int i = 0; i < kNumInst; ++i)
    {
        if (! live.present[(size_t) i]) continue;
        const auto& si = live.srcInst[(size_t) i];
        const auto& sj = live.srcJack[(size_t) i];
        for (int j = 0; j < kMaxJacks; ++j)
            in[j] = si[(size_t) j] >= 0 ? outVal[(size_t) si[(size_t) j]][(size_t) sj[(size_t) j]] : 0.0f;
        mods[(size_t) i]->tick (in, outVal[(size_t) i].data());
    }
}

float Rack::outL() const noexcept { return output().left; }
float Rack::outR() const noexcept { return output().right; }
} // namespace wc
