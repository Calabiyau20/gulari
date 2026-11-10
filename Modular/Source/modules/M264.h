#pragma once
// QS-264 Quad Sample & Hold / Polyphonic Adaptor - emulation of the Model 264 / 264t.
// Four channels: pulse in, c.v. in, sample (latch on the rising edge) or track (follow while the pulse is high) mode,
// pass-through pulse out, c.v. out. COMMON: the 1st c.v. input feeds all four channels. POLYPHONIC inputs: each pulse goes to
// the next voice (1..voices), samples the poly c.v. input into it and fires its pulse output. OFFSET +-2 V on every output,
// MODULATION input (audio, with depth) added before the hold. ART outputs = quantized c.v. (key + scale m / ch / M).
#include "../rack/Module.h"
#include "Dsp.h"

namespace wc
{
class M264 : public Module
{
public:
    enum In { In_Pulse1, In_Pulse2, In_Pulse3, In_Pulse4, In_Cv1, In_Cv2, In_Cv3, In_Cv4, In_PolyCv, In_Poly, In_Mod, In_Count };
    enum Out { Out_Pulse1, Out_Pulse2, Out_Pulse3, Out_Pulse4, Out_Cv1, Out_Cv2, Out_Cv3, Out_Cv4, Out_Art1, Out_Art2, Out_Art3, Out_Art4, Out_Count };
    // ART outputs: the c.v. outputs quantized to KEY / SCALE (1.2 V/oct) - the plugin's stand-in for the 264t digital ART bus
    static float quantize (float v, int scale, int key) noexcept;
    enum Led { Led_1, Led_2, Led_3, Led_4, Led_Count };

    M264 (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix);
    void prepare (double sampleRate) override;
    void beginBlock (int numSamples) override;
    void tick (const float* in, float* out) noexcept override;
    void reset() override;
    float led (int id) const noexcept override { return id >= 0 && id < Led_Count ? leds[(size_t) id].load (std::memory_order_relaxed) : 0.0f; }

private:
    std::atomic<float>* modeP[4] {};
    std::atomic<float>* commonP = nullptr; std::atomic<float>* voicesP = nullptr; std::atomic<float>* offP = nullptr; std::atomic<float>* modP = nullptr; std::atomic<float>* scaleP = nullptr; std::atomic<float>* keyP = nullptr;
    bool track[4] {}, common = false;
    int voices = 4, scale = 1, key = 0;
    float off = 0.0f, modK = 0.0f, fs = 48000.0f;
    float held[4] {};
    bool was[4] {}, polyWas = false;
    int polyNext = 0, polyPulse[4] {};
    float flash[4] {};
    std::array<std::atomic<float>, Led_Count> leds {};
};
} // namespace wc
