#include "Spectral.h"
#include "Sample.h"
#include <juce_dsp/juce_dsp.h>

namespace pg
{
namespace
{
    constexpr int kFrames = 64;
    constexpr int kP = SpectralTable::kPartials;

    // scale the whole table so its loudest frame has an amplitude norm of 'target'
    void normalise (SpectralTable& t, float target = 0.8f)
    {
        float mx = 0.0f;
        for (int f = 0; f < t.numFrames; ++f)
        {
            double e = 0.0;
            for (int k = 0; k < kP; ++k) e += (double) t.amp[(size_t) (f * kP + k)] * t.amp[(size_t) (f * kP + k)];
            mx = juce::jmax (mx, (float) std::sqrt (e));
        }
        if (mx <= 0.0f) return;
        for (auto& a : t.amp) a *= target / mx;
    }

    // smooth hash noise for evolving random spectra
    float hashNoise (int a, int b)
    {
        uint32_t h = (uint32_t) a * 374761393u + (uint32_t) b * 668265263u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return (float) ((h ^ (h >> 16)) & 0xffffu) / 65535.0f;
    }
    float smoothNoise (int k, float t, int seed)
    {
        const float x = t * 7.0f;
        const int i = (int) x;
        const float f = x - (float) i;
        const float s = f * f * (3.0f - 2.0f * f);
        return hashNoise (k + seed * 131, i) * (1.0f - s) + hashNoise (k + seed * 131, i + 1) * s;
    }

    float formantEnv (float hz, float vowel)   // vowel 0..4: a e i o u
    {
        static const float F[5][3] = { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 }, { 570, 840, 2410 }, { 300, 870, 2240 } };
        static const float A[3] = { 1.0f, 0.5f, 0.25f };
        const float v = clampf (vowel, 0.0f, 4.0f);
        const int i0 = juce::jmin (3, (int) v);
        const float fr = v - (float) i0;
        float e = 0.02f;
        for (int n = 0; n < 3; ++n)
        {
            const float fc = F[i0][n] + (F[i0 + 1][n] - F[i0][n]) * fr;
            const float bw = 80.0f + fc * 0.08f;
            const float d = (hz - fc) / bw;
            e += A[n] * std::exp (-0.5f * d * d);
        }
        return e;
    }

    bool isPrime (int n)
    {
        if (n < 2) return false;
        for (int d = 2; d * d <= n; ++d) if (n % d == 0) return false;
        return true;
    }

    SpectralPtr make (const juce::String& name, std::function<float (float t, int k)> fn)
    {
        auto t = std::make_shared<SpectralTable>();
        t->name = name;
        t->numFrames = kFrames;
        t->amp.assign ((size_t) (kFrames * kP), 0.0f);
        for (int f = 0; f < kFrames; ++f)
        {
            const float tt = (float) f / (float) (kFrames - 1);
            for (int k = 1; k <= kP; ++k)
                t->amp[(size_t) (f * kP + k - 1)] = juce::jmax (0.0f, fn (tt, k));
        }
        normalise (*t);
        return t;
    }

