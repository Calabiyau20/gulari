#include "MutableWrappers.h"
#include <algorithm>
#include <cstdlib>
#include <new>

#include "plaits/dsp/voice.h"

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
// Plaits
// =============================================================================
struct PlaitsSource::Impl : ZeroedAlloc
{
    plaits::Voice voice;
    char buffer[16384];
    stmlib::BufferAllocator allocator;
    plaits::Patch patch {};
    plaits::Modulations mods {};
    Impl()
    {
        allocator.Init (buffer, sizeof (buffer));
        voice.Init (&allocator);
    }
};

PlaitsSource::PlaitsSource() : impl (std::make_unique<Impl>()) {}
PlaitsSource::~PlaitsSource() = default;

void PlaitsSource::prepare (double hostRate)
{
    step = 48000.0 / hostRate;
    reset();
}

void PlaitsSource::reset()
{
    frac = 0.0;
    for (auto& h : hist) h = 0.0f;
    blockPos = 12;
}

void PlaitsSource::renderBlock()
{
    auto& p = impl->patch;
    p.note = prm.note;
    p.harmonics = prm.harmonics;
    p.timbre = prm.timbre;
    p.morph = prm.morph;
    p.engine = prm.engine;
    p.decay = prm.decay;
    p.lpg_colour = prm.colour;
    p.frequency_modulation_amount = 0.0f;
    p.timbre_modulation_amount = 0.0f;
    p.morph_modulation_amount = 0.0f;

    auto& m = impl->mods;
    m = {};
    m.trigger_patched = prm.lpg;
    m.trigger = pendingTrigger ? 1.0f : 0.0f;
    m.level_patched = false;
    pendingTrigger = false;

    plaits::Voice::Frame frames[plaits::kBlockSize];
    impl->voice.Render (p, m, frames, plaits::kBlockSize);
    for (int i = 0; i < (int) plaits::kBlockSize; ++i)
        block[i] = (float) frames[i].out / 32768.0f;
    blockPos = 0;
}

float PlaitsSource::tick()
{
    while (frac >= 1.0)
    {
        if (blockPos >= 12) renderBlock();
        hist[0] = hist[1]; hist[1] = hist[2]; hist[2] = hist[3]; hist[3] = block[blockPos++];
        frac -= 1.0;
    }
    const float y = hermite (hist[0], hist[1], hist[2], hist[3], (float) frac);
    frac += step;
    return y;
}

// =============================================================================
// RateBridge
// =============================================================================
void RateBridge::prepare (double hostRate, double internalRate, int blockSize)
{
    host = hostRate;
    internal = internalRate;
    bs = blockSize;
    inStep = host / internal;
    outStep = internal / host;
    inBlock.assign ((size_t) bs * 2, 0.0f);
    outFifo.assign ((size_t) bs * 2 * 64, 0.0f);
    inFill = 0; inFrac = 0.0; outFrac = 0.0; outRead = 0; outAvail = 0; primed = false;
    for (auto& h : inHist) for (auto& v : h) v = 0.0f;
    for (auto& h : outHist) for (auto& v : h) v = 0.0f;
}

} // namespace pg
