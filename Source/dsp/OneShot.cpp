#include "OneShot.h"
#include "Sample.h"

namespace pg
{
namespace
{
    ShotPtr synth (const juce::String& name, float seconds, std::function<float (float t, int i, Rng& r)> fn)
    {
        auto s = std::make_shared<ShotData>();
        s->name = name;
        s->rate = 48000.0;
        s->length = (int) (seconds * 48000.0f);
        s->data.assign ((size_t) s->length + 4, 0.0f);
        Rng r; r.seed (0xC1A1u + (uint32_t) name.hashCode());
        float peak = 0.0f;
        for (int i = 0; i < s->length; ++i)
        {
            const float v = fn ((float) i / 48000.0f, i, r);
            s->data[(size_t) i] = v;
            peak = juce::jmax (peak, std::abs (v));
        }
        if (peak > 0.0f) for (auto& v : s->data) v *= 0.9f / peak;
        return s;
    }

    // tiny resonant band-pass for the noise based sounds
    struct Bp
    {
        float ic1 = 0, ic2 = 0, a1 = 0, a2 = 0, a3 = 0, k = 1;
        Bp (float fc, float q) { const float g = std::tan (kPi * fc / 48000.0f); k = 1.0f / q; a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2; }
        float bp (float x) { const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3; ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2; return v1; }
        float hp (float x) { const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3; ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2; return x - k * v1 - v2; }
    };

    std::vector<ShotPtr> buildAll()
    {
        std::vector<ShotPtr> v;
        v.push_back (synth ("Click", 0.02f, [] (float t, int i, Rng&) {
            return (i == 0 ? 1.0f : 0.0f) + std::sin (kTwoPi * 3200.0f * t) * std::exp (-t * 900.0f); }));
        v.push_back (synth ("Tick", 0.03f, [bp = std::make_shared<Bp> (6000.0f, 4.0f)] (float t, int, Rng& r) {
            return bp->bp (r.bi()) * std::exp (-t * 400.0f); }));
        v.push_back (synth ("Kick Click", 0.12f, [ph = std::make_shared<float> (0.0f)] (float t, int, Rng&) {
            const float f = 60.0f + 1800.0f * std::exp (-t * 90.0f);
            *ph += f / 48000.0f;
            return std::sin (kTwoPi * *ph) * std::exp (-t * 30.0f); }));
        v.push_back (synth ("Rim", 0.08f, [bp = std::make_shared<Bp> (1700.0f, 18.0f)] (float t, int i, Rng& r) {
            return bp->bp ((i < 48 ? r.bi() : 0.0f) * 4.0f) + 0.4f * std::sin (kTwoPi * 820.0f * t) * std::exp (-t * 80.0f); }));
        v.push_back (synth ("Snap", 0.15f, [bp = std::make_shared<Bp> (2400.0f, 1.2f)] (float t, int, Rng& r) {
            return bp->bp (r.bi()) * (std::exp (-t * 40.0f) + 0.6f * std::exp (-std::abs (t - 0.012f) * 300.0f)); }));
        v.push_back (synth ("Noise Burst", 0.25f, [] (float t, int, Rng& r) {
            return r.bi() * std::exp (-t * 14.0f); }));
        v.push_back (synth ("Hat", 0.18f, [bp = std::make_shared<Bp> (8000.0f, 0.8f)] (float t, int, Rng& r) {
            float m = 0.0f;
            static const float f[] = { 2.0f * 205.3f, 2.0f * 304.4f, 2.0f * 369.6f, 2.0f * 522.7f, 2.0f * 540.0f, 2.0f * 800.0f };
            for (float ff : f) m += std::sin (kTwoPi * ff * 3.0f * t) > 0.0f ? 1.0f : -1.0f;
            return bp->hp (m * 0.15f + r.bi() * 0.5f) * std::exp (-t * 28.0f); }));
        v.push_back (synth ("Zap", 0.2f, [ph = std::make_shared<float> (0.0f)] (float t, int, Rng&) {
            const float f = 80.0f + 6000.0f * std::exp (-t * 35.0f);
            *ph += f / 48000.0f;
            return std::sin (kTwoPi * *ph) * std::exp (-t * 12.0f); }));
        v.push_back (synth ("Blip", 0.09f, [] (float t, int, Rng&) {
            return std::sin (kTwoPi * 1760.0f * t) * std::exp (-t * 45.0f) * clamp01 (t * 2000.0f); }));
        v.push_back (synth ("Metal Hit", 0.6f, [] (float t, int, Rng&) {
            static const float f[] = { 312.0f, 793.0f, 1121.0f, 1767.0f, 2419.0f, 3305.0f };
            float m = 0.0f;
            for (int k = 0; k < 6; ++k) m += std::sin (kTwoPi * f[k] * t) * std::exp (-t * (6.0f + 4.0f * (float) k)) / (1.0f + 0.4f * (float) k);
            return m; }));
        v.push_back (synth ("Crackle", 1.0f, [c = std::make_shared<float> (0.0f)] (float, int, Rng& r) {
            *c *= 0.9f;
            if (r.uni() < 0.004f) *c = r.bi();
            return *c + r.bi() * 0.02f; }));
        v.push_back (synth ("Breath", 0.8f, [bp = std::make_shared<Bp> (1200.0f, 0.7f)] (float t, int, Rng& r) {
            const float env = std::sin (kPi * clamp01 (t / 0.8f));
            return bp->bp (r.bi()) * env * env; }));
        return v;
    }
}

const std::vector<ShotPtr>& builtInShots()
{
    static const std::vector<ShotPtr> all = buildAll();
    return all;
}

ShotPtr loadShotFile (const juce::File& file, juce::String& error)
{
    auto smp = loadSampleFile (file, error, 20.0);
    if (smp == nullptr) return nullptr;
    auto s = std::make_shared<ShotData>();
    s->name = file.getFileNameWithoutExtension();
    s->rate = smp->sampleRate;
    s->length = smp->length;
    s->data.assign ((size_t) s->length + 4, 0.0f);
    for (int i = 0; i < s->length; ++i)
        s->data[(size_t) i] = 0.5f * (smp->audio.getSample (0, i) + smp->audio.getSample (1, i));
    return s;
}
} // namespace pg
