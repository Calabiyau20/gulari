#include "Sample.h"
#include "Common.h"

namespace pg
{
static juce::AudioFormatManager& formatManager()
{
    // WAV, AIFF, FLAC, Ogg, MP3 (JUCE_USE_MP3AUDIOFORMAT) and CoreAudio formats on macOS
    static juce::AudioFormatManager fm;
    static bool registered = [] { fm.registerBasicFormats(); return true; }();
    juce::ignoreUnused (registered);
    return fm;
}

juce::String supportedAudioWildcard()
{
    return "*.wav;*.wave;*.aif;*.aiff;*.flac;*.mp3;*.ogg";
}

SamplePtr makeSample (juce::AudioBuffer<float>&& source, int numFrames, double sampleRate, const juce::String& name)
{
    auto s = std::make_shared<SampleData>();
    s->length = numFrames;
    s->sampleRate = sampleRate;
    s->name = name;
    s->audio.setSize (2, numFrames + 3);
    s->audio.clear();
    for (int ch = 0; ch < 2; ++ch)
        s->audio.copyFrom (ch, 0, source, juce::jmin (ch, source.getNumChannels() - 1), 0, numFrames);

    // normalise to -1 dBFS peak so quiet files still drive the oscillator
    const float peak = s->audio.getMagnitude (0, numFrames);
    if (peak > 1.0e-6f)
        s->audio.applyGain (0.89f / peak);

    s->peaks.assign (1024, 0.0f);
    for (int i = 0; i < 1024; ++i)
    {
        const int a = (int) ((int64_t) i * numFrames / 1024);
        const int b = juce::jmax (a + 1, (int) ((int64_t) (i + 1) * numFrames / 1024));
        float m = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
        {
            auto r = juce::FloatVectorOperations::findMinAndMax (s->audio.getReadPointer (ch, a), juce::jmin (b, numFrames) - a);
            m = juce::jmax (m, std::abs (r.getStart()), std::abs (r.getEnd()));
        }
        s->peaks[(size_t) i] = m;
    }
    return s;
}

SamplePtr loadSampleFile (const juce::File& file, juce::String& error, double maxSeconds)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager().createReaderFor (file));
    if (reader == nullptr)
    {
        error = "Could not read \"" + file.getFileName() + "\". Use WAV, AIFF, FLAC, MP3 or Ogg.";
        return nullptr;
    }
    const auto maxFrames = (juce::int64) (maxSeconds * reader->sampleRate);
    const int frames = (int) juce::jmin (reader->lengthInSamples, maxFrames);
    if (frames < 64)
    {
        error = "\"" + file.getFileName() + "\" is too short.";
        return nullptr;
    }
    juce::AudioBuffer<float> buf ((int) juce::jlimit (1u, 2u, reader->numChannels), frames);
    reader->read (&buf, 0, frames, 0, true, buf.getNumChannels() > 1);
    auto s = makeSample (std::move (buf), frames, reader->sampleRate, file.getFileNameWithoutExtension());
    std::const_pointer_cast<SampleData> (s)->path = file.getFullPathName();
    return s;
}

SamplePtr makeDefaultSample()
{
    // 4 seconds of a slowly evolving vowel-like drone: several detuned
    // harmonic stacks with moving formants, plus breathy noise.
    const double sr = 48000.0;
    const int n = (int) (sr * 4.0);
    juce::AudioBuffer<float> buf (2, n);
    Rng rng;
    float pink[2] = { 0.0f, 0.0f };
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* d = buf.getWritePointer (ch);
        double ph[24] = {};
        for (int i = 0; i < n; ++i)
        {
            const float t = (float) i / (float) n;
            const float f0 = 110.0f * (1.0f + 0.003f * (ch ? 1.0f : -1.0f));
            const float f1 = 400.0f + 500.0f * std::sin (kTwoPi * t * 1.0f);
            const float f2 = 1200.0f + 900.0f * std::sin (kTwoPi * t * 0.5f + 1.0f);
            float x = 0.0f;
            for (int h = 1; h <= 24; ++h)
            {
                const float fh = f0 * (float) h;
                const float g1 = std::exp (-std::pow ((fh - f1) / 180.0f, 2.0f));
                const float g2 = 0.6f * std::exp (-std::pow ((fh - f2) / 300.0f, 2.0f));
                const float amp = (0.15f / (float) h) + g1 + g2;
                ph[h - 1] += fh / sr;
                x += amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * ph[h - 1]);
            }
            pink[ch] = 0.98f * pink[ch] + 0.02f * rng.bi();
            x = x * 0.12f + pink[ch] * 0.6f * (0.5f + 0.5f * std::sin (kTwoPi * t * 3.0f));
            // gentle fade in/out so the loop point is clean
            const float edge = juce::jmin (1.0f, (float) i / 2400.0f, (float) (n - i) / 2400.0f);
            d[i] = x * edge;
        }
    }
    auto s = makeSample (std::move (buf), n, sr, "Default Texture");
    return s;
}
} // namespace pg
