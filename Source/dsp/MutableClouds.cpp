#include "MutableWrappers.h"
#include <algorithm>
#include <cstdlib>
#include <new>

#include "clouds/dsp/granular_processor.h"

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
// Clouds
// =============================================================================
struct CloudsFx::Impl : ZeroedAlloc
{
    clouds::GranularProcessor proc;
    std::vector<uint8_t> big, small;
    clouds::ShortFrame in[32], out[32];
    bool needTrigger;
    int lastMode;
    Impl() : big (118784), small (65536 - 128)
    {
        proc.Init (big.data(), big.size(), small.data(), small.size());
        proc.set_silence (false);
        needTrigger = true;
        lastMode = -1;
    }
};

CloudsFx::CloudsFx() : impl (std::make_unique<Impl>()) {}
CloudsFx::~CloudsFx() = default;

void CloudsFx::prepare (double hostRate) { bridge.prepare (hostRate, 32000.0, 32); }

void CloudsFx::process (float* l, float* r, int n)
{
    auto& d = *impl;
    static const int modeMap[] = { 0, 2, 3 };   // Granular, Looping Delay, Spectral
    const int mode = modeMap[std::clamp (prm.mode, 0, 2)];
    d.proc.set_playback_mode ((clouds::PlaybackMode) mode);
    d.proc.set_quality (std::clamp (prm.quality, 0, 3));
    d.proc.set_freeze (prm.freeze);
    auto* p = d.proc.mutable_parameters();
    p->position = prm.position;
    p->size = prm.size;
    p->pitch = prm.pitch;
    p->density = prm.density;
    p->texture = prm.texture;
    p->dry_wet = 1.0f;   // the dry/wet mix is done here, sample-aligned, at the internal rate
    p->stereo_spread = prm.spread;
    p->feedback = prm.feedback;
    p->reverb = prm.reverb;
    p->freeze = prm.freeze;
    if (mode != d.lastMode) { d.needTrigger = true; d.lastMode = mode; }
    p->gate = false;

    bridge.process (l, r, n, [&] (float* frames)
    {
        for (int i = 0; i < 32; ++i)
        {
            d.in[i].l = (short) std::clamp (frames[i * 2] * 32767.0f, -32767.0f, 32767.0f);
            d.in[i].r = (short) std::clamp (frames[i * 2 + 1] * 32767.0f, -32767.0f, 32767.0f);
        }
        d.proc.mutable_parameters()->trigger = d.needTrigger;   // starts the Stretch envelope
        d.needTrigger = false;
        d.proc.Prepare();
        d.proc.Process (d.in, d.out, 32);
        const float mix = prm.mix;
        for (int i = 0; i < 32; ++i)
        {
            const float wl = (float) d.out[i].l / 32768.0f, wr = (float) d.out[i].r / 32768.0f;
            frames[i * 2] += (wl - frames[i * 2]) * mix;
            frames[i * 2 + 1] += (wr - frames[i * 2 + 1]) * mix;
        }
    });
}
} // namespace pg
