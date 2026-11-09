// Click / pop detector shared by both instruments' tests.
// For every parameter: render a held chord twice from identical fresh instances, once untouched (A) and once with the
// parameter moved mid-note (B). D = B - A is exactly what the move added. A smooth move makes D grow over several
// milliseconds; an instant jump (a click, a zipper step, a waveform switch without crossfade) makes D appear at full size
// in the very first millisecond. ratio = largest step of D in the first 1.5 ms / largest step 40-120 ms later.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <functional>
#include <memory>
#include <cstdlib>

namespace clicktest
{
struct Result { juce::String id; float to; double ratio, early, late; bool nondet; };

// largest sample-to-sample step of D = a - b: clicks are broadband, slow drifts (DC, low notes) are not
inline double maxStep (const std::vector<float>& a, const std::vector<float>& b, int s, int e)
{
    double m = 0;
    for (int i = std::max (1, s); i < std::min ((int) a.size(), e); ++i)
    {
        const double d1 = (double) a[(size_t) i] - b[(size_t) i], d0 = (double) a[(size_t) i - 1] - b[(size_t) i - 1];
        m = std::max (m, std::abs (d1 - d0));
    }
    return m;
}

inline double rms (const std::vector<float>& a, const std::vector<float>& b, int s, int e)
{
    double sum = 0; int n = 0;
    for (int i = std::max (0, s); i < std::min ((int) a.size(), e); ++i, ++n) { const double d = (double) a[(size_t) i] - b[(size_t) i]; sum += d * d; }
    return n ? std::sqrt (sum / n) : 0.0;
}

template <typename Proc>
void render (Proc& proc, std::vector<float>& L, std::vector<float>& R, const std::vector<int>& notes, int total, int change,
             juce::RangedAudioParameter* param, float target, double sr, int block)
{
    proc.prepareToPlay (sr, block);
    juce::AudioBuffer<float> buf (2, block);
    L.clear(); R.clear();
    for (int pos = 0; pos < total; pos += block)
    {
        juce::MidiBuffer midi;
        if (pos == 0) for (int nn : notes) midi.addEvent (juce::MidiMessage::noteOn (1, nn, (juce::uint8) 100), 0);
        if (pos == change && param != nullptr) param->setValueNotifyingHost (target);
        buf.clear();
        proc.processBlock (buf, midi);
        for (int i = 0; i < block; ++i) { L.push_back (buf.getSample (0, i)); R.push_back (buf.getSample (1, i)); }
    }
}

template <typename Proc>
std::vector<Result> run (const std::function<std::unique_ptr<Proc>()>& make, const std::function<bool (const juce::String&)>& skip,
                         const std::vector<int>& notes, double sr = 48000.0, int block = 128)
{
    std::vector<Result> out;
    const int change = (int) (sr * 0.4) / block * block, total = change + (int) (sr * 0.2);
    std::vector<float> AL, AR, BL, BR;
    { auto a = make(); render (*a, AL, AR, notes, total, change, nullptr, 0.0f, sr, block); }
    auto probe = make();
    for (auto* ap : probe->getParameters())
    {
        auto* rp0 = dynamic_cast<juce::RangedAudioParameter*> (ap);
        if (rp0 == nullptr || skip (rp0->getParameterID())) continue;
        const auto id = rp0->getParameterID();
        for (float target : { 0.15f, 0.85f })
        {
            if (std::abs (rp0->getValue() - target) < 0.05f) continue;
            auto b = make();
            auto* rp = dynamic_cast<juce::RangedAudioParameter*> (b->getParameters()[ap->getParameterIndex()]);
            render (*b, BL, BR, notes, total, change, rp, target, sr, block);
            Result r { id, target, 0, 0, 0, false };
            r.nondet = std::max (rms (BL, AL, change - 4800, change), rms (BR, AR, change - 4800, change)) > 1.0e-6;
            const int e1 = change + (int) (sr * 0.0015), l0 = change + (int) (sr * 0.04), l1 = change + (int) (sr * 0.12);
            r.early = std::max (maxStep (BL, AL, change, e1), maxStep (BR, AR, change, e1));
            r.late = std::max (maxStep (BL, AL, l0, l1), maxStep (BR, AR, l0, l1));
            r.ratio = r.early / std::max (r.late, 1.0e-4);
            if (auto* dump = std::getenv ("CLICK_DUMP"))
                if (id == juce::String (dump))
                {
                    juce::String csv;
                    for (int i = change - 200; i < change + 2400; ++i) csv << (BL[(size_t) i] - AL[(size_t) i]) << "," << AL[(size_t) i] << "\n";
                    juce::File ("/tmp/claude-0/-home-claude/4ef04474-c1fb-5d2e-b7f2-26288e6fc533/scratchpad/dump_" + id + "_" + juce::String (target) + ".csv").replaceWithText (csv);
                }
            out.push_back (r);
        }
    }
    std::sort (out.begin(), out.end(), [] (const Result& a, const Result& b) { return a.ratio > b.ratio; });
    return out;
}

inline int report (const std::vector<Result>& r, double threshold, const char* label)
{
    int bad = 0, nd = 0;
    std::printf ("---- %s: %d moves tested ----\n", label, (int) r.size());
    for (auto& x : r)
    {
        if (x.nondet) { ++nd; continue; }
        if (x.ratio > threshold && x.early > 0.001)
        {
            ++bad;
            std::printf ("CLICK  %-22s -> %.2f   ratio %5.2f  early %.4f  late %.4f\n", x.id.toRawUTF8(), x.to, x.ratio, x.early, x.late);
        }
    }
    std::printf ("%s: %d instant jumps above %.2f (%d moves not comparable)\n", label, bad, threshold, nd);
    return bad;
}
} // namespace clicktest