    std::vector<SpectralPtr> buildAll()
    {
        std::vector<SpectralPtr> v;
        // 0 Saw Stack: a saw whose brightness opens with the position
        v.push_back (make ("Saw Stack", [] (float t, int k) {
            const float cut = 3.0f + 61.0f * t;
            return (1.0f / (float) k) / (1.0f + std::pow ((float) k / cut, 4.0f)); }));
        // 1 Hollow Square: odd partials, even ones fade in
        v.push_back (make ("Hollow Square", [] (float t, int k) {
            return (k % 2 ? 1.0f : 0.6f * t) / (float) k; }));
        // 2 Drawbar Organ: registrations cross-fading
        v.push_back (make ("Drawbar Organ", [] (float t, int k) {
            static const int bars[] = { 1, 2, 3, 4, 6, 8, 10, 12, 16 };
            static const float regA[] = { 1.0f, 0.8f, 0.6f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
            static const float regB[] = { 1.0f, 0.5f, 0.8f, 0.7f, 0.5f, 0.6f, 0.4f, 0.4f, 0.5f };
            for (int b = 0; b < 9; ++b) if (bars[b] == k) return regA[b] + (regB[b] - regA[b]) * t;
            return 0.0f; }));
        // 3 Choir Ah-Oh
        v.push_back (make ("Choir Ah-Oh", [] (float t, int k) {
            return formantEnv (110.0f * (float) k, t * 3.0f) / std::sqrt ((float) k); }));
        // 4 Vocal Ee-Oo
        v.push_back (make ("Vocal Ee-Oo", [] (float t, int k) {
            return formantEnv (130.0f * (float) k, 1.0f + 3.0f * t) / std::sqrt ((float) k); }));
        // 5 Bell Bloom: bright strike that dies to the hum
        v.push_back (make ("Bell Bloom", [] (float t, int k) {
            static const int bell[] = { 1, 2, 3, 5, 7, 10, 13, 17, 21, 27, 33, 41 };
            for (int b = 0; b < 12; ++b)
                if (bell[b] == k) return std::exp (-(float) b * t * 1.4f) * (1.0f / (1.0f + 0.25f * (float) b));
            return 0.0f; }));
        // 6 Glass Harmonics: square-number partials
        v.push_back (make ("Glass Harmonics", [] (float t, int k) {
            const int r = (int) std::round (std::sqrt ((float) k));
            if (r * r != k) return 0.0f;
            return 0.4f + 0.6f * std::sin (kPi * clamp01 (t * 1.4f - (float) r * 0.08f + 0.3f)); }));
        // 7 Brass Swell
        v.push_back (make ("Brass Swell", [] (float t, int k) {
            return std::pow ((float) k, -(3.0f - 2.3f * t)); }));
        // 8 String Section: bow position comb
        v.push_back (make ("String Section", [] (float t, int k) {
            const float bow = 0.08f + 0.3f * t;
            return (0.15f + std::abs (std::sin (kPi * (float) k * bow))) / (float) k; }));
        // 9 Pluck Decay: highs die first
        v.push_back (make ("Pluck Decay", [] (float t, int k) {
            return std::exp (-(float) k * t * 0.35f) * std::exp (-t * 1.5f) / (float) k; }));
        // 10 Bass Growl: a moving formant over a dark body
        v.push_back (make ("Bass Growl", [] (float t, int k) {
            const float c = 2.0f + 18.0f * (0.5f - 0.5f * std::cos (kTwoPi * t * 2.0f));
            const float d = ((float) k - c) / 2.5f;
            return std::pow ((float) k, -0.9f) * (0.3f + 2.0f * std::exp (-0.5f * d * d)); }));
        // 11 Pad Bloom: a soft cloud of partials drifting up
        v.push_back (make ("Pad Bloom", [] (float t, int k) {
            const float c = 2.0f + 28.0f * t, w = 4.0f + 10.0f * t;
            const float d = ((float) k - c) / w;
            return std::exp (-0.5f * d * d) * (0.6f + 0.4f * smoothNoise (k, t, 3)); }));
        // 12 Formant Sweep: saw through a resonant peak
        v.push_back (make ("Formant Sweep", [] (float t, int k) {
            const float c = 3.0f + 47.0f * t;
            const float d = ((float) k - c) / 1.8f;
            return (0.15f + 3.0f * std::exp (-0.5f * d * d)) / (float) k; }));
        // 13 Comb Ripple
        v.push_back (make ("Comb Ripple", [] (float t, int k) {
            return std::pow ((float) k, -0.7f) * (0.5f + 0.5f * std::cos (kTwoPi * (float) k * (0.04f + 0.21f * t))); }));
        // 14 Prime Partials
        v.push_back (make ("Prime Partials", [] (float t, int k) {
            if (k != 1 && ! isPrime (k)) return 0.0f;
            return (0.5f + 0.5f * std::sin (kTwoPi * (t + (float) k * 0.031f))) / std::sqrt ((float) k); }));
        // 15 Fibonacci: partials light up one after another
        v.push_back (make ("Fibonacci", [] (float t, int k) {
            static const int fib[] = { 1, 2, 3, 5, 8, 13, 21, 34, 55 };
            for (int b = 0; b < 9; ++b)
                if (fib[b] == k) return clamp01 (t * 9.0f - (float) b + 1.0f) / std::sqrt ((float) k) + (b == 0 ? 0.3f : 0.0f);
            return 0.0f; }));
        // 16 Odd Shimmer: odd partials with evolving random levels
        v.push_back (make ("Odd Shimmer", [] (float t, int k) {
            if (k % 2 == 0) return 0.0f;
            return smoothNoise (k, t, 7) / std::sqrt ((float) k); }));
        // 17 Spectral Wind: dense noisy band sweeping
        v.push_back (make ("Spectral Wind", [] (float t, int k) {
            const float c = 6.0f + 40.0f * (0.5f - 0.5f * std::cos (kTwoPi * t)), w = 9.0f;
            const float d = ((float) k - c) / w;
            return std::exp (-0.5f * d * d) * hashNoise (k, (int) (t * 40.0f)); }));
        // 18 Metal Plate: sparse, bright, random set changing slowly
        v.push_back (make ("Metal Plate", [] (float t, int k) {
            const float r = hashNoise (k, 99);
            if (r < 0.62f) return 0.0f;
            return (0.4f + 0.6f * smoothNoise (k, t, 11)) * std::pow ((float) k, -0.35f); }));
        // 19 Singularity: the spectrum collapses into a single resonant line
        v.push_back (make ("Singularity", [] (float t, int k) {
            const float c = 64.0f * (1.0f - t) + 1.0f;
            const float d = ((float) k - c) / (1.0f + 3.0f * (1.0f - t));
            return std::pow ((float) k, -(0.5f + 2.5f * t)) + t * 1.5f * std::exp (-0.5f * d * d) / std::sqrt ((float) k); }));
        jassert (v.size() == 20);
        return v;
    }

    // YIN pitch estimate on a mono buffer; 0 if not pitched
    float detectPitch (const float* x, int n, double sr)
    {
        const int W = juce::jmin (2048, n / 2);
        const int maxLag = juce::jmin (n - W - 1, (int) (sr / 40.0));
        const int minLag = juce::jmax (2, (int) (sr / 1200.0));
        if (W < 256 || maxLag <= minLag) return 0.0f;
        std::vector<float> d ((size_t) maxLag + 1, 0.0f);
        for (int lag = 1; lag <= maxLag; ++lag)
        {
            double s = 0.0;
            for (int i = 0; i < W; ++i) { const float e = x[i] - x[i + lag]; s += e * e; }
            d[(size_t) lag] = (float) s;
        }
        double run = 0.0;
        std::vector<float> cm ((size_t) maxLag + 1, 1.0f);
        for (int lag = 1; lag <= maxLag; ++lag)
        {
            run += d[(size_t) lag];
            cm[(size_t) lag] = run > 0.0 ? (float) (d[(size_t) lag] * lag / run) : 1.0f;
        }
        for (int lag = minLag; lag < maxLag; ++lag)
        {
            if (cm[(size_t) lag] < 0.15f)
            {
                while (lag + 1 < maxLag && cm[(size_t) lag + 1] < cm[(size_t) lag]) ++lag;
                // parabolic refinement
                const float a = cm[(size_t) lag - 1], b = cm[(size_t) lag], c = cm[(size_t) lag + 1];
                const float den = a - 2.0f * b + c;
                const float off = std::abs (den) > 1.0e-9f ? 0.5f * (a - c) / den : 0.0f;
                return (float) (sr / ((double) lag + off));
            }
        }
        return 0.0f;
    }
}

const std::vector<SpectralPtr>& builtInSpectra()
{
    static const std::vector<SpectralPtr> all = buildAll();
    return all;
}

SpectralPtr analyseSpectralFile (const juce::File& file, juce::String& error)
{
    auto smp = loadSampleFile (file, error, 30.0);
    if (smp == nullptr) return nullptr;
    const int n = smp->length;
    if (n < 512) { error = "The file is too short to analyse."; return nullptr; }
    const double sr = smp->sampleRate;
    std::vector<float> mono ((size_t) n);
    for (int i = 0; i < n; ++i) mono[(size_t) i] = 0.5f * (smp->audio.getSample (0, i) + smp->audio.getSample (1, i));

    // pitch: analyse the loudest quarter second
    const int win = juce::jmin (n, (int) (sr * 0.25));
    int best = 0;
    double bestE = -1.0;
    for (int s = 0; s + win <= n; s += juce::jmax (1, win / 2))
    {
        double e = 0.0;
        for (int i = 0; i < win; ++i) e += mono[(size_t) (s + i)] * mono[(size_t) (s + i)];
        if (e > bestE) { bestE = e; best = s; }
    }
    float f0 = detectPitch (mono.data() + best, juce::jmin (win, n - best), sr);
    if (f0 < 30.0f || f0 > 1500.0f) f0 = 130.81f;   // unpitched: read the spectral envelope at C3 spacing

    const int order = 13, N = 1 << order;   // 8192-point FFT
    juce::dsp::FFT fft (order);
    std::vector<float> buf ((size_t) N * 2), window ((size_t) N);
    for (int i = 0; i < N; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (kTwoPi * (float) i / (float) (N - 1));

    auto t = std::make_shared<SpectralTable>();
    t->name = file.getFileNameWithoutExtension();
    t->numFrames = kFrames;
    t->baseHz = f0;
    t->amp.assign ((size_t) (kFrames * kP), 0.0f);
    const float binHz = (float) (sr / N);
    for (int f = 0; f < kFrames; ++f)
    {
        const int start = (int) ((double) f / (kFrames - 1) * juce::jmax (0, n - N));
        std::fill (buf.begin(), buf.end(), 0.0f);
        for (int i = 0; i < N; ++i)
        {
            const int idx = start + i;
            if (idx < n) buf[(size_t) i] = mono[(size_t) idx] * window[(size_t) i];
        }
        fft.performFrequencyOnlyForwardTransform (buf.data());
        for (int k = 1; k <= kP; ++k)
        {
            const float hz = f0 * (float) k;
            if (hz >= (float) sr * 0.48f) break;
            const float bin = hz / binHz;
            const int w = juce::jmax (1, (int) (0.3f * f0 / binHz));
            float mx = 0.0f;
            for (int b = juce::jmax (1, (int) bin - w); b <= juce::jmin (N / 2 - 1, (int) bin + w); ++b) mx = juce::jmax (mx, buf[(size_t) b]);
            t->amp[(size_t) (f * kP + k - 1)] = mx;
        }
    }
    normalise (*t);
    return t;
}

// =============================================================================
void SpectralOsc::prepare (double sampleRate, uint32_t seed)
{
    sr = sampleRate;
    rng.seed (seed);
    noteOn (0.0f);
}

void SpectralOsc::noteOn (float)
{
    for (int k = 0; k < kN; ++k)
    {
        const float ph = rng.uni() * kTwoPi;
        cs[k] = std::cos (ph); sn[k] = std::sin (ph);
        aL[k] = aR[k] = dL[k] = dR[k] = 0.0f;
        smoothAmp[k] = -1.0f;   // first update copies the target
        flicker[k] = flickerTarget[k] = 0.0f;
    }
    playPos = 0.0f;
    active = 0;
}

void SpectralOsc::update (const SpectralTable& t, const Params& p, float freqHz, int blockLen)
{
    const float blockSecs = (float) blockLen / (float) sr;
    if (! p.freeze) playPos += p.scan * blockSecs * 0.25f;   // scan 1 = one sweep every 4 seconds
    float x = p.pos + playPos;
    x = playPos == 0.0f ? clamp01 (x) : x - std::floor (x);

    const float fpos = x * (float) (t.numFrames - 1);
    const int f0 = juce::jlimit (0, t.numFrames - 1, (int) fpos);
    const int f1 = juce::jmin (f0 + 1, t.numFrames - 1);
    const float ft = fpos - (float) f0;
    const float* A = t.frame (f0);
    const float* B = t.frame (f1);
    float raw[kN];
    double e0 = 0.0;
    for (int k = 0; k < kN; ++k) { raw[k] = A[k] + (B[k] - A[k]) * ft; e0 += (double) raw[k] * raw[k]; }

    // formant shift: read the spectral envelope at k / ratio
    float amp[kN];
    const float ratio = semisToRatio (p.shift);
    for (int k = 0; k < kN; ++k)
    {
        const float src = (float) (k + 1) / ratio - 1.0f;
        if (src < -0.5f || src > (float) (kN - 1)) { amp[k] = 0.0f; continue; }
        const int i = juce::jmax (0, (int) src);
        const float fr = juce::jmax (0.0f, src - (float) i);
        amp[k] = raw[i] + ((i + 1 < kN ? raw[i + 1] : 0.0f) - raw[i]) * fr;
    }
    // spectral blur (neighbour smear)
    if (p.blur > 0.0f)
    {
        float tmp[kN];
        const float b = p.blur * 0.66f;
        for (int k = 0; k < kN; ++k)
        {
            const float l = k > 0 ? amp[k - 1] : 0.0f, r = k + 1 < kN ? amp[k + 1] : 0.0f;
            tmp[k] = amp[k] * (1.0f - b) + 0.5f * b * (l + r);
        }
        std::copy (tmp, tmp + kN, amp);
    }
    // tilt, odd/even, partial count, shimmer
    const int count = juce::jlimit (1, kN, p.partials);
    for (int k = 0; k < kN; ++k)
    {
        const int h = k + 1;
        float a = amp[k] * std::pow ((float) h / 4.0f, p.tilt * 1.3f);
        if (h % 2 == 0) a *= 1.0f - juce::jmax (0.0f, p.oddEven);
        else if (h > 1) a *= 1.0f - juce::jmax (0.0f, -p.oddEven);
        if (h > count) a = 0.0f;
        if (p.shimmer > 0.0f)
        {
            if (rng.uni() < blockSecs * 12.0f) flickerTarget[k] = rng.uni();
            flicker[k] += (flickerTarget[k] - flicker[k]) * 0.2f;
            a *= 1.0f - p.shimmer * flicker[k];
        }
        amp[k] = a;
    }
    // keep the loudness of the frame after tilt / odd-even / count
    double e1 = 0.0;
    for (int k = 0; k < kN; ++k) e1 += (double) amp[k] * amp[k];
    const float comp = e1 > 1.0e-12 ? clampf ((float) std::sqrt (e0 / e1), 0.25f, 4.0f) : 1.0f;

    // temporal blur
    const float tc = p.blur * 0.96f;
    const float stretchExp = 1.0f + p.stretch * 0.25f;
    const float nyq = (float) sr * 0.5f;
    int newActive = 0;
    for (int k = 0; k < kN; ++k)
    {
        float a = amp[k] * comp;
        if (smoothAmp[k] < 0.0f) smoothAmp[k] = a;
        smoothAmp[k] = a + (smoothAmp[k] - a) * tc;
        a = smoothAmp[k];

        const float hz = freqHz * std::pow ((float) (k + 1), stretchExp);
        if (hz > nyq * 0.9f) a = 0.0f;
        else if (hz > nyq * 0.8f) a *= (nyq * 0.9f - hz) / (nyq * 0.1f);
        const float w = kTwoPi * juce::jmin (hz, nyq * 0.9f) / (float) sr;
        rc[k] = std::cos (w);
        rs[k] = std::sin (w);
        // re-normalise the rotator so it never drifts
        const float m = 1.5f - 0.5f * (cs[k] * cs[k] + sn[k] * sn[k]);
        cs[k] *= m; sn[k] *= m;

        // stereo: alternate partials left/right, fundamental in the centre
        const float side = k == 0 ? 0.0f : ((k % 2) ? -1.0f : 1.0f) * p.width * (0.4f + 0.6f * hashNoise (k, 5));
        const float gl = a * std::sqrt (0.5f * (1.0f - side)) * 1.4142f;
        const float gr = a * std::sqrt (0.5f * (1.0f + side)) * 1.4142f;
        dL[k] = (gl - aL[k]) / (float) blockLen;
        dR[k] = (gr - aR[k]) / (float) blockLen;
        if (a > 0.0f || aL[k] > 0.0f || aR[k] > 0.0f) newActive = k + 1;
    }
    active = newActive;
}
} // namespace pg
