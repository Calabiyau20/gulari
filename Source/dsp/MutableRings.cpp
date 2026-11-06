#include "MutableWrappers.h"
#include <algorithm>
#include <cstdlib>
#include <new>

#include "rings/dsp/part.h"

namespace pg
{
// The Mutable code was written for firmware where static memory starts zeroed, and some
// members are never initialised explicitly. Allocate the wrappers from zeroed memory.
struct ZeroedAlloc
{
    static void* operator new (size_t size)
    {
        void* p = std::calloc (1, size);
        if (p == nullptr) throw std::bad_alloc();
        return p;
    }
    static void operator delete (void* p) { std::free (p); }
};

// =============================================================================
// Rings
// =============================================================================
struct RingsFx::Impl : ZeroedAlloc
{
    rings::Part part;
    uint16_t reverbBuffer[32768];
    rings::PerformanceState perf {};
    rings::Patch patch {};
    float in[rings::kMaxBlockSize], out[rings::kMaxBlockSize], aux[rings::kMaxBlockSize];
    Impl()
    {
        std::fill (std::begin (reverbBuffer), std::end (reverbBuffer), (uint16_t) 0);
        part.Init (reverbBuffer);
    }
};

RingsFx::RingsFx() : impl (std::make_unique<Impl>()) {}
RingsFx::~RingsFx() = default;

void RingsFx::prepare (double hostRate) { bridge.prepare (hostRate, 48000.0, (int) rings::kMaxBlockSize); }

void RingsFx::process (float* l, float* r, int n)
{
    auto& d = *impl;
    const int poly = prm.polyphony <= 1 ? 1 : (prm.polyphony == 2 ? 2 : 4);
    d.part.set_polyphony (poly);
    d.part.set_model ((rings::ResonatorModel) std::clamp (prm.model, 0, 5));
    d.patch.structure = prm.structure;
    d.patch.brightness = prm.brightness;
    d.patch.damping = prm.damping;
    d.patch.position = prm.position;
    const float mix = prm.mix;

    bridge.process (l, r, n, [&] (float* frames)
    {
        const int bs = (int) rings::kMaxBlockSize;
        for (int i = 0; i < bs; ++i)
            d.in[i] = (frames[i * 2] + frames[i * 2 + 1]) * 0.5f;
        d.perf.internal_exciter = false;
        d.perf.internal_note = false;
        d.perf.internal_strum = ! prm.followKeys;
        d.perf.strum = pendingStrum && prm.followKeys;
        pendingStrum = false;
        d.perf.tonic = 0.0f;
        d.perf.note = prm.note;
        d.perf.fm = 0.0f;
        d.perf.chord = 0;
        d.part.Process (d.perf, d.patch, d.in, d.out, d.aux, (size_t) bs);
        for (int i = 0; i < bs; ++i)
        {
            const float dl = frames[i * 2], dr = frames[i * 2 + 1];
            frames[i * 2] = dl + (d.out[i] - dl) * mix;
            frames[i * 2 + 1] = dr + (d.aux[i] - dr) * mix;
        }
    });
}

} // namespace pg
