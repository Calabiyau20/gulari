#pragma once
#include "Common.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>
#include <vector>

// Small one-shot sampler for clicks, transients and noises, played on every note-on
// with its own pitch and a two-stage (attack / decay) envelope.
namespace pg
{
struct ShotData
{
    juce::String name;
    double rate = 48000.0;
    std::vector<float> data;   // mono, plus 4 guard samples
    int length = 0;
};
using ShotPtr = std::shared_ptr<const ShotData>;

const std::vector<ShotPtr>& builtInShots();
ShotPtr loadShotFile (const juce::File& file, juce::String& error);

class OneShotPlayer
{
public:
    void prepare (double sampleRate) { sr = sampleRate; playing = false; }
    void trigger (const ShotData& s, float startFrac, float rateRatio, float attackSec, float decaySec)
    {
        pos = (double) clamp01 (startFrac) * s.length;
        step = s.rate / sr * (double) rateRatio;
        attackInc = attackSec <= 0.0005f ? 1.0f : 1.0f / (attackSec * (float) sr);
        env = attackSec <= 0.0005f ? 1.0f : 0.0f;
        decayCoef = std::exp (-6.9f / juce::jmax (1.0f, decaySec * (float) sr));   // -60 dB over the decay
        attacking = attackSec > 0.0005f;
        playing = true;
    }
    void setRatio (const ShotData& s, float rateRatio) { step = s.rate / sr * (double) rateRatio; }
    inline float tick (const ShotData& s)
    {
        if (! playing) return 0.0f;
        const int i = (int) pos;
        if (i >= s.length - 1) { playing = false; return 0.0f; }
        const float t = (float) (pos - (double) i);
        const float* d = s.data.data();
        const float x = hermite (i > 0 ? d[i - 1] : d[i], d[i], d[i + 1], d[i + 2], t);
        pos += step;
        if (attacking) { env += attackInc; if (env >= 1.0f) { env = 1.0f; attacking = false; } }
        else { env *= decayCoef; if (env < 1.0e-4f) playing = false; }
        return x * env;
    }
    bool isPlaying() const { return playing; }

private:
    double sr = 48000.0, pos = 0.0, step = 1.0;
    float env = 0.0f, attackInc = 1.0f, decayCoef = 0.999f;
    bool attacking = false, playing = false;
};
} // namespace pg
