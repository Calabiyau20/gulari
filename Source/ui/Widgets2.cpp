#include "Widgets2.h"
#include "../PluginProcessor.h"

namespace pg::ui
{
using namespace juce;

// =============================================================================
// MasterSection
// =============================================================================
MasterSection::MasterSection (SynthgularityProcessor& p, APVTS& state) : proc (p)
{
    knob = std::make_unique<ParamControl> (state, "master_gain", "MASTER");
    addAndMakeVisible (*knob);
    for (int i = 0; i < kSize; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (kTwoPi * (float) i / (float) (kSize - 1));
    bands.fill (-100.0f);
    setTooltip ("True peak (dBTP) and loudness (LUFS: momentary, short-term, integrated). Click the meters to reset.");
    startTimerHz (30);
}

void MasterSection::resized()
{
    auto r = getLocalBounds();
    meterArea = r.removeFromRight (150);
    knob->setBounds (r.removeFromRight (62).withTrimmedTop (-4));
    scopeArea = r.removeFromLeft (jmin (150, r.getWidth() / 3)).reduced (2, 4);
    r.removeFromLeft (4);
    specArea = r.reduced (2, 4);
}

void MasterSection::mouseDown (const MouseEvent& e)
{
    if (meterArea.contains (e.getPosition())) proc.meter.requestReset();
}

void MasterSection::timerCallback()
{
    auto& m = proc.meter;
    tpL = m.tpL.load(); tpR = m.tpR.load(); tpMax = m.tpMax.load();
    lm = m.lufsM.load(); ls = m.lufsS.load(); li = m.lufsI.load();

    // latest kSize samples from the ring
    const int wp = m.ringPos.load (std::memory_order_acquire);
    {
        // oscilloscope: find a rising zero crossing in the last 2048 samples, show ~20 ms from it
        const int span = (int) (m.sampleRate() * 0.02);
        int start = wp - span - 4;
        for (int k = wp - span - 4; k > wp - 2048; --k)
        {
            const float a = m.ring[(size_t) ((k - 1) & (MasterMeter::kRing - 1))], b = m.ring[(size_t) (k & (MasterMeter::kRing - 1))];
            if (a <= 0.0f && b > 0.0f) { start = k; break; }
        }
        traceHead = (traceHead + 3) % 4;
        auto& t = traces[(size_t) traceHead];
        for (int i = 0; i < kScope; ++i)
            t[(size_t) i] = m.ring[(size_t) ((start + i * span / kScope) & (MasterMeter::kRing - 1))];
    }
    for (int i = 0; i < kSize; ++i)
        fftData[(size_t) i] = m.ring[(size_t) ((wp - kSize + i) & (MasterMeter::kRing - 1))] * window[(size_t) i];
    std::fill (fftData.begin() + kSize, fftData.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (fftData.data());
    const double sr = m.sampleRate();
    for (int b = 0; b < kBands; ++b)
    {
        const double f0 = 20.0 * std::pow (1000.0, (double) b / kBands), f1 = 20.0 * std::pow (1000.0, (double) (b + 1) / kBands);
        const int i0 = jlimit (1, kSize / 2 - 1, (int) (f0 / sr * kSize)), i1 = jlimit (i0, kSize / 2 - 1, (int) (f1 / sr * kSize));
        float mx = 0.0f;
        for (int i = i0; i <= i1; ++i) mx = jmax (mx, fftData[(size_t) i]);
        const float db = 20.0f * std::log10 (jmax (1.0e-7f, mx / (float) kSize * 4.0f)) + 4.5f * std::log2 ((float) f0 / 1000.0f) * 0.5f;   // gentle +2.25 dB/oct tilt
        auto& v = bands[(size_t) b];
        v = db > v ? db : v + (db - v) * 0.25f;
    }
    repaint();
}

void MasterSection::paint (Graphics& g)
{
    {
        // oscilloscope with phosphor trails
        auto sr = scopeArea.toFloat();
        g.setColour (cosmos::voidC.withAlpha (0.85f));
        g.fillRoundedRectangle (sr, 5.0f);
        g.setColour (cosmos::nebula);
        g.drawRoundedRectangle (sr.reduced (0.5f), 5.0f, 1.0f);
        g.drawHorizontalLine ((int) sr.getCentreY(), sr.getX() + 3.0f, sr.getRight() - 3.0f);
        auto pr = sr.reduced (4.0f, 5.0f);
        for (int k = 3; k >= 0; --k)
        {
            const auto& t = traces[(size_t) ((traceHead + k) % 4)];
            Path p;
            for (int i = 0; i < kScope; ++i)
            {
                const float x = pr.getX() + (float) i / (float) (kScope - 1) * pr.getWidth();
                const float y = pr.getCentreY() - jlimit (-1.0f, 1.0f, t[(size_t) i]) * pr.getHeight() * 0.48f;
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            const float a = k == 0 ? 1.0f : 0.35f / (float) k;
            if (k == 0) { g.setColour (cosmos::neon.withAlpha (0.25f)); g.strokePath (p, PathStrokeType (4.0f)); }
            g.setColour ((k == 0 ? cosmos::neon : cosmos::teal).withAlpha (a));
            g.strokePath (p, PathStrokeType (k == 0 ? 1.4f : 1.0f));
        }
        g.setColour (colours::textDim);
        g.setFont (fonts::label (8.0f));
        g.drawText ("SCOPE", sr.reduced (5.0f, 2.0f), Justification::topLeft);
    }
    // spectrum
    auto r = specArea.toFloat();
    g.setColour (cosmos::voidC.withAlpha (0.85f));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (cosmos::nebula);
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
    auto plot = r.reduced (3.0f, 3.0f);
    g.setColour (cosmos::nebula.withAlpha (0.6f));
    for (float f : { 100.0f, 1000.0f, 10000.0f })
    {
        const float x = plot.getX() + std::log10 (f / 20.0f) / 3.0f * plot.getWidth();
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
    }
    const float lo = -84.0f, hi = 0.0f;
    Path p;
    p.startNewSubPath (plot.getX(), plot.getBottom());
    for (int b = 0; b < kBands; ++b)
    {
        const float x = plot.getX() + ((float) b + 0.5f) / (float) kBands * plot.getWidth();
        const float y = jmap (jlimit (lo, hi, bands[(size_t) b]), lo, hi, plot.getBottom(), plot.getY());
        p.lineTo (x, y);
    }
    p.lineTo (plot.getRight(), plot.getBottom());
    p.closeSubPath();
    g.setGradientFill (ColourGradient (cosmos::fuchsia.withAlpha (0.55f), 0.0f, plot.getY(), cosmos::teal.withAlpha (0.15f), 0.0f, plot.getBottom(), false));
    g.fillPath (p);
    g.setColour (cosmos::neon.withAlpha (0.9f));
    g.strokePath (p, PathStrokeType (1.2f));
    g.setColour (colours::textDim);
    g.setFont (fonts::label (8.0f));
    g.drawText ("FFT", plot.reduced (3.0f, 1.0f), Justification::topLeft);

    // meters: two true-peak bars + readouts
    auto m = meterArea.toFloat().reduced (2.0f, 4.0f);
    auto bars = m.removeFromLeft (18.0f);
    auto bar = [&] (Rectangle<float> b, float db)
    {
        g.setColour (cosmos::voidC);
        g.fillRoundedRectangle (b, 2.0f);
        const float norm = jlimit (0.0f, 1.0f, (db + 48.0f) / 51.0f);   // -48 .. +3 dBTP
        auto fill = b.withTrimmedTop (b.getHeight() * (1.0f - norm));
        g.setGradientFill (ColourGradient (cosmos::fuchsia, 0.0f, b.getY(), cosmos::neon, 0.0f, b.getBottom(), false));
        g.fillRoundedRectangle (fill, 2.0f);
        const float zero = b.getY() + b.getHeight() * (3.0f / 51.0f);
        g.setColour (cosmos::gold.withAlpha (0.8f));
        g.drawHorizontalLine ((int) zero, b.getX(), b.getRight());
    };
    bar (bars.removeFromLeft (8.0f), tpL);
    bars.removeFromLeft (2.0f);
    bar (bars.removeFromLeft (8.0f), tpR);
    m.removeFromLeft (6.0f);

    const float rowH = m.getHeight() / 4.0f;
    auto line = [&] (const String& label, float v, Colour c, const String& unit)
    {
        auto row = m.removeFromTop (rowH);
        g.setColour (colours::textDim);
        g.setFont (fonts::label (9.0f));
        g.drawText (label, row.removeFromLeft (18.0f), Justification::centredLeft);
        g.setColour (c);
        g.setFont (fonts::sans (12.0f, true));
        g.drawText (v <= -119.0f ? String ("-inf") : String (v, 1), row.removeFromLeft (46.0f), Justification::centredRight);
        g.setColour (colours::textDim);
        g.setFont (fonts::label (8.0f));
        g.drawText (unit, row.withTrimmedLeft (3.0f), Justification::centredLeft);
    };
    line ("TP", tpMax, tpMax > -0.1f ? Colours::red : (tpMax > -1.0f ? cosmos::gold : cosmos::starlight), "dBTP");
    line ("M", lm, cosmos::starlight, "LUFS");
    line ("S", ls, cosmos::starlight, "LUFS");
    line ("I", li, cosmos::neon, "LUFS");
}

// =============================================================================
// SpectralView
// =============================================================================
void SpectralView::filesDropped (const StringArray& files, int, int)
{
    if (! files.isEmpty() && onFile) onFile (File (files[0]));
}

void SpectralView::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (cosmos::voidC);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (cosmos::nebula);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    auto t = proc.getActiveSpectral();
    if (t == nullptr) return;
    const float* cache = proc.paramCache();
    const bool on = cache[pg::sp_on] > 0.5f;
    const int N = pg::SpectralTable::kPartials;

    auto inner = r.reduced (8.0f, 22.0f);
    auto heatArea = inner.removeFromLeft (inner.getWidth() * 0.62f);
    inner.removeFromLeft (10.0f);
    auto barArea = inner;

    // heat map image, rebuilt when the table changes
    if (cachedFor != t.get() || heat.isNull())
    {
        cachedFor = t.get();
        heat = Image (Image::ARGB, t->numFrames, N, true);
        float mx = 1.0e-6f;
        for (float a : t->amp) mx = jmax (mx, a);
        for (int f = 0; f < t->numFrames; ++f)
            for (int k = 0; k < N; ++k)
            {
                const float a = t->frame (f)[k] / mx;
                const float v = jlimit (0.0f, 1.0f, 1.0f + std::log10 (jmax (1.0e-4f, a)) / 3.0f);   // 60 dB range
                const Colour c = v < 0.5f ? cosmos::voidC.interpolatedWith (cosmos::violet, v * 2.0f)
                                          : cosmos::violet.interpolatedWith (cosmos::neon, (v - 0.5f) * 2.0f);
                heat.setPixelAt (f, N - 1 - k, c);
            }
    }
    {
        Graphics::ScopedSaveState ss (g);
        g.setImageResamplingQuality (Graphics::lowResamplingQuality);
        g.drawImage (heat, heatArea, RectanglePlacement::stretchToFit);
    }
    const float pos = cache[pg::sp_pos];
    const float px = heatArea.getX() + pos * heatArea.getWidth();
    g.setColour (cosmos::gold);
    g.drawVerticalLine ((int) px, heatArea.getY(), heatArea.getBottom());

    // the frame at the position, with tilt / odd-even / partial count applied
    const float fpos = pos * (float) (t->numFrames - 1);
    const int f0 = jlimit (0, t->numFrames - 1, (int) fpos), f1 = jmin (f0 + 1, t->numFrames - 1);
    const float ft = fpos - (float) f0;
    const int count = (int) cache[pg::sp_partials];
    const float tilt = cache[pg::sp_tilt], oe = cache[pg::sp_oddeven];
    float vals[64];
    float mx = 1.0e-6f;
    for (int k = 0; k < N; ++k)
    {
        const int h = k + 1;
        float a = t->frame (f0)[k] + (t->frame (f1)[k] - t->frame (f0)[k]) * ft;
        a *= std::pow ((float) h / 4.0f, tilt * 1.3f);
        if (h % 2 == 0) a *= 1.0f - jmax (0.0f, oe); else if (h > 1) a *= 1.0f - jmax (0.0f, -oe);
        if (h > count) a = 0.0f;
        vals[k] = a;
        mx = jmax (mx, a);
    }
    const float bw = barArea.getWidth() / (float) N;
    for (int k = 0; k < N; ++k)
    {
        const float v = jlimit (0.0f, 1.0f, 1.0f + std::log10 (jmax (1.0e-4f, vals[k] / mx)) / 3.0f);
        auto b = Rectangle<float> (barArea.getX() + (float) k * bw, barArea.getY(), bw, barArea.getHeight()).reduced (0.6f, 0.0f);
        g.setColour ((on ? (k % 2 ? cosmos::orchid : cosmos::neon) : cosmos::steel).withAlpha (k < count ? 1.0f : 0.25f));
        g.fillRect (b.withTrimmedTop (b.getHeight() * (1.0f - v)));
    }
    g.setColour (colours::textDim);
    g.setFont (fonts::sans (10.5f));
    String info = t->name;
    if (t->baseHz > 0.0f) info << "   (analysed, " << String (t->baseHz, 1) << " Hz)";
    g.drawText (info, r.reduced (8.0f, 4.0f).removeFromTop (14.0f), Justification::topLeft);
    g.setFont (fonts::label (8.5f));
    g.drawText ("TIME  >", heatArea.withY (heatArea.getBottom() + 3.0f).withHeight (12.0f), Justification::centredRight);
    g.drawText ("PARTIALS 1-64", barArea.withY (barArea.getBottom() + 3.0f).withHeight (12.0f), Justification::centredLeft);
}

// =============================================================================
// ShotView
// =============================================================================
void ShotView::filesDropped (const StringArray& files, int, int)
{
    if (! files.isEmpty() && onFile) onFile (File (files[0]));
}

void ShotView::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (cosmos::voidC);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (cosmos::nebula);
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
    auto s = proc.getActiveShot();
    if (s == nullptr || s->length < 2) return;
    const float* cache = proc.paramCache();
    const bool on = cache[pg::smp_on] > 0.5f;
    auto plot = r.reduced (4.0f, 4.0f);
    const int cols = jmax (1, (int) plot.getWidth());
    // the window covers the sample and its envelope (max 2 s)
    const float att = cache[pg::smp_attack], dec = cache[pg::smp_decay];
    const float secs = jlimit (0.05f, 2.0f, jmax ((float) s->length / (float) s->rate, att + dec));
    const int shown = (int) (secs * (float) s->rate);
    g.setColour ((on ? cosmos::orchid : cosmos::steel).withAlpha (0.8f));
    for (int c = 0; c < cols; ++c)
    {
        const int a = c * shown / cols, b = jmax (a + 1, (c + 1) * shown / cols);
        float mx = 0.0f;
        for (int i = a; i < b && i < s->length; ++i) mx = jmax (mx, std::abs (s->data[(size_t) i]));
        const float h = mx * plot.getHeight() * 0.48f;
        g.drawVerticalLine ((int) plot.getX() + c, plot.getCentreY() - h, plot.getCentreY() + h);
    }
    // attack / decay envelope
    Path env;
    for (int c = 0; c <= cols; c += 2)
    {
        const float t = (float) c / (float) cols * secs;
        const float e = t < att ? t / jmax (1.0e-4f, att) : std::exp (-6.9f * (t - att) / jmax (1.0e-3f, dec));
        const float y = plot.getBottom() - e * plot.getHeight() * 0.95f;
        if (c == 0) env.startNewSubPath (plot.getX(), y); else env.lineTo (plot.getX() + (float) c, y);
    }
    g.setColour (cosmos::gold);
    g.strokePath (env, PathStrokeType (1.6f));
    g.setColour (colours::textDim);
    g.setFont (fonts::sans (10.0f));
    g.drawText (s->name, r.reduced (6.0f, 3.0f), Justification::topLeft);
}

// =============================================================================
// FxSlotRow
// =============================================================================
String FxSlotRow::onParamFor (int type, bool mixbus)
{
    if (mixbus)
    {
        static const char* ids[] = { "", "mb_tp_on", "mb_eq_on", "mb_cp_on", "mb_ch_on", "mb_im_on", "mb_tg_on", "mb_dj_on", "mb_bc_on", "mb_tr_on", "mb_lm_on" };
        return type > 0 && type < MB_Count ? String (ids[type]) : String();
    }
    switch (type)
    {
        case FX_Filter1: return "ff1_on";
        case FX_Filter2: return "ff2_on";
        case FX_Stereo:  return "fx_st_on";
        case FX_Flanger: return "fx_fl_on";
        case FX_Phaser:  return "fx_ph_on";
        case FX_Delay:   return "fx_dl_on";
        case FX_Rings:   return "fx_rn_on";
        case FX_Clouds:  return "fx_gr_on";
        case FX_Reverb:  return "fx_rv_on";
        default: return {};
    }
}

FxSlotRow::FxSlotRow (APVTS& state, int s, bool mixbus) : st (state), slot (s), mb (mixbus)
{
    combo.addItemList (mb ? pg::choices::mixbusTypes() : pg::choices::fxTypes(), 1);
    addAndMakeVisible (combo);
    att = std::make_unique<APVTS::ComboBoxAttachment> (st, (mb ? "mb_slot" : "fx_slot") + String (slot + 1), combo);
    combo.onChange = [this] { if (onSelect) onSelect (slot); repaint(); };
    for (auto* b : { &up, &down }) { addAndMakeVisible (*b); b->setTooltip (b == &up ? "Move this effect earlier in the cascade" : "Move this effect later in the cascade"); }
    up.onClick = [this] { if (onMove) onMove (slot, -1); };
    down.onClick = [this] { if (onMove) onMove (slot, 1); };
}

int FxSlotRow::type() const { return combo.getSelectedItemIndex(); }

Rectangle<float> FxSlotRow::ledArea() const { return Rectangle<float> (26.0f, (float) getHeight() * 0.5f - 7.0f, 14.0f, 14.0f); }

void FxSlotRow::resized()
{
    auto r = getLocalBounds().reduced (2, 3);
    r.removeFromLeft (46);
    down.setBounds (r.removeFromRight (24));
    r.removeFromRight (2);
    up.setBounds (r.removeFromRight (24));
    r.removeFromRight (6);
    combo.setBounds (r);
}

void FxSlotRow::mouseDown (const MouseEvent& e)
{
    const auto id = onParamFor (type(), mb);
    if (ledArea().expanded (4.0f).contains (e.position) && id.isNotEmpty())
        if (auto* p = st.getParameter (id)) p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
    if (onSelect) onSelect (slot);
    repaint();
}

void FxSlotRow::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f, 1.5f);
    g.setColour (selected ? cosmos::nebula : cosmos::night.withAlpha (0.7f));
    g.fillRoundedRectangle (r, 4.0f);
    if (selected)
    {
        g.setColour (cosmos::neon);
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.2f);
    }
    g.setColour (colours::textDim);
    g.setFont (fonts::display (10.0f));
    g.drawText (String (slot + 1), r.withWidth (22.0f), Justification::centred);
    const auto id = onParamFor (type(), mb);
    bool on = false;
    if (id.isNotEmpty())
        if (auto* p = st.getRawParameterValue (id)) on = p->load() > 0.5f;
    auto led = ledArea();
    if (on)
    {
        // breathing glow around a running effect
        const float br = 0.5f + 0.5f * std::sin ((float) Time::getMillisecondCounter() * 0.004f + (float) slot);
        g.setColour (cosmos::neon.withAlpha (0.12f + 0.22f * br));
        g.fillEllipse (led.expanded (4.0f + 2.0f * br));
    }
    g.setColour (id.isEmpty() ? cosmos::night : (on ? cosmos::neon : cosmos::voidC));
    g.fillEllipse (led);
    g.setColour (on ? cosmos::starlight : cosmos::steel.withAlpha (0.6f));
    g.drawEllipse (led, 1.0f);
}
// =============================================================================
// AnalyzerStrip
// =============================================================================
AnalyzerStrip::AnalyzerStrip (ScopeFeed f) : feed (f)
{
    for (int i = 0; i < kSize; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (pg::kTwoPi * (float) i / (float) (kSize - 1));
    bands.fill (-100.0f);
    peaks.fill (-100.0f);
    setTooltip ("Oscilloscope, FFT spectrum, goniometer (stereo image, correlation) and true peak / LUFS. Click the meters to reset.");
    startTimerHz (30);
}

void AnalyzerStrip::resized()
{
    auto r = getLocalBounds().reduced (6, 4);
    meterA = r.removeFromRight (jmin (190, r.getWidth() / 6));
    r.removeFromRight (6);
    gonioA = r.removeFromRight (jmin (r.getHeight() + 10, r.getWidth() / 3));
    r.removeFromRight (6);
    scopeA = r.removeFromLeft (r.getWidth() * 2 / 5);
    r.removeFromLeft (6);
    fftA = r;
}

void AnalyzerStrip::mouseDown (const MouseEvent& e)
{
    if (meterA.contains (e.getPosition()) && feed.meter != nullptr) feed.meter->requestReset();
}

void AnalyzerStrip::timerCallback()
{
    if (feed.meter == nullptr) return;
    auto& m = *feed.meter;
    tpL = m.tpL.load(); tpR = m.tpR.load(); tpMax = m.tpMax.load();
    lm = m.lufsM.load(); ls = m.lufsS.load(); li = m.lufsI.load();
    const int N = feed.size, mask = N - 1;
    const int wp = feed.pos->load (std::memory_order_acquire);
    auto mono = [&] (int k) { return 0.5f * (feed.L[k & mask] + feed.R[k & mask]); };
    // scope: rising zero crossing, ~25 ms
    {
        const int span = (int) (m.sampleRate() * 0.025);
        int start = wp - span - 4;
        for (int k = wp - span - 4; k > wp - 4000; --k)
            if (mono (k - 1) <= 0.0f && mono (k) > 0.0f) { start = k; break; }
        traceHead = (traceHead + 2) % 3;
        auto& t = traces[(size_t) traceHead];
        for (int i = 0; i < kScope; ++i) t[(size_t) i] = mono (start + i * span / kScope);
    }
    // FFT
    for (int i = 0; i < kSize; ++i) fftData[(size_t) i] = mono (wp - kSize + i) * window[(size_t) i];
    std::fill (fftData.begin() + kSize, fftData.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (fftData.data());
    const float sr = (float) m.sampleRate();
    for (int b = 0; b < kBands; ++b)
    {
        const float f0 = 20.0f * std::pow (1000.0f, (float) b / kBands), f1 = 20.0f * std::pow (1000.0f, (float) (b + 1) / kBands);
        const int i0 = jlimit (1, kSize / 2 - 1, (int) (f0 / sr * kSize)), i1 = jlimit (i0, kSize / 2 - 1, (int) (f1 / sr * kSize));
        float mx = 0.0f;
        for (int i = i0; i <= i1; ++i) mx = jmax (mx, fftData[(size_t) i]);
        const float db = Decibels::gainToDecibels (mx * 2.0f / (float) kSize * 2.0f, -100.0f) + 4.5f * std::log2 (f0 / 1000.0f) * 0.5f;
        bands[(size_t) b] = jmax (db, bands[(size_t) b] - 2.5f);
        peaks[(size_t) b] = jmax (db, peaks[(size_t) b] - 0.35f);
    }
    // goniometer: last 1024 samples, correlation
    double sLR = 0, sLL = 0, sRR = 0;
    for (int i = 0; i < kGonio; ++i)
    {
        const int k = (wp - kGonio + i) & mask;
        gl[(size_t) i] = feed.L[k];
        gr[(size_t) i] = feed.R[k];
        sLR += gl[(size_t) i] * gr[(size_t) i]; sLL += gl[(size_t) i] * gl[(size_t) i]; sRR += gr[(size_t) i] * gr[(size_t) i];
    }
    const double den = std::sqrt (sLL * sRR);
    const float c = den > 1e-9 ? (float) (sLR / den) : 0.0f;
    correlation += (c - correlation) * 0.2f;
    repaint();
}

static void panelBg (Graphics& g, Rectangle<int> a, const String& title)
{
    g.setColour (cosmos::voidC.withAlpha (0.85f));
    g.fillRoundedRectangle (a.toFloat(), 7.0f);
    g.setColour (cosmos::nebula);
    g.drawRoundedRectangle (a.toFloat().reduced (0.5f), 7.0f, 1.0f);
    g.setColour (colours::textDim);
    g.setFont (fonts::label (8.5f));
    g.drawText (title, a.reduced (8, 4), Justification::topLeft);
}

void AnalyzerStrip::paint (Graphics& g)
{
    // ---- scope with phosphor persistence ----
    panelBg (g, scopeA, "SCOPE");
    {
        auto r = scopeA.toFloat().reduced (8.0f, 14.0f);
        g.setColour (colours::outline.withAlpha (0.5f));
        g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());
        for (int age = 2; age >= 0; --age)
        {
            const auto& t = traces[(size_t) ((traceHead + age) % 3)];
            Path p;
            for (int i = 0; i < kScope; ++i)
            {
                const float x = r.getX() + r.getWidth() * (float) i / (kScope - 1), y = r.getCentreY() - jlimit (-1.2f, 1.2f, t[(size_t) i]) * r.getHeight() * 0.45f;
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (cosmos::neon.withAlpha (age == 0 ? 0.95f : 0.25f / (float) age));
            g.strokePath (p, PathStrokeType (age == 0 ? 1.6f : 1.0f));
        }
    }
    // ---- spectrum ----
    panelBg (g, fftA, "SPECTRUM");
    {
        auto r = fftA.toFloat().reduced (8.0f, 14.0f);
        g.setFont (fonts::sans (8.5f));
        for (float f : { 100.0f, 1000.0f, 10000.0f })
        {
            const float x = r.getX() + r.getWidth() * std::log (f / 20.0f) / std::log (1000.0f);
            g.setColour (colours::outline.withAlpha (0.45f));
            g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
            g.setColour (colours::textDim);
            g.drawText (f >= 1000.0f ? String ((int) (f / 1000.0f)) + "k" : String ((int) f), (int) x + 2, (int) r.getBottom() - 10, 30, 10, Justification::left);
        }
        auto yOf = [&] (float db) { return r.getBottom() - jlimit (0.0f, 1.0f, (db + 90.0f) / 90.0f) * r.getHeight(); };
        Path fill, line, pk;
        fill.startNewSubPath (r.getX(), r.getBottom());
        for (int b = 0; b < kBands; ++b)
        {
            const float x = r.getX() + r.getWidth() * (float) b / (kBands - 1);
            fill.lineTo (x, yOf (bands[(size_t) b]));
            if (b == 0) { line.startNewSubPath (x, yOf (bands[0])); pk.startNewSubPath (x, yOf (peaks[0])); }
            else { line.lineTo (x, yOf (bands[(size_t) b])); pk.lineTo (x, yOf (peaks[(size_t) b])); }
        }
        fill.lineTo (r.getRight(), r.getBottom());
        fill.closeSubPath();
        g.setGradientFill (ColourGradient (cosmos::fuchsia.withAlpha (0.55f), r.getX(), r.getY(), cosmos::electric.withAlpha (0.1f), r.getX(), r.getBottom(), false));
        g.fillPath (fill);
        g.setColour (cosmos::orchid);
        g.strokePath (line, PathStrokeType (1.3f));
        g.setColour (cosmos::gold.withAlpha (0.5f));
        g.strokePath (pk, PathStrokeType (0.8f));
    }
    // ---- goniometer (M/S rotated 45 degrees) ----
    panelBg (g, gonioA, "GONIOMETER");
    {
        auto r = gonioA.toFloat().reduced (8.0f, 14.0f);
        auto sq = r.withSizeKeepingCentre (jmin (r.getWidth(), r.getHeight() - 12.0f), jmin (r.getWidth(), r.getHeight() - 12.0f)).translated (0.0f, -6.0f);
        const auto c = sq.getCentre();
        const float h = sq.getWidth() * 0.5f;
        g.setColour (colours::outline.withAlpha (0.5f));
        g.drawLine (c.x, sq.getY(), c.x, sq.getBottom(), 0.8f);
        g.drawLine (sq.getX(), c.y, sq.getRight(), c.y, 0.8f);
        g.drawLine (c.x - h * 0.7f, c.y - h * 0.7f, c.x + h * 0.7f, c.y + h * 0.7f, 0.6f);
        g.drawLine (c.x + h * 0.7f, c.y - h * 0.7f, c.x - h * 0.7f, c.y + h * 0.7f, 0.6f);
        g.setFont (fonts::sans (8.0f));
        g.setColour (colours::textDim);
        g.drawText ("L", (int) (c.x - h * 0.75f) - 8, (int) (c.y - h * 0.75f) - 8, 10, 10, Justification::centred);
        g.drawText ("R", (int) (c.x + h * 0.75f), (int) (c.y - h * 0.75f) - 8, 10, 10, Justification::centred);
        for (int i = 0; i < kGonio; ++i)
        {
            const float L = gl[(size_t) i], R = gr[(size_t) i];
            const float x = (R - L) * 0.7071f, y = (L + R) * 0.7071f;
            const float px = c.x + jlimit (-1.0f, 1.0f, x) * h, py = c.y - jlimit (-1.0f, 1.0f, y) * h;
            g.setColour (cosmos::neon.withAlpha (0.08f + 0.5f * (float) i / kGonio));
            g.fillRect (px, py, 1.5f, 1.5f);
        }
        // correlation bar
        auto bar = Rectangle<float> (r.getX() + 6.0f, r.getBottom() - 6.0f, r.getWidth() - 12.0f, 4.0f);
        g.setColour (colours::outline);
        g.fillRoundedRectangle (bar, 2.0f);
        const float cx = bar.getCentreX() + correlation * bar.getWidth() * 0.5f;
        g.setColour (correlation < 0.0f ? Colour (0xffff5c5c) : cosmos::neon);
        g.fillEllipse (Rectangle<float> (7.0f, 7.0f).withCentre ({ cx, bar.getCentreY() }));
    }
    // ---- true peak + LUFS ----
    panelBg (g, meterA, "OUTPUT");
    {
        auto r = meterA.toFloat().reduced (8.0f, 16.0f);
        auto bars = r.removeFromLeft (34.0f);
        auto yOf = [&] (float db) { return bars.getBottom() - jlimit (0.0f, 1.0f, (db + 60.0f) / 63.0f) * bars.getHeight(); };
        for (int ch = 0; ch < 2; ++ch)
        {
            auto b = bars.withWidth (12.0f).translated ((float) ch * 16.0f, 0.0f);
            g.setColour (colours::outline);
            g.fillRect (b);
            const float v = ch == 0 ? tpL : tpR;
            const float top = yOf (v);
            g.setGradientFill (ColourGradient (Colour (0xffff5c5c), 0, yOf (0.0f), cosmos::neon, 0, yOf (-24.0f), false));
            g.fillRect (b.withTop (top));
        }
        g.setColour (cosmos::gold);
        g.drawHorizontalLine ((int) yOf (-1.0f), bars.getX(), bars.getRight());
        r.removeFromLeft (8.0f);
        g.setFont (fonts::sans (10.5f));
        auto line = [&] (const String& k, float v, Colour c)
        {
            auto row = r.removeFromTop (r.getHeight() / 4.0f);
            g.setColour (colours::textDim);
            g.drawText (k, row.removeFromLeft (30.0f), Justification::centredLeft);
            g.setColour (c);
            g.drawText (v <= -99.0f ? String ("-inf") : String (v, 1), row, Justification::centredRight);
        };
        line ("TP", tpMax, tpMax > -1.0f ? Colour (0xffff5c5c) : colours::text);
        line ("M", lm, colours::text);
        line ("S", ls, colours::text);
        line ("INT", li, cosmos::neon);
    }
}

// =============================================================================
// Goniometer (small, for headers)
// =============================================================================
GonioMini::GonioMini (ScopeFeed f) : feed (f)
{
    setTooltip ("Goniometer: vertical = mono, horizontal = stereo side. Red dot on the bar = phase problems.");
}

void GonioMini::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (cosmos::voidC.withAlpha (0.85f));
    g.fillRoundedRectangle (r, 5.0f);
    if (feed.L == nullptr) return;
    auto sq = r.reduced (4.0f).withTrimmedBottom (6.0f);
    sq = sq.withSizeKeepingCentre (jmin (sq.getWidth(), sq.getHeight()), jmin (sq.getWidth(), sq.getHeight()));
    const auto c = sq.getCentre();
    const float h = sq.getWidth() * 0.5f;
    g.setColour (colours::outline.withAlpha (0.5f));
    g.drawLine (c.x, sq.getY(), c.x, sq.getBottom(), 0.6f);
    g.drawLine (sq.getX(), c.y, sq.getRight(), c.y, 0.6f);
    const int N = feed.size, mask = N - 1, wp = feed.pos->load (std::memory_order_acquire);
    double sLR = 0, sLL = 0, sRR = 0;
    for (int i = 0; i < 512; ++i)
    {
        const int k = (wp - 512 + i) & mask;
        const float L = feed.L[k], R = feed.R[k];
        sLR += L * R; sLL += L * L; sRR += R * R;
        const float px = c.x + jlimit (-1.0f, 1.0f, (R - L) * 0.7071f) * h, py = c.y - jlimit (-1.0f, 1.0f, (L + R) * 0.7071f) * h;
        g.setColour (cosmos::neon.withAlpha (0.1f + 0.6f * (float) i / 512.0f));
        g.fillRect (px, py, 1.4f, 1.4f);
    }
    const double den = std::sqrt (sLL * sRR);
    corr += ((den > 1e-9 ? (float) (sLR / den) : 0.0f) - corr) * 0.2f;
    auto bar = Rectangle<float> (r.getX() + 5.0f, r.getBottom() - 6.0f, r.getWidth() - 10.0f, 3.0f);
    g.setColour (colours::outline);
    g.fillRoundedRectangle (bar, 1.5f);
    g.setColour (corr < 0.0f ? Colour (0xffff5c5c) : cosmos::neon);
    g.fillEllipse (Rectangle<float> (6.0f, 6.0f).withCentre ({ bar.getCentreX() + corr * bar.getWidth() * 0.5f, bar.getCentreY() }));
}

// =============================================================================
// SourceChip: drag a modulation source onto any knob
// =============================================================================
SourceChip::SourceChip (int s, const String& n, Colour c, std::function<float()> v, bool bip)
    : src (s), name (n), col (c), value (std::move (v)), bipolar (bip)
{
    setTooltip ("Drag " + name + " onto any knob to modulate it");
    setMouseCursor (MouseCursor::DraggingHandCursor);
}

void SourceChip::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.5f, 3.0f);
    const float v = value ? value() : 0.0f;
    const float act = jlimit (0.0f, 1.0f, std::abs (v));
    g.setColour (cosmos::night.withAlpha (0.92f));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (col.withAlpha (0.16f + 0.3f * act));
    auto fill = r.reduced (1.5f);
    g.fillRoundedRectangle (bipolar ? fill.withWidth (fill.getWidth() * (0.5f + 0.5f * jlimit (-1.0f, 1.0f, v))) : fill.withWidth (fill.getWidth() * act), 4.0f);
    g.setColour (col.withAlpha (isMouseOver() ? 1.0f : 0.7f));
    g.drawRoundedRectangle (r, 5.0f, isMouseOver() ? 1.6f : 1.0f);
    g.setColour (cosmos::starlight);
    g.setFont (fonts::label (8.5f));
    g.drawFittedText (name, r.reduced (2.0f, 0.0f).toNearestInt(), Justification::centred, 1, 0.6f);
}

void SourceChip::mouseDrag (const MouseEvent& e)
{
    if (e.getDistanceFromDragStart() < 4) return;
    if (auto* dc = DragAndDropContainer::findParentDragContainerFor (this))
        if (! dc->isDragAndDropActive())
        {
            Image img (Image::ARGB, getWidth(), getHeight(), true);
            Graphics g (img);
            paint (g);
            dc->startDragging ("mod:" + String (src), this, ScaledImage (img), true);
        }
}
} // namespace pg::ui
