#pragma once
#include "Common.h"
#include <memory>
#include <vector>

// Wrappers that run the Mutable Instruments modules (fixed internal sample rates and
// block sizes) inside a host running at any sample rate. The Mutable headers stay
// inside MutableWrappers.cpp so the rest of the plugin compiles without them.
namespace pg
{
// Plaits voice: renders at 48 kHz in blocks of 12, resampled to the host rate.
class PlaitsSource
{
public:
    PlaitsSource();
    ~PlaitsSource();
    void prepare (double hostRate);
    void reset();

    struct Params { int engine = 8; float note = 60, harmonics = 0.5f, timbre = 0.5f, morph = 0.5f, decay = 0.5f, colour = 0.5f; bool lpg = false; };
    void setParams (const Params& p) { prm = p; }
    void trigger() { pendingTrigger = true; }
    void gate (bool g) { gateHigh = g; }
    float tick();   // one host-rate sample, -1..1 (main output)

private:
    void renderBlock();
    struct Impl;
    std::unique_ptr<Impl> impl;
    Params prm;
    bool pendingTrigger = false, gateHigh = false;
    double step = 1.0, frac = 0.0;
    float hist[4] {};          // xm1, x0, x1, x2 in internal-rate samples
    float block[12] {};
    int blockPos = 12;
};

// Generic stereo bridge: host rate <-> fixed internal rate, fixed block processing.
// The callback processes exactly 'blockSize' internal frames in place (interleaved L/R).
class RateBridge
{
public:
    void prepare (double hostRate, double internalRate, int blockSize);
    template <typename Fn> void process (float* l, float* r, int n, Fn&& processBlock);
    int latencySamples() const { return (int) std::ceil ((double) bs * host / internal) + 4; }

private:
    double host = 48000.0, internal = 48000.0, inStep = 1.0, outStep = 1.0;
    int bs = 24;
    // input side: host -> internal
    float inHist[2][4] {};
    double inFrac = 0.0;
    std::vector<float> inBlock;   // interleaved, bs frames
    int inFill = 0;
    // output side: internal -> host
    std::vector<float> outFifo;   // interleaved internal-rate frames ready to be resampled
    int outRead = 0, outAvail = 0;
    float outHist[2][4] {};
    double outFrac = 0.0;
    bool primed = false;
    std::vector<float> scratch;
};

// Rings as an effect (48 kHz, block 24). Input mono, output stereo (odd/even strings).
class RingsFx
{
public:
    RingsFx();
    ~RingsFx();
    void prepare (double hostRate);
    struct Params { int model = 0, polyphony = 1; float note = 48, structure = 0.4f, brightness = 0.5f, damping = 0.6f, position = 0.3f, mix = 0.5f; bool followKeys = true; };
    void setParams (const Params& p) { prm = p; }
    void strum() { pendingStrum = true; }
    void process (float* l, float* r, int n);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    RateBridge bridge;
    Params prm;
    bool pendingStrum = false;
};

// Clouds as an effect (32 kHz, block 32).
class CloudsFx
{
public:
    CloudsFx();
    ~CloudsFx();
    void prepare (double hostRate);
    struct Params { int mode = 0, quality = 0; bool freeze = false; float position = 0.5f, size = 0.5f, pitch = 0, density = 0.6f, texture = 0.5f, spread = 0.5f, feedback = 0.2f, reverb = 0.4f, mix = 0.5f; };
    void setParams (const Params& p) { prm = p; }
    void process (float* l, float* r, int n);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    RateBridge bridge;
    Params prm;
};

// ---------------------------------------------------------------------------
template <typename Fn>
void RateBridge::process (float* l, float* r, int n, Fn&& processBlock)
{
    // 1) host -> internal: push host samples, emit internal frames as they become available
    for (int i = 0; i < n; ++i)
    {
        for (int c = 0; c < 2; ++c)
        {
            auto& h = inHist[c];
            h[0] = h[1]; h[1] = h[2]; h[2] = h[3]; h[3] = (c == 0 ? l[i] : r[i]);
        }
        // internal sample times that fall between host samples h[1]..h[2]
        while (inFrac < 1.0)
        {
            const float t = (float) inFrac;
            inBlock[(size_t) inFill * 2] = hermite (inHist[0][0], inHist[0][1], inHist[0][2], inHist[0][3], t);
            inBlock[(size_t) inFill * 2 + 1] = hermite (inHist[1][0], inHist[1][1], inHist[1][2], inHist[1][3], t);
            inFrac += inStep;
            if (++inFill == bs)
            {
                processBlock (inBlock.data());
                // append to output FIFO
                if (outRead + outAvail + bs > (int) outFifo.size() / 2)
                {
                    // compact
                    std::copy (outFifo.begin() + outRead * 2, outFifo.begin() + (outRead + outAvail) * 2, outFifo.begin());
                    outRead = 0;
                }
                const int w = outRead + outAvail;
                if ((w + bs) * 2 <= (int) outFifo.size())
                {
                    std::copy (inBlock.begin(), inBlock.end(), outFifo.begin() + w * 2);
                    outAvail += bs;
                }
                inFill = 0;
            }
        }
        inFrac -= 1.0;
    }

    // 2) internal -> host
    if (! primed && outAvail >= bs * 2) primed = true;
    for (int i = 0; i < n; ++i)
    {
        if (! primed) { l[i] = r[i] = 0.0f; continue; }
        while (outFrac >= 1.0)
        {
            if (outAvail == 0) { primed = false; break; }
            for (int c = 0; c < 2; ++c)
            {
                auto& h = outHist[c];
                h[0] = h[1]; h[1] = h[2]; h[2] = h[3];
                h[3] = outFifo[(size_t) outRead * 2 + (size_t) c];
            }
            ++outRead; --outAvail;
            outFrac -= 1.0;
        }
        const float t = (float) outFrac;
        l[i] = hermite (outHist[0][0], outHist[0][1], outHist[0][2], outHist[0][3], t);
        r[i] = hermite (outHist[1][0], outHist[1][1], outHist[1][2], outHist[1][3], t);
        outFrac += outStep;
    }
}
} // namespace pg
