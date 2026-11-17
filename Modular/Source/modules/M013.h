#pragma once
// TMJ-13 Tactile Matrix - a Calabi Yau original. An XY pad with a point and eight nodes on its edge, clockwise from the
// top left: 1 (0,1), 2 (.5,1), 3 (1,1), 4 (1,.5), 5 (1,0), 6 (.5,0), 7 (0,0), 8 (0,.5); u to the right, v upward.
// The mouse sets the base B (X, Y around the centre times SCALE X / Y, clipped to the pad). TACTILE: the point P = B.
// MODULATED: CV n pushes P from B toward node n, 10 % of the way per volt (negative: away), shared past 10 V in all,
// clipped to the pad. Node n gives 0..10 V by how near P is: 10 x (1 + cos (pi d / r)) / 2 within r of it, 0 beyond;
// r = 0.15 .. 1.5 pad sides by SENSITIVITY; never normalised. Outputs: X, Y (0..10 V, P) and the eight nodes, which
// have no jack on the panel: only the ASSIGN buttons take them (and they run with no cable).
#include "../rack/Module.h"
#include "Dsp.h"

namespace wc
{
class M013 : public Module
{
public:
    static constexpr int kNodes = 8;
    enum Out { Out_X, Out_Y, Out_Node1 };   // node n (1..8): Out_Node1 + n - 1
    static int inCv (int n) { return n - 1; }  // CV n, n = 1..8
    static constexpr float nodeU[kNodes] { 0.0f, 0.5f, 1.0f, 1.0f, 1.0f, 0.5f, 0.0f, 0.0f };
    static constexpr float nodeV[kNodes] { 1.0f, 1.0f, 1.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f };

    // The one formula, for the audio and for the panel with the audio stopped. k: the eight CV / 10 times the mode
    // (nullptr: none). Gives P and the eight node voltages.
    static void compute (float x, float y, float sens, float sx, float sy, const float* k, float& pu, float& pv, float* volts) noexcept;

    M013 (const Params& params, const juce::String& prefix);
    void prepare (double sampleRate) override { rampLen = juce::jmax (1, (int) (0.012 * sampleRate)); first = true; }
    void reset() override { first = true; }   // in the rack again: where the knobs are, no glide
    void beginBlock (int numSamples) override;
    void tick (const float* in, float* out) noexcept override;
    float led (int id) const noexcept override;   // node id + 1, its volts / 10

    // UI thread: the point and the node voltages as the audio left them, or, when it has not run for 250 ms (a host
    // with the transport stopped), from the knobs without c.v. (H5)
    void view (float& pu, float& pv, float* volts) const;

private:
    std::atomic<float>* xP = nullptr; std::atomic<float>* yP = nullptr; std::atomic<float>* sensP = nullptr;
    std::atomic<float>* sxP = nullptr; std::atomic<float>* syP = nullptr; std::atomic<float>* modeP = nullptr;
    std::atomic<float>* knob[5] {};                       // x, y, sens, scale x, scale y as the APVTS holds them (view)
    float lastU = 0.5f, lastV = 0.5f, last[kNodes] {};    // the last sample, published on the next control tick
    dsp::Ramp x, y, sens, sx, sy, mode;
    int rampLen = 576;
    bool first = true;
    std::atomic<float> shownU { 0.5f }, shownV { 0.5f };
    std::array<std::atomic<float>, kNodes> shown {};
    std::atomic<juce::uint32> blocks { 0 };               // the audio counts its blocks; the UI watches it move
    mutable juce::uint32 seenBlocks = 0, seenAt = 0;      // UI thread only
};
} // namespace wc
