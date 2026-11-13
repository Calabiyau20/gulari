#include "WcParams.h"

namespace wc
{
using APF = juce::AudioParameterFloat;
using APC = juce::AudioParameterChoice;
using APB = juce::AudioParameterBool;
using NR = juce::NormalisableRange<float>;
using FA = juce::AudioParameterFloatAttributes;

// =================================================================================================
// Reading a typed value back (valueFromStringFunction)
// =================================================================================================
// Each float parameter says which unit its text is in and the continuous number behind that text, so a typed
// "440 Hz" lands on 440 Hz and not on the edge of a rounded display. The text functions stay as they were; the
// round trip in WcRenderTest is what keeps the two in step.
enum class Unit { None, Hz, Sec, Pct, Volt, dB, St, Oct, Times, Div, Bpm, Label };

struct Readback
{
    Unit unit = Unit::None;
    std::function<double (float)> toUnit;   // plain value -> the number the text shows before rounding (Hz, s, %, V...); empty = itself
    float lo = 0.0f, hi = 0.0f;             // plain values that number covers, when it is not the whole range
    const char* word = nullptr;             // the rest of the range is shown as this word ("off")
    float wordValue = 0.0f;
};

// a text function and its readback, made together
struct Fmt { FA attr; Readback rb; };

namespace
{
struct Entry
{
    Readback rb;
    NR range;
    float def = 0.0f;
    std::function<juce::String (float, int)> text;
    juce::String bare;                      // unit token of the text at the default value
};

// spaces, capitals and the typographic minus do not matter; "(behind)", "(x4 long)" are comments
juce::String normalised (const juce::String& s)
{
    auto t = s.toLowerCase().replace (juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")), "-")
                            .replace (juce::String (juce::CharPointer_UTF8 ("\xc2\xb5")), "u")
                            .removeCharacters (juce::String (" \t") + juce::String (juce::CharPointer_UTF8 ("\xc2\xa0")));
    if (t.containsChar ('(')) t = t.upToFirstOccurrenceOf ("(", false, false);
    return t;
}

struct Token { double number = 0.0; juce::String unit; bool kilo = false; };

// "+1,5 kHz" -> 1.5, "khz";  "x2" -> 2, "x";  "1.2k" -> 1.2, "", kilo
std::optional<Token> tokenize (const juce::String& text)
{
    auto t = normalised (text);
    Token tk;
    if (t.startsWithChar ('x')) { tk.unit = "x"; t = t.substring (1); }
    int i = 0;
    bool neg = false;
    if (t[0] == '+' || t[0] == '-') { neg = t[0] == '-'; ++i; }
    juce::String num;
    bool point = false, digits = false;
    for (; i < t.length(); ++i)
    {
        const auto c = t[i];
        if (c >= '0' && c <= '9') { num << juce::String::charToString (c); digits = true; }
        else if ((c == '.' || c == ',') && ! point) { num << "."; point = true; }   // a comma is a decimal point, never a thousands separator
        else break;
    }
    if (! digits) return {};
    tk.number = (neg ? -1.0 : 1.0) * num.getDoubleValue();
    auto rest = t.substring (i);
    if (rest.startsWithChar ('k') && rest != "khz") { tk.kilo = true; rest = rest.substring (1); }
    if (tk.unit == "x") { if (rest.isNotEmpty()) return {}; }
    else tk.unit = rest;
    return tk;
}

// unit token -> kind and factor to the kind's base (Hz, s)
bool unitOf (const juce::String& u, Unit& kind, double& factor)
{
    struct U { const char* tok; Unit kind; double f; };
    static const U table[] = { { "", Unit::None, 1.0 }, { "hz", Unit::Hz, 1.0 }, { "khz", Unit::Hz, 1000.0 },
                               { "s", Unit::Sec, 1.0 }, { "ms", Unit::Sec, 1.0e-3 }, { "us", Unit::Sec, 1.0e-6 },
                               { "%", Unit::Pct, 1.0 }, { "v", Unit::Volt, 1.0 }, { "db", Unit::dB, 1.0 },
                               { "st", Unit::St, 1.0 }, { "oct", Unit::Oct, 1.0 }, { "x", Unit::Times, 1.0 },
                               { "/div", Unit::Div, 1.0 }, { "bpm", Unit::Bpm, 1.0 } };
    for (auto& e : table)
        if (u == e.tok) { kind = e.kind; factor = e.f; return true; }
    return false;
}

juce::String unitTokenOf (const juce::String& shown)
{
    auto tk = tokenize (shown);
    return tk ? tk->unit : juce::String();
}

double toUnitOf (const Entry& e, float plain) { return e.rb.toUnit ? e.rb.toUnit (plain) : (double) plain; }

// a label, by its full text or an unambiguous start of it; or by its position 1..N when no label is a number
std::optional<int> pickLabel (const juce::StringArray& labels, const juce::String& text)
{
    const auto t = normalised (text);
    if (t.isEmpty()) return {};
    for (int i = 0; i < labels.size(); ++i) if (normalised (labels[i]) == t) return i;
    int found = -1;
    for (int i = 0; i < labels.size(); ++i)
        if (normalised (labels[i]).startsWith (t)) { if (found >= 0) { found = -2; break; } found = i; }
    if (found >= 0) return found;
    bool numeric = false;
    for (auto& l : labels) if (tokenize (l).has_value()) numeric = true;
    if (! numeric && t.containsOnly ("0123456789"))
    {
        const int k = t.getIntValue();
        if (k >= 1 && k <= labels.size()) return k - 1;
    }
    return {};
}

std::optional<TypedValue> parseFloat (const Entry& e, const juce::String& text, const juce::String& shown)
{
    const auto& r = e.range;
    const auto t = normalised (text);
    if (t.isEmpty()) return {};
    if (e.rb.word != nullptr && t == normalised (e.rb.word)) return TypedValue { e.rb.wordValue, 0 };

    if (e.rb.unit == Unit::Label)
    {
        juce::StringArray labels;
        for (float v = r.start; v <= r.end + 0.5f * r.interval; v += r.interval) labels.add (e.text (v, 64));
        if (auto k = pickLabel (labels, text)) return TypedValue { r.start + r.interval * (float) *k, 0 };
        if (! t.endsWithChar ('%')) return {};
    }

    auto tk = tokenize (text);
    if (! tk) return {};
    juce::String unit = tk->unit;
    if (unit.isEmpty())
    {
        // a bare number is in the unit the control showed; "1.2k" on a frequency is always 1200 Hz
        auto shownUnit = unitTokenOf (shown);
        Unit k; double f;
        if (shown.isEmpty() || ! unitOf (shownUnit, k, f) || k != e.rb.unit) shownUnit = e.bare;
        unit = (tk->kilo && e.rb.unit == Unit::Hz) ? juce::String ("hz") : shownUnit;
    }
    Unit kind;
    double factor;
    if (! unitOf (unit, kind, factor)) return {};
    const double value = tk->number * factor * (tk->kilo ? 1000.0 : 1.0);

    // a value typed as the rounded text of an end ("3.97 kHz" for 3968 Hz) is that end, not a limited value
    auto limit = [] (double x, double lo, double hi, int& side)
    {
        const double span = std::abs (hi - lo) * 1.0e-9;
        side = x < lo - (std::abs (lo) * 2.0e-3 + span) ? -1 : (x > hi + (std::abs (hi) * 2.0e-3 + span) ? 1 : 0);
        return juce::jlimit (lo, hi, x);
    };

    // "%" on a control that is not shown in % is a position of the knob
    if (kind == Unit::Pct && e.rb.unit != Unit::Pct)
    {
        TypedValue out;
        const double n = limit (value / 100.0, 0.0, 1.0, out.limited);
        out.plain = r.convertFrom0to1 ((float) n);
        return out;
    }
    if (kind != e.rb.unit) return {};   // a unit of another kind; labels only take a name or a %

    const float lo = e.rb.hi > e.rb.lo ? e.rb.lo : r.start, hi = e.rb.hi > e.rb.lo ? e.rb.hi : r.end;
    const double ulo = toUnitOf (e, lo), uhi = toUnitOf (e, hi);
    TypedValue out;
    const double target = limit (value, ulo, uhi, out.limited);
    double a = lo, b = hi;   // every readback grows with the plain value
    for (int it = 0; it < 64; ++it)
    {
        const double m = 0.5 * (a + b);
        if (toUnitOf (e, (float) m) < target) a = m; else b = m;
    }
    double plain = 0.5 * (a + b);
    if (r.interval > 0.0f) plain = r.snapToLegalValue ((float) plain);
    out.plain = juce::jlimit (r.start, r.end, (float) plain);
    return out;
}

// the readbacks of every float parameter, by id: built once, from a layout made only for that
thread_local std::map<juce::String, Entry>* collecting = nullptr;

const std::map<juce::String, Entry>& registry()
{
    static const std::map<juce::String, Entry> reg = []
    {
        std::map<juce::String, Entry> m;
        collecting = &m;
        juce::ignoreUnused (createLayout());
        collecting = nullptr;
        return m;
    }();
    return reg;
}
} // namespace

// Floats: texts that were missing (raw "0.5000000") become % for 0..1 and -1..1, and every float reads typed values back.
static std::unique_ptr<APF> flt (const juce::ParameterID& id, const juce::String& name, NR range, float def, Fmt f = {})
{
    auto& rb = f.rb;
    auto attr = f.attr;
    const bool stepped = range.interval > 0.0f;
    if (attr.getStringFromValueFunction() == nullptr && rb.unit == Unit::None && ! stepped)
    {
        const bool bipolar = juce::approximatelyEqual (range.start, -1.0f) && juce::approximatelyEqual (range.end, 1.0f);
        if (bipolar || (juce::approximatelyEqual (range.start, 0.0f) && juce::approximatelyEqual (range.end, 1.0f)))
        {
            rb = { Unit::Pct, [] (float v) { return 100.0 * v; } };
            attr = attr.withStringFromValueFunction ([bipolar] (float v, int)
            {
                const int n = juce::roundToInt (v * 100.0f);
                return (bipolar && n > 0 ? "+" : "") + juce::String (n) + " %";
            });
        }
    }
    Entry e { rb, range, def, attr.getStringFromValueFunction(), {} };
    if (e.text == nullptr)   // the stepped values keep JUCE's own text, a whole number
        e.text = [] (float v, int) { return juce::String (juce::roundToInt (v)); };
    e.bare = unitTokenOf (e.text (def, 64));
    attr = attr.withValueFromStringFunction ([e] (const juce::String& t)
    {
        // a text that is no value for this parameter leaves it at its default
        auto v = parseFloat (e, t, {});
        return v ? v->plain : e.def;
    });
    if (collecting != nullptr) (*collecting)[id.getParamID()] = e;
    return std::make_unique<APF> (id, name, range, def, attr);
}

// Choices read back their label (or an unambiguous start of it) or their position 1..N.
static std::unique_ptr<APC> choice (const juce::ParameterID& id, const juce::String& name, const juce::StringArray& items, int def)
{
    return std::make_unique<APC> (id, name, items, def, juce::AudioParameterChoiceAttributes().withValueFromStringFunction ([items, def] (const juce::String& t)
    {
        auto k = pickLabel (items, t);
        return k ? *k : def;
    }));
}

std::optional<TypedValue> parseValueText (const juce::RangedAudioParameter& p, const juce::String& text, const juce::String& shown)
{
    if (auto* c = dynamic_cast<const APC*> (&p))
    {
        if (auto k = pickLabel (c->choices, text)) return TypedValue { (float) *k, 0 };
        return {};
    }
    if (dynamic_cast<const APB*> (&p) != nullptr)
    {
        const auto t = normalised (text);
        if (t == "on" || t == "1" || t == "yes" || t == "true") return TypedValue { 1.0f, 0 };
        if (t == "off" || t == "0" || t == "no" || t == "false") return TypedValue { 0.0f, 0 };
        return {};
    }
    const auto& reg = registry();
    auto it = reg.find (p.getParameterID());
    if (it == reg.end()) return {};
    return parseFloat (it->second, text, shown);
}

juce::String valueTextExample (const juce::RangedAudioParameter& p)
{
    auto labelsHint = [] (const juce::StringArray& l) { return l.size() > 1 ? l[0] + " or " + l[1] : l[0]; };
    if (auto* c = dynamic_cast<const APC*> (&p)) return labelsHint (c->choices);
    if (dynamic_cast<const APB*> (&p) != nullptr) return "on or off";
    const auto& reg = registry();
    auto it = reg.find (p.getParameterID());
    if (it == reg.end()) return "a number";
    const auto& e = it->second;
    switch (e.rb.unit)
    {
        case Unit::Hz:    return "440 Hz or 1.2k";
        case Unit::Sec:   return "250 ms or 1.5 s";
        case Unit::Pct:   return e.range.start < 0.0f ? "+50 % or -25 %" : "50 %";
        case Unit::Volt:  return "2.5 V";
        case Unit::dB:    return "-6 dB";
        case Unit::St:    return "1.5 st";
        case Unit::Oct:   return "1 oct";
        case Unit::Times: return "x2 or 0.5";
        case Unit::Div:   return "0.5 /div";
        case Unit::Bpm:   return "120 BPM";
        case Unit::Label:
        {
            juce::StringArray l;
            for (float v = e.range.start; v <= e.range.end + 0.5f * e.range.interval && l.size() < 2; v += e.range.interval) l.add (e.text (v, 64));
            return labelsHint (l);
        }
        case Unit::None:
        default:          return juce::String (juce::roundToInt (toUnitOf (e, e.range.convertFrom0to1 (0.5f))));
    }
}

static Fmt hzAttr (bool canBeLow)
{
    return { FA().withStringFromValueFunction ([canBeLow] (float v, int)
    {
        juce::ignoreUnused (canBeLow);
        const float hz = coarseHz (v, false);
        return hz < 1000.0f ? juce::String (hz, hz < 100.0f ? 1 : 0) + " Hz" : juce::String (hz / 1000.0f, 2) + " kHz";
    }), { Unit::Hz, [] (float v) { return 27.5 * std::exp2 (8.0 * v); } } };
}
static Fmt unitAttr (const char* unit, int dec = 2)
{
    Unit k; double f;
    unitOf (normalised (unit), k, f);
    return { FA().withStringFromValueFunction ([unit, dec] (float v, int) { return juce::String (v, dec) + unit; }),
             { k, [f] (float v) { return v * f; } } };
}
// readbacks of the usual shapes: a * b^v and a + b * v
static Readback expRb (Unit u, double a, double b) { return { u, [a, b] (float v) { return a * std::pow (b, (double) v); } }; }
static Readback linRb (Unit u, double a, double b) { return { u, [a, b] (float v) { return a + b * (double) v; } }; }

static void add259 (juce::AudioProcessorValueTreeState::ParameterLayout& l, int i)
{
    const juce::String p (prefix259 (i)), n ("259 " + juce::String::charToString ((juce::juce_wchar) ('A' + i)) + " ");
    auto id = [&] (const char* s) { return juce::ParameterID (p + s, 1); };
    auto group = std::make_unique<juce::AudioProcessorParameterGroup> (p + "grp", n.trim(), "|");
    // modulation oscillator
    group->addChild (flt (id ("mo_freq"), n + "MO Frequency", NR (0.0f, 1.0f), coarseNorm (130.81f, false), hzAttr (true)));
    group->addChild (flt (id ("mo_fine"), n + "MO Fine", NR (-3.5f, 3.5f), 0.0f, unitAttr (" st")));
    group->addChild (choice (id ("mo_range"), n + "MO Range", juce::StringArray { "High", "Low" }, 0));
    group->addChild (choice (id ("mo_wave"), n + "MO Wave", juce::StringArray { "Sawtooth", "Square", "Triangle" }, 2));
    group->addChild (flt (id ("mo_cv"), n + "MO CV Amount", NR (-1.0f, 1.0f), 0.0f));
    group->addChild (flt (id ("mo_fm"), n + "MO FM Amount", NR (0.0f, 1.0f), 0.0f));
    // modulation section
    group->addChild (flt (id ("idx"), n + "Mod Index", NR (0.0f, 1.0f), 0.0f));
    group->addChild (flt (id ("idx_cv"), n + "Index CV Amount", NR (0.0f, 1.0f), 0.5f));
    group->addChild (std::make_unique<APB> (id ("mod_amp"), n + "Mod Amplitude", false));
    group->addChild (std::make_unique<APB> (id ("mod_pitch"), n + "Mod Pitch", false));
    group->addChild (std::make_unique<APB> (id ("mod_timbre"), n + "Mod Timbre", false));
    group->addChild (choice (id ("prog"), n + "Program", juce::StringArray { "Local", "Remote", "Both" }, 0));
    group->addChild (flt (id ("lock"), n + "Phase Lock Gain", NR (0.0f, 1.0f), 0.7f));
    group->addChild (std::make_unique<APB> (id ("lock_on"), n + "Phase Lock", false));
    group->addChild (std::make_unique<APB> (id ("volt1"), n + "1V/Oct", false));
    // principal oscillator
    group->addChild (flt (id ("po_freq"), n + "Principal Frequency", NR (0.0f, 1.0f), coarseNorm (65.406f, false), hzAttr (false)));
    group->addChild (flt (id ("po_fine"), n + "Principal Fine", NR (-3.5f, 3.5f), 0.0f, unitAttr (" st")));
    group->addChild (flt (id ("po_cv"), n + "Principal CV Amount", NR (-1.0f, 1.0f), 0.0f));
    group->addChild (flt (id ("po_fm"), n + "Principal FM Amount", NR (0.0f, 1.0f), 0.0f));
    // waveshape
    group->addChild (flt (id ("timbre"), n + "Timbre", NR (0.0f, 1.0f), 0.0f));
    group->addChild (flt (id ("timbre_cv"), n + "Timbre CV Amount", NR (-1.0f, 1.0f), 0.5f));
    group->addChild (flt (id ("sym"), n + "Symmetry", NR (0.0f, 1.0f), 0.5f));
    group->addChild (flt (id ("order"), n + "Order", NR (0.0f, 1.0f), 0.0f));
    l.add (std::move (group));
}

static void add281 (juce::AudioProcessorValueTreeState::ParameterLayout& l, int i)
{
    const juce::String p (prefix281 (i)), n ("281 " + juce::String::charToString ((juce::juce_wchar) ('A' + i)) + " ");
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> (p + "grp", n.trim(), "|");
    const Fmt timeAttr { FA().withStringFromValueFunction ([] (float v, int)
    {
        const float t = 0.001f * std::pow (10.0f, 4.0f * v);
        return t < 1.0f ? juce::String (t * 1000.0f, t < 0.01f ? 1 : 0) + " ms" : juce::String (t, 2) + " s";
    }), expRb (Unit::Sec, 0.001, 10000.0) };
    const char* ch = "ABCD";
    for (int k = 1; k <= 4; ++k)
    {
        const juce::String s (k), c = juce::String::charToString ((juce::juce_wchar) ch[k - 1]);
        g->addChild (flt (juce::ParameterID (p + "att" + s, 1), n + c + " Attack", NR (0.0f, 1.0f), 0.15f, timeAttr));
        g->addChild (flt (juce::ParameterID (p + "dec" + s, 1), n + c + " Decay", NR (0.0f, 1.0f), 0.55f, timeAttr));
        g->addChild (choice (juce::ParameterID (p + "mode" + s, 1), n + c + " Mode", juce::StringArray { "Transient", "Sustained", "Cycle" }, 0));
    }
    g->addChild (std::make_unique<APB> (juce::ParameterID (p + "quad12", 1), n + "Quadrature A-B", false));
    g->addChild (std::make_unique<APB> (juce::ParameterID (p + "quad34", 1), n + "Quadrature C-D", false));
    g->addChild (flt (juce::ParameterID (p + "mix12", 1), n + "Quadrature Mix A-B", NR (0.0f, 1.0f), 0.5f));
    g->addChild (flt (juce::ParameterID (p + "mix34", 1), n + "Quadrature Mix C-D", NR (0.0f, 1.0f), 0.5f));
    l.add (std::move (g));
}

static void add292 (juce::AudioProcessorValueTreeState::ParameterLayout& l, int i)
{
    const juce::String p (prefix292 (i)), n ("292 " + juce::String::charToString ((juce::juce_wchar) ('A' + i)) + " ");
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> (p + "grp", n.trim(), "|");
    for (int k = 1; k <= 4; ++k)
    {
        const juce::String s (k);
        g->addChild (flt (juce::ParameterID (p + "off" + s, 1), n + s + " Offset", NR (0.0f, 1.0f), 0.0f));
        g->addChild (flt (juce::ParameterID (p + "cv" + s, 1), n + s + " CV Level", NR (0.0f, 1.0f), 1.0f));
        g->addChild (flt (juce::ParameterID (p + "res" + s, 1), n + s + " Resonance", NR (0.0f, 1.0f), 0.6f));
        g->addChild (choice (juce::ParameterID (p + "mode" + s, 1), n + s + " Mode", juce::StringArray { "LoPass", "Combo", "Gate" }, 1));
    }
    l.add (std::move (g));
}

static void add266 (juce::AudioProcessorValueTreeState::ParameterLayout& l)
{
    const juce::String p ("ua_"), n ("266 ");
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("ua_grp", "266", "|");
    g->addChild (flt (juce::ParameterID (p + "rate", 1), n + "Fluctuating Rate", NR (0.0f, 1.0f), 0.45f,
        Fmt { FA().withStringFromValueFunction ([] (float v, int) { return juce::String (0.05f * std::pow (1000.0f, v), 2) + " Hz"; }), expRb (Unit::Hz, 0.05, 1000.0) }));
    g->addChild (flt (juce::ParameterID (p + "rate2", 1), n + "Fluctuating Rate 2", NR (0.0f, 1.0f), 0.3f,
        Fmt { FA().withStringFromValueFunction ([] (float v, int) { return juce::String (0.05f * std::pow (1000.0f, v), 2) + " Hz"; }), expRb (Unit::Hz, 0.05, 1000.0) }));
    g->addChild (flt (juce::ParameterID (p + "smooth", 1), n + "Integrator Smoothing", NR (0.0f, 1.0f), 0.3f,
        Fmt { FA().withStringFromValueFunction ([] (float v, int) { return juce::String (0.01f * std::pow (1000.0f, v), 2) + " s"; }), expRb (Unit::Sec, 0.01, 1000.0) }));
    g->addChild (flt (juce::ParameterID (p + "steps", 1), n + "Quantized Steps", NR (1.0f, 6.0f, 1.0f), 3.0f));
    g->addChild (flt (juce::ParameterID (p + "dist", 1), n + "Stored Probability", NR (0.0f, 1.0f), 0.5f));
    l.add (std::move (g));
}

static void add245 (juce::AudioProcessorValueTreeState::ParameterLayout& l)
{
    const juce::String p ("sv_"), n ("245 ");
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("sv_grp", "245", "|");
    const char* rows = "abcd";
    for (int r = 0; r < 4; ++r)
        for (int st = 1; st <= 5; ++st)
            g->addChild (flt (juce::ParameterID (p + juce::String::charToString ((juce::juce_wchar) rows[r]) + juce::String (st), 1),
                n + juce::String::charToString ((juce::juce_wchar) ('A' + r)) + juce::String (st), NR (0.0f, 1.0f), 0.0f,
                Fmt { FA().withStringFromValueFunction ([] (float v, int) { return juce::String (v * 10.0f, 2) + " V"; }), linRb (Unit::Volt, 0.0, 10.0) }));
    for (int k = 1; k <= 4; ++k)
        g->addChild (std::make_unique<APB> (juce::ParameterID (p + "sw" + juce::String (k), 1), n + "Stage Switch " + juce::String (k), true));
    g->addChild (flt (juce::ParameterID (p + "time", 1), n + "Pulser Time", NR (0.0f, 1.0f), 0.55f,
        Fmt { FA().withStringFromValueFunction ([] (float v, int) { const float t = 0.002f * std::pow (5000.0f, v); return t < 1.0f ? juce::String (t * 1000.0f, 0) + " ms" : juce::String (t, 2) + " s"; }), expRb (Unit::Sec, 0.002, 5000.0) }));
    g->addChild (flt (juce::ParameterID (p + "tcv", 1), n + "Pulser Time CV", NR (-1.0f, 1.0f), 0.0f));
    g->addChild (flt (juce::ParameterID (p + "len", 1), n + "Pulse Length", NR (0.0f, 1.0f), 0.5f,
        Fmt { FA().withStringFromValueFunction ([] (float v, int) { return juce::String (v * 100.0f, 0) + " %"; }), linRb (Unit::Pct, 0.0, 100.0) }));
    l.add (std::move (g));
}

static void add242 (juce::AudioProcessorValueTreeState::ParameterLayout& l)
{
    const juce::String p ("pp_"), n ("242 ");
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("pp_grp", "242", "|");
    const char* rows = "abc";
    for (int r = 0; r < 3; ++r)
        for (int st = 1; st <= 12; ++st)
            g->addChild (std::make_unique<APB> (juce::ParameterID (p + juce::String::charToString ((juce::juce_wchar) rows[r]) + juce::String (st), 1),
                n + "Matrix " + juce::String::charToString ((juce::juce_wchar) ('A' + r)) + juce::String (st), false));
    g->addChild (flt (juce::ParameterID (p + "len", 1), n + "Stages", NR (1.0f, 12.0f, 1.0f), 8.0f));
    g->addChild (flt (juce::ParameterID (p + "period", 1), n + "Period", NR (0.0f, 1.0f), 0.5f,
        Fmt { FA().withStringFromValueFunction ([] (float v, int)
        {
            if (v > 0.92f) return juce::String ("off");
            const float t = 0.002f * std::pow (5000.0f, v / 0.92f);
            return t < 1.0f ? juce::String (t * 1000.0f, 0) + " ms" : juce::String (t, 2) + " s";
        }), { Unit::Sec, [] (float v) { return 0.002 * std::pow (5000.0, v / 0.92); }, 0.0f, 0.92f, "off", 1.0f } }));
    g->addChild (flt (juce::ParameterID (p + "cv", 1), n + "Period CV", NR (-1.0f, 1.0f), 0.0f));
    g->addChild (flt (juce::ParameterID (p + "row", 1), n + "Edit Row", NR (1.0f, 4.0f, 1.0f), 2.0f));
    g->addChild (flt (juce::ParameterID (p + "col", 1), n + "Edit Column", NR (1.0f, 12.0f, 1.0f), 1.0f));
    l.add (std::move (g));
}

static Fmt voltAttr (float scale, int dec = 2)
{
    return { FA().withStringFromValueFunction ([scale, dec] (float v, int) { return juce::String (v * scale, dec) + " V"; }), linRb (Unit::Volt, 0.0, scale) };
}

static void add257 (juce::AudioProcessorValueTreeState::ParameterLayout& l)
{
    const juce::String p ("vp_"), n ("257 ");
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("vp_grp", "257", "|");
    for (int s = 1; s <= 2; ++s)
    {
        const juce::String k (s);
        g->addChild (flt (juce::ParameterID (p + "k" + k, 1), n + k + " K (attenuverter)", NR (-1.0f, 1.0f), 1.0f));
        g->addChild (flt (juce::ParameterID (p + "m" + k, 1), n + k + " M (crossfade)", NR (0.0f, 1.0f), 0.0f));
        g->addChild (std::make_unique<APB> (juce::ParameterID (p + "p5" + k, 1), n + k + " +.5 (Vb + 5 V)", false));
        g->addChild (flt (juce::ParameterID (p + "off" + k, 1), n + k + " Offset", NR (0.0f, 1.0f), 0.0f, voltAttr (10.0f)));
    }
    l.add (std::move (g));
}

static void add258 (juce::AudioProcessorValueTreeState::ParameterLayout& l, int i)
{
    const juce::String p (prefix258 (i)), n ("258 " + juce::String::charToString ((juce::juce_wchar) ('A' + i)) + " ");
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> (p + "grp", n.trim(), "|");
    const Fmt hz { FA().withStringFromValueFunction ([] (float v, int)
    {
        const float f = 5.0f * std::pow (4000.0f, v);
        return f < 1000.0f ? juce::String (f, f < 100.0f ? 1 : 0) + " Hz" : juce::String (f / 1000.0f, 2) + " kHz";
    }), expRb (Unit::Hz, 5.0, 4000.0) };
    const float defHz[2] = { 130.81f, 196.0f };
    for (int o = 1; o <= 2; ++o)
    {
        const juce::String k (o);
        g->addChild (flt (juce::ParameterID (p + "freq" + k, 1), n + k + " Frequency", NR (0.0f, 1.0f), std::log (defHz[o - 1] / 5.0f) / std::log (4000.0f), hz));
        g->addChild (flt (juce::ParameterID (p + "fine" + k, 1), n + k + " Fine", NR (-3.5f, 3.5f), 0.0f, unitAttr (" st")));
        g->addChild (flt (juce::ParameterID (p + "proc" + k, 1), n + k + " Processing", NR (-1.0f, 1.0f), 0.0f));
        g->addChild (flt (juce::ParameterID (p + "fm" + k, 1), n + k + " FM Depth", NR (0.0f, 1.0f), 0.0f));
        g->addChild (flt (juce::ParameterID (p + "shape" + k, 1), n + k + (o == 1 ? " Waveshape (sine-saw)" : " Waveshape (sine-square)"), NR (0.0f, 1.0f), 0.0f));
    }
    g->addChild (std::make_unique<APB> (juce::ParameterID (p + "volt1", 1), n + "1V/Oct", false));
    l.add (std::move (g));
}

static void add264 (juce::AudioProcessorValueTreeState::ParameterLayout& l)
{
    const juce::String p ("sh_"), n ("264 ");
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("sh_grp", "264", "|");
    for (int c = 1; c <= 4; ++c)
        g->addChild (std::make_unique<APB> (juce::ParameterID (p + "track" + juce::String (c), 1), n + juce::String (c) + " Track (off = Sample)", false));
    g->addChild (std::make_unique<APB> (juce::ParameterID (p + "common", 1), n + "Common C.V.", false));
    g->addChild (flt (juce::ParameterID (p + "voices", 1), n + "Number of Voices", NR (1.0f, 4.0f, 1.0f), 4.0f));
    g->addChild (flt (juce::ParameterID (p + "offset", 1), n + "Offset", NR (-2.0f, 2.0f), 0.0f, unitAttr (" V")));
    g->addChild (flt (juce::ParameterID (p + "mod", 1), n + "Modulation", NR (0.0f, 1.0f), 0.0f));
    g->addChild (choice (juce::ParameterID (p + "scale", 1), n + "ART Scale", juce::StringArray { "m (minor)", "ch (chromatic)", "M (major)" }, 1));
    g->addChild (choice (juce::ParameterID (p + "key", 1), n + "ART Key", juce::StringArray { "c", "c#", "d", "d#", "e", "f", "f#", "g", "g#", "a", "a#", "b" }, 0));
    l.add (std::move (g));
}

static void add230 (juce::AudioProcessorValueTreeState::ParameterLayout& l)
{
    const juce::String p ("ef_"), n ("230 ");
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("ef_grp", "230", "|");
    for (int c = 1; c <= 3; ++c)
    {
        const juce::String k (c);
        g->addChild (flt (juce::ParameterID (p + "sens" + k, 1), n + k + " Sensitivity", NR (0.0f, 1.0f), 0.4f,
            Fmt { FA().withStringFromValueFunction ([] (float v, int) { return juce::String (-20.0f + 50.0f * v, 1) + " dB"; }), linRb (Unit::dB, -20.0, 50.0) }));
        g->addChild (flt (juce::ParameterID (p + "decay" + k, 1), n + k + " Decay", NR (0.0f, 1.0f), 0.35f,
            Fmt { FA().withStringFromValueFunction ([] (float v, int) { const float t = 0.01f * std::pow (300.0f, v); return t < 1.0f ? juce::String (t * 1000.0f, 0) + " ms" : juce::String (t, 2) + " s"; }), expRb (Unit::Sec, 0.01, 300.0) }));
        g->addChild (flt (juce::ParameterID (p + "thr" + k, 1), n + k + " Pulse Threshold", NR (0.0f, 1.0f), 0.3f, voltAttr (10.0f, 1)));
        g->addChild (std::make_unique<APB> (juce::ParameterID (p + "mode" + k, 1), n + k + " Sustained (off = Transient)", false));
    }
    l.add (std::move (g));
}

static void add285 (juce::AudioProcessorValueTreeState::ParameterLayout& l)
{
    const juce::String p ("fs_"), n ("285 ");
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("fs_grp", "285", "|");
    g->addChild (flt (juce::ParameterID (p + "ctl", 1), n + "Shift Control CV", NR (-1.0f, 1.0f), 0.0f));
    g->addChild (std::make_unique<APB> (juce::ParameterID (p + "int", 1), n + "Internal Reference", true));
    g->addChild (flt (juce::ParameterID (p + "ref", 1), n + "Reference (shift)", NR (0.0f, 1.0f), 0.25f,
        Fmt { FA().withStringFromValueFunction ([] (float v, int) { return juce::String (1000.0f * v * v, 1) + " Hz"; }), { Unit::Hz, [] (float v) { return 1000.0 * v * v; } } }));
    g->addChild (flt (juce::ParameterID (p + "fm", 1), n + "F.M.", NR (0.0f, 1.0f), 0.0f));
    g->addChild (flt (juce::ParameterID (p + "bmctl", 1), n + "Modulation Control CV", NR (0.0f, 1.0f), 0.5f));
    g->addChild (flt (juce::ParameterID (p + "amp", 1), n + "Amplitude - Ring", NR (0.0f, 1.0f), 1.0f));
    l.add (std::move (g));
}

static void add277 (juce::AudioProcessorValueTreeState::ParameterLayout& l)
{
    const juce::String p ("dl_"), n ("277 ");
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("dl_grp", "277", "|");
    for (int i = 0; i < 3; ++i)
        g->addChild (flt (juce::ParameterID (p + "lvl" + juce::String::charToString ((juce::juce_wchar) ('a' + i)), 1),
                     n + "Input " + juce::String::charToString ((juce::juce_wchar) ('A' + i)), NR (0.0f, 1.0f), i == 0 ? 0.8f : 0.0f));
    g->addChild (flt (juce::ParameterID (p + "time", 1), n + "Delay Time", NR (0.0f, 1.0f), 0.33f,
        Fmt { FA().withStringFromValueFunction ([] (float v, int) { return juce::String (0.05f + 0.15f * v, 3) + " s (x4 long)"; }), linRb (Unit::Sec, 0.05, 0.15) }));
    g->addChild (std::make_unique<APB> (juce::ParameterID (p + "long", 1), n + "Long", false));
    g->addChild (flt (juce::ParameterID (p + "ctl", 1), n + "Control Input", NR (-1.0f, 1.0f), 0.0f));
    g->addChild (flt (juce::ParameterID (p + "mod", 1), n + "Modulation", NR (0.0f, 1.0f), 0.0f));
    l.add (std::move (g));
}

static void add207 (juce::AudioProcessorValueTreeState::ParameterLayout& l)
{
    const juce::String p ("mx_"), n ("207 ");
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("mx_grp", "207", "|");
    for (int c = 0; c < 6; ++c)
    {
        const juce::String ch = juce::String::charToString ((juce::juce_wchar) ('a' + c)), CH = ch.toUpperCase();
        g->addChild (flt (juce::ParameterID (p + "lvl_" + ch, 1), n + CH + " Level", NR (0.0f, 1.0f), 0.8f,
            Fmt { FA().withStringFromValueFunction ([] (float v, int) { return juce::String (v * 10.0f, 1); }), linRb (Unit::None, 0.0, 10.0) }));
        g->addChild (std::make_unique<APB> (juce::ParameterID (p + "prog_" + ch, 1), n + CH + " To Signal Outputs", true));
        g->addChild (std::make_unique<APB> (juce::ParameterID (p + "mon_" + ch, 1), n + CH + " To Monitor", false));
    }
    for (int k = 1; k <= 4; ++k)
        g->addChild (flt (juce::ParameterID (p + "pan" + juce::String (k), 1), n + juce::String::charToString ((juce::juce_wchar) ('A' + k)) + " Assignment (L-R)", NR (0.0f, 1.0f), 0.5f));
    g->addChild (flt (juce::ParameterID (p + "gain", 1), n + "Preamp Gain", NR (0.0f, 1.0f), 0.3f));
    g->addChild (std::make_unique<APB> (juce::ParameterID (p + "range", 1), n + "Preamp Range 70 dB", false));
    l.add (std::move (g));
}

static void add296 (juce::AudioProcessorValueTreeState::ParameterLayout& l)
{
    const juce::String p ("sp_"), n ("296 ");
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("sp_grp", "296", "|");
    static const char* names[16] = { "<100", "150", "250", "350", "500", "630", "800", "1k", "1.3k", "1.6k", "2k", "2.6k", "3.5k", "5k", "8k", ">10k" };
    for (int b = 0; b < 16; ++b)
        g->addChild (flt (juce::ParameterID (p + "att" + juce::String (b), 1), n + names[b] + " Attenuator", NR (0.0f, 1.0f), 36.0f / 42.0f,
            Fmt { FA().withStringFromValueFunction ([] (float v, int) { return v < 0.02f ? juce::String ("off") : juce::String (-36.0f + 42.0f * v, 1) + " dB"; }),
                  { Unit::dB, [] (float v) { return -36.0 + 42.0 * v; }, 0.02f, 1.0f, "off", 0.0f } }));
    g->addChild (choice (juce::ParameterID (p + "decay", 1), n + "Env. Decay Time", juce::StringArray { "short", "combo", "long" }, 1));
    g->addChild (flt (juce::ParameterID (p + "freq", 1), n + "Program Frequency", NR (0.0f, 1.0f), 0.5f,
        Fmt { FA().withStringFromValueFunction ([] (float v, int) { const float hz = 100.0f * std::pow (100.0f, v); return hz < 1000.0f ? juce::String (hz, 0) + " Hz" : juce::String (hz / 1000.0f, 1) + " kHz"; }), expRb (Unit::Hz, 100.0, 100.0) }));
    g->addChild (flt (juce::ParameterID (p + "fcv", 1), n + "Program Frequency CV", NR (-1.0f, 1.0f), 0.0f));
    g->addChild (flt (juce::ParameterID (p + "width", 1), n + "Program Width", NR (0.0f, 1.0f), 1.0f));
    g->addChild (flt (juce::ParameterID (p + "pre_even", 1), n + "Even Pre-emphasis", NR (0.0f, 1.0f), 0.0f));
    g->addChild (flt (juce::ParameterID (p + "pre_odd", 1), n + "Odd Pre-emphasis", NR (0.0f, 1.0f), 0.0f));
    g->addChild (std::make_unique<APB> (juce::ParameterID (p + "tr_even", 1), n + "Transfer to Even", false));
    g->addChild (std::make_unique<APB> (juce::ParameterID (p + "tr_odd", 1), n + "Transfer to Odd", false));
    l.add (std::move (g));
}

static void add248 (juce::AudioProcessorValueTreeState::ParameterLayout& l)
{
    const juce::String p ("mf_"), n ("248 ");
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("mf_grp", "248", "|");
    static const char* fnNames[13] = { "p1", "p2", "quant", "slope", "range", "vext", "stop", "sust", "enable", "first", "last", "trange", "text" };
    static const char* fnLabels[13] = { "Pulse 1", "Pulse 2", "Quantize", "Sloped", "Range", "Voltage External", "Stop", "Sustain", "Enable", "Cycle First", "Cycle Last", "Time Range", "Time External" };
    for (int s = 1; s <= 16; ++s)
    {
        const juce::String k (s);
        g->addChild (flt (juce::ParameterID (p + "lvl" + k, 1), n + "Stage " + k + " Level", NR (0.0f, 1.0f), 0.5f));
        g->addChild (flt (juce::ParameterID (p + "tim" + k, 1), n + "Stage " + k + " Interval Time", NR (0.0f, 1.0f), 0.5f));
        for (int f = 0; f < 13; ++f)
        {
            const juce::ParameterID id (p + fnNames[f] + "_" + k, 1);
            const juce::String nm = n + "Stage " + k + " " + fnLabels[f];
            if (f == 4) g->addChild (choice (id, nm, juce::StringArray { "full", "half", "+0", "+2", "+4", "+6", "+8" }, 0));
            else if (f == 11) g->addChild (choice (id, nm, juce::StringArray { ".002-.03 s", ".02-.3 s", ".2-3 s", "2-30 s" }, 2));
            else g->addChild (std::make_unique<APB> (id, nm, false));
        }
    }
    g->addChild (flt (juce::ParameterID (p + "edit", 1), n + "Stage No. (edit)", NR (1.0f, 16.0f, 1.0f), 1.0f));
    g->addChild (choice (juce::ParameterID (p + "key", 1), n + "Key", juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    g->addChild (choice (juce::ParameterID (p + "scale", 1), n + "Scale", juce::StringArray { "m (minor)", "ch (chromatic)", "M (major)" }, 1));
    for (int f = 1; f <= 2; ++f)
    {
        const juce::String k (f), fg = n + "FG" + k + " ";
        g->addChild (flt (juce::ParameterID (p + "mult" + k, 1), fg + "Time Multiplier", NR (0.0f, 1.0f), 1.0f / 3.0f,
            Fmt { FA().withStringFromValueFunction ([] (float v, int) { return "x" + juce::String (0.5f * std::pow (8.0f, v), 2); }), expRb (Unit::Times, 0.5, 8.0) }));
        g->addChild (flt (juce::ParameterID (p + "mcv" + k, 1), fg + "Time Multiplier CV", NR (-1.0f, 1.0f), 0.0f));
        g->addChild (flt (juce::ParameterID (p + "addr" + k, 1), fg + "Stage Address", NR (0.0f, 1.0f), 0.0f));
        g->addChild (std::make_unique<APB> (juce::ParameterID (p + "cont" + k, 1), fg + "Continuous", false));
        g->addChild (std::make_unique<APB> (juce::ParameterID (p + "ext" + k, 1), fg + "External Address", false));
        g->addChild (choice (juce::ParameterID (p + "art" + k, 1), fg + "ART Gate Source", juce::StringArray { "1", "2", "all" }, 2));
    }
    l.add (std::move (g));
}

static void add218 (juce::AudioProcessorValueTreeState::ParameterLayout& l)
{
    const juce::String p ("kb_"), n ("218 ");
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("kb_grp", "218", "|");
    g->addChild (flt (juce::ParameterID (p + "porta", 1), n + "Portamento Slope", NR (0.0f, 1.0f), 0.0f));
    g->addChild (flt (juce::ParameterID (p + "rate", 1), n + "Arpeggiation Rate", NR (0.0f, 1.0f), 0.5f));
    g->addChild (choice (juce::ParameterID (p + "arp", 1), n + "Arpeggiation Pattern", juce::StringArray { "ascending", "random", "none" }, 2));
    g->addChild (choice (juce::ParameterID (p + "add", 1), n + "Add To Pitch", juce::StringArray { "octaves", "preset", "none" }, 2));
    for (int i = 1; i <= 4; ++i)
        g->addChild (flt (juce::ParameterID (p + "preset" + juce::String (i), 1), n + "Preset Voltage " + juce::String (i), NR (0.0f, 1.0f), 0.0f,
            Fmt { FA().withStringFromValueFunction ([] (float v, int) { return juce::String (v * 10.0f, 2) + " V"; }), linRb (Unit::Volt, 0.0, 10.0) }));
    g->addChild (flt (juce::ParameterID (p + "sel", 1), n + "Preset Pad", NR (0.0f, 3.0f, 1.0f), 0.0f));
    l.add (std::move (g));
}

static void add250 (juce::AudioProcessorValueTreeState::ParameterLayout& l)
{
    const juce::String p ("sq_"), n ("250 ");
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("sq_grp", "250", "|");
    for (int s = 1; s <= 16; ++s)
    {
        const juce::String k (s), st = n + "Stage " + k + " ";
        g->addChild (flt (juce::ParameterID (p + "v1_" + k, 1), st + "Voltage 1", NR (0.0f, 1.0f), 0.0f));
        g->addChild (flt (juce::ParameterID (p + "v2_" + k, 1), st + "Voltage 2", NR (0.0f, 1.0f), 0.0f));
        g->addChild (flt (juce::ParameterID (p + "t_" + k, 1), st + "Time", NR (0.0f, 1.0f), 0.5f));
        for (auto* b : { "p1", "p2", "tq", "ti", "vq", "vi", "text", "vext", "fix" })
            g->addChild (std::make_unique<APB> (juce::ParameterID (p + b + juce::String ("_") + k, 1), st + b, false));
        g->addChild (choice (juce::ParameterID (p + "trng_" + k, 1), st + "Time Range", juce::StringArray { ".01-.1 s", ".08-.8 s", ".6-6 s", "sync" }, 1));
        g->addChild (choice (juce::ParameterID (p + "vrng_" + k, 1), st + "Voltage Range", juce::StringArray { "0-2", "2-4", "4-6", "6-8", "8-10", "all" }, 5));
        g->addChild (choice (juce::ParameterID (p + "mode_" + k, 1), st + "Mode", juce::StringArray { "adv", "sust", "enble", "stop", "pulse" }, 0));
    }
    g->addChild (flt (juce::ParameterID (p + "edit", 1), n + "Edit Stage", NR (1.0f, 16.0f, 1.0f), 1.0f));
    g->addChild (std::make_unique<APB> (juce::ParameterID (p + "editmode", 1), n + "Edit Mode", false));
    g->addChild (flt (juce::ParameterID (p + "set", 1), n + "Voltage Set", NR (0.0f, 1.0f, 1.0f), 0.0f));
    g->addChild (flt (juce::ParameterID (p + "mult", 1), n + "Time Multiplier", NR (0.0f, 1.0f), 0.5f,
        Fmt { FA().withStringFromValueFunction ([] (float v, int) { return "x" + juce::String (0.25f * std::pow (16.0f, v), 2); }), expRb (Unit::Times, 0.25, 16.0) }));
    g->addChild (flt (juce::ParameterID (p + "mcv", 1), n + "Time Multiplier CV", NR (-1.0f, 1.0f), 0.0f));
    g->addChild (choice (juce::ParameterID (p + "addr", 1), n + "Stage Addressing", juce::StringArray { "off", "strobed", "continuous" }, 0));
    g->addChild (flt (juce::ParameterID (p + "offset", 1), n + "Address Offset", NR (0.0f, 1.0f), 0.0f));
    g->addChild (flt (juce::ParameterID (p + "loopend", 1), n + "Loop From Stage", NR (0.0f, 16.0f, 1.0f), 0.0f));
    g->addChild (flt (juce::ParameterID (p + "loopto", 1), n + "Jump To Stage", NR (1.0f, 16.0f, 1.0f), 1.0f));
    g->addChild (flt (juce::ParameterID (p + "loops", 1), n + "Loop Count (0 = forever)", NR (0.0f, 16.0f, 1.0f), 0.0f));
    g->addChild (flt (juce::ParameterID (p + "setstep", 1), n + "Loop Setup Step", NR (0.0f, 3.0f, 1.0f), 0.0f));
    g->addChild (std::make_unique<APB> (juce::ParameterID (p + "remote", 1), n + "Remote Enable", false));
    l.add (std::move (g));
}

static void add252 (juce::AudioProcessorValueTreeState::ParameterLayout& l)
{
    const juce::String p ("rg_"), n ("252 ");
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("rg_grp", "252", "|");
    g->addChild (flt (juce::ParameterID (p + "bpm", 1), n + "Master BPM", NR (20.0f, 300.0f, 1.0f), 120.0f,
        Fmt { FA().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " BPM"; }), { Unit::Bpm } }));
    g->addChild (flt (juce::ParameterID (p + "pdur", 1), n + "Pulse Duration", NR (0.0f, 1.0f), 0.1f));
    g->addChild (choice (juce::ParameterID (p + "sync", 1), n + "Sync", juce::StringArray { "free", "on 1", "on cycl" }, 1));
    g->addChild (std::make_unique<APB> (juce::ParameterID (p + "play", 1), n + "Play", true));
    static const char* rings[12] = { "none", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "16" };
    for (int k = 1; k <= 3; ++k)
    {
        const juce::String kk (k), nm = n + (k == 1 ? juce::String ("Master") : "Clock " + kk);
        g->addChild (flt (juce::ParameterID (p + "clk" + kk, 1), nm + " Ring", NR (0.0f, 11.0f, 1.0f), k == 1 ? 11.0f : 0.0f,
            Fmt { FA().withStringFromValueFunction ([] (float v, int) { return juce::String (rings[juce::jlimit (0, 11, (int) std::lround (v))]); }), { Unit::Label } }));
        g->addChild (std::make_unique<APB> (juce::ParameterID (p + "dt" + kk, 1), nm + " Delta-t", false));
        g->addChild (flt (juce::ParameterID (p + "dts" + kk, 1), nm + " Delta-t Scale", NR (0.5f, 3.0f), 1.0f,
            Fmt { FA().withStringFromValueFunction ([] (float v, int) { return "x" + juce::String (v, 2); }), { Unit::Times } }));
    }
    g->addChild (flt (juce::ParameterID (p + "ring", 1), n + "Edit Ring (beats/cycl)", NR (0.0f, 10.0f, 1.0f), 10.0f,
        Fmt { FA().withStringFromValueFunction ([] (float v, int) { return juce::String (rings[juce::jlimit (0, 10, (int) std::lround (v)) + 1]); }), { Unit::Label } }));
    g->addChild (flt (juce::ParameterID (p + "cell", 1), n + "Cursor Cell", NR (0.0f, 15.0f, 1.0f), 0.0f));
    g->addChild (flt (juce::ParameterID (p + "pattern", 1), n + "Pattern #", NR (0.0f, 16.0f, 1.0f), 0.0f));
    g->addChild (flt (juce::ParameterID (p + "layer", 1), n + "Display", NR (0.0f, 3.0f, 1.0f), 0.0f));
    g->addChild (flt (juce::ParameterID (p + "cvsel", 1), n + "CV Edit Select", NR (0.0f, 5.0f, 1.0f), 0.0f));
    g->addChild (std::make_unique<APB> (juce::ParameterID (p + "euclid", 1), n + "Euclidean", false));
    g->addChild (std::make_unique<APB> (juce::ParameterID (p + "edit", 1), n + "Edit", true));
    g->addChild (std::make_unique<APB> (juce::ParameterID (p + "remote", 1), n + "Remote Enable", false));
    l.add (std::move (g));
}

static Fmt secsAttr (float (*fn) (float))
{
    return { FA().withStringFromValueFunction ([fn] (float v, int)
    {
        const float s = fn (v);
        return s < 1.0f ? juce::String (s * 1000.0f, s < 0.01f ? 1 : 0) + " ms" : juce::String (s, 2) + " s";
    }), { Unit::Sec, [fn] (float v) { return (double) fn (v); } } };
}
static Fmt hz31Attr()
{
    return { FA().withStringFromValueFunction ([] (float v, int)
    {
        const float hz = 31.0f * std::pow (2.0f, 7.0f * v);
        return hz < 1000.0f ? juce::String (hz, 0) + " Hz" : juce::String (hz / 1000.0f, 2) + " kHz";
    }), expRb (Unit::Hz, 31.0, 128.0) };
}
static float t284 (float k) { return 0.002f * std::pow (5000.0f, k); }
static float t291 (float k) { return 0.01f * std::pow (1000.0f, k); }

static void add267 (juce::AudioProcessorValueTreeState::ParameterLayout& l)
{
    const juce::String p ("us_"), n ("267 ");
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("us_grp", "267", "|");
    for (auto* ab : { "a", "b" })
    {
        const juce::String X (juce::String (ab).toUpperCase());
        g->addChild (flt (juce::ParameterID (p + "rate" + ab, 1), n + "Random " + X + " Rate", NR (0.0f, 1.0f), 0.45f,
            Fmt { FA().withStringFromValueFunction ([] (float v, int) { return juce::String (0.05f * std::pow (1000.0f, v), 2) + " Hz"; }), expRb (Unit::Hz, 0.05, 1000.0) }));
        g->addChild (flt (juce::ParameterID (p + "rand" + ab, 1), n + "Random " + X + " Random", NR (0.0f, 1.0f), 1.0f));
    }
    for (auto* ab : { "a", "b" })
    {
        const juce::String X (juce::String (ab).toUpperCase());
        g->addChild (flt (juce::ParameterID (p + "freq" + ab, 1), n + "Filter " + X + " Frequency", NR (0.0f, 1.0f), 0.6f, hz31Attr()));
        g->addChild (flt (juce::ParameterID (p + "bw" + ab, 1), n + "Filter " + X + " Bandwidth", NR (0.0f, 1.0f), 0.4f));
        g->addChild (flt (juce::ParameterID (p + "idx" + ab, 1), n + "Filter " + X + " Modulation Index", NR (0.0f, 1.0f), 0.0f));
        g->addChild (choice (juce::ParameterID (p + "mod" + ab, 1), n + "Filter " + X + " Mod Type", juce::StringArray { "amp", "freqncy", "bandwidth" }, 0));
    }
    g->addChild (std::make_unique<APB> (juce::ParameterID (p + "remote", 1), n + "Remote Enable", false));
    l.add (std::move (g));
}

static void add275 (juce::AudioProcessorValueTreeState::ParameterLayout& l)
{
    const juce::String p ("rv_"), n ("275 ");
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("rv_grp", "275", "|");
    for (int c = 1; c <= 2; ++c)
    {
        const juce::String k (c), ch = n + "Ch " + k + " ";
        g->addChild (flt (juce::ParameterID (p + "treb" + k, 1), ch + "Treble", NR (-10.0f, 10.0f), 0.0f, unitAttr (" dB", 1)));
        g->addChild (flt (juce::ParameterID (p + "bass" + k, 1), ch + "Bass", NR (-10.0f, 10.0f), 0.0f, unitAttr (" dB", 1)));
        g->addChild (flt (juce::ParameterID (p + "mix" + k, 1), ch + "Reverberation", NR (0.0f, 1.0f), 0.3f));
        g->addChild (std::make_unique<APB> (juce::ParameterID (p + "rec" + k, 1), ch + "Recede (off = Near)", false));
        g->addChild (std::make_unique<APB> (juce::ParameterID (p + "hot" + k, 1), ch + "Hot Input", false));
    }
    l.add (std::move (g));
}

static void add284 (juce::AudioProcessorValueTreeState::ParameterLayout& l)
{
    const juce::String p ("ev_"), n ("284 ");
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("ev_grp", "284", "|");
    for (int c = 1; c <= 4; ++c)
    {
        const juce::String k (c), ch = n + "Ch " + k + " ";
        g->addChild (flt (juce::ParameterID (p + "ctl" + k, 1), ch + "Control", NR (-1.0f, 1.0f), 0.0f));
        g->addChild (flt (juce::ParameterID (p + "del" + k, 1), ch + "Delay", NR (0.0f, 1.0f), 0.0f, secsAttr (t284)));
        g->addChild (flt (juce::ParameterID (p + "att" + k, 1), ch + "Attack", NR (0.0f, 1.0f), 0.25f, secsAttr (t284)));
        g->addChild (flt (juce::ParameterID (p + "dur" + k, 1), ch + "Duration", NR (0.0f, 1.0f), 0.4f, secsAttr (t284)));
        g->addChild (std::make_unique<APB> (juce::ParameterID (p + "sus" + k, 1), ch + "Sustain", false));
        g->addChild (flt (juce::ParameterID (p + "dec" + k, 1), ch + "Decay", NR (0.0f, 1.0f), 0.5f, secsAttr (t284)));
    }
    l.add (std::move (g));
}

static void add291 (juce::AudioProcessorValueTreeState::ParameterLayout& l)
{
    const juce::String p ("tm_"), n ("291 ");
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("tm_grp", "291", "|");
    static const float defF[3] = { 0.42f, 0.62f, 0.82f };
    for (int s = 1; s <= 8; ++s)
    {
        const juce::String sn (s);
        for (int nd = 0; nd < 3; ++nd)
        {
            const juce::String c = juce::String::charToString ((juce::juce_wchar) ('a' + nd)), st = n + "Stage " + sn + " " + c.toUpperCase() + " ";
            g->addChild (flt (juce::ParameterID (p + "f_" + sn + "_" + c, 1), st + "Frequency", NR (0.0f, 1.0f), juce::jlimit (0.0f, 1.0f, defF[nd] + 0.02f * (float) (s - 1) * (nd == 1 ? -1.0f : 1.0f)), hz31Attr()));
            g->addChild (flt (juce::ParameterID (p + "bw_" + sn + "_" + c, 1), st + "Bandwidth", NR (0.0f, 1.0f), 0.3f));
            g->addChild (flt (juce::ParameterID (p + "amp_" + sn + "_" + c, 1), st + "Amplitude", NR (0.0f, 1.0f), 0.8f));
        }
        g->addChild (flt (juce::ParameterID (p + "time_" + sn, 1), n + "Stage " + sn + " Time", NR (0.0f, 1.0f), 0.5f, secsAttr (t291)));
    }
    g->addChild (flt (juce::ParameterID (p + "node", 1), n + "Node", NR (0.0f, 2.0f, 1.0f), 0.0f,
        Fmt { FA().withStringFromValueFunction ([] (float v, int) { return juce::String::charToString ((juce::juce_wchar) ('A' + juce::jlimit (0, 2, (int) std::lround (v)))); }), { Unit::Label } }));
    g->addChild (flt (juce::ParameterID (p + "edit", 1), n + "Edit Stage", NR (1.0f, 8.0f, 1.0f), 1.0f));
    g->addChild (std::make_unique<APB> (juce::ParameterID (p + "play", 1), n + "Play", false));
    g->addChild (flt (juce::ParameterID (p + "index", 1), n + "Modulation Index", NR (0.0f, 1.0f), 0.0f));
    g->addChild (choice (juce::ParameterID (p + "modtype", 1), n + "Modulation Type", juce::StringArray { "a.m.", "b.w.", "f.m." }, 0));
    g->addChild (std::make_unique<APB> (juce::ParameterID (p + "solo", 1), n + "Solo", false));
    g->addChild (std::make_unique<APB> (juce::ParameterID (p + "remote", 1), n + "Remote Enable", false));
    l.add (std::move (g));
}

static void add666 (juce::AudioProcessorValueTreeState::ParameterLayout& l, int i)
{
    const juce::String p (prefix666 (i)), n ("SC-666 " + juce::String (i + 1) + " ");
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> (p + "grp", n.trim(), "|");
    const Fmt vdiv { FA().withStringFromValueFunction ([] (float v, int) { return juce::String (0.02f * std::pow (250.0f, v), 2) + " /div"; }), expRb (Unit::Div, 0.02, 250.0) };
    g->addChild (flt (juce::ParameterID (p + "va", 1), n + "Vertical A", NR (0.0f, 1.0f), 0.42f, vdiv));
    g->addChild (flt (juce::ParameterID (p + "vb", 1), n + "Vertical B", NR (0.0f, 1.0f), 0.42f, vdiv));
    g->addChild (flt (juce::ParameterID (p + "hpos", 1), n + "Horizontal Position", NR (-1.0f, 1.0f), 0.0f));
    g->addChild (flt (juce::ParameterID (p + "xgain", 1), n + "X Gain", NR (0.0f, 1.0f), 0.42f, vdiv));
    g->addChild (choice (juce::ParameterID (p + "src", 1), n + "Trigger Source", juce::StringArray { "INT", "EXT", "FREE" }, 0));
    g->addChild (flt (juce::ParameterID (p + "level", 1), n + "Trigger Level", NR (-1.0f, 1.0f), 0.0f));
    g->addChild (flt (juce::ParameterID (p + "time", 1), n + "Time / Div", NR (0.0f, 1.0f), 0.5f,
        Fmt { FA().withStringFromValueFunction ([] (float v, int)
        { const float s = 0.0001f * std::pow (10000.0f, v); return s < 0.001f ? juce::String (s * 1.0e6f, 0) + " us" : (s < 1.0f ? juce::String (s * 1000.0f, 1) + " ms" : juce::String (s, 2) + " s"); }),
              expRb (Unit::Sec, 0.0001, 10000.0) }));
    g->addChild (choice (juce::ParameterID (p + "mode", 1), n + "Mode", juce::StringArray { "AUTO", "NORM", "SINGLE", "X-Y" }, 0));
    g->addChild (choice (juce::ParameterID (p + "coupling", 1), n + "Coupling", juce::StringArray { "AC", "DC", "GND" }, 1));
    g->addChild (std::make_unique<APB> (juce::ParameterID (p + "power", 1), n + "Power", true));
    g->addChild (flt (juce::ParameterID (p + "intensity", 1), n + "Intensity", NR (0.0f, 1.0f), 0.7f));
    g->addChild (flt (juce::ParameterID (p + "focus", 1), n + "Focus", NR (0.0f, 1.0f), 0.5f));
    l.add (std::move (g));
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("midi_grp", "MIDI Interface", "|");
        g->addChild (flt (juce::ParameterID ("midi_glide", 1), "MIDI Glide", NR (0.0f, 1.0f), 0.0f));
        g->addChild (flt (juce::ParameterID ("midi_bend", 1), "MIDI Bend Range", NR (0.0f, 12.0f, 1.0f), 2.0f, unitAttr (" st", 0)));
        g->addChild (flt (juce::ParameterID ("midi_trans", 1), "MIDI Transpose", NR (-3.0f, 3.0f, 1.0f), 0.0f, unitAttr (" oct", 0)));
        l.add (std::move (g));
    }
    add218 (l);
    add242 (l);
    add245 (l);
    add248 (l);
    add250 (l);
    add252 (l);
    add266 (l);
    add230 (l);
    add264 (l);
    add257 (l);
    for (int i = 0; i < kMax281; ++i) add281 (l, i);
    for (int i = 0; i < kMax258; ++i) add258 (l, i);
    for (int i = 0; i < kMax259; ++i) add259 (l, i);
    add285 (l);
    add296 (l);
    add277 (l);
    for (int i = 0; i < kMax292; ++i) add292 (l, i);
    add207 (l);
    add267 (l);
    add275 (l);
    add284 (l);
    add291 (l);
    for (int i = 0; i < kMax666; ++i) add666 (l, i);
    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("out_grp", "Output", "|");
        g->addChild (flt (juce::ParameterID ("out_level", 1), "Output Level", NR (0.0f, 1.0f), 0.8f));
        g->addChild (flt (juce::ParameterID ("out_cv", 1), "Output Level CV Amount", NR (0.0f, 1.0f), 1.0f));
        g->addChild (flt (juce::ParameterID ("out_lag", 1), "Output Lag", NR (1.0f, 300.0f, 0.0f, 0.4f), 12.0f, unitAttr (" ms", 0)));
        g->addChild (flt (juce::ParameterID ("out_master", 1), "Master", NR (-48.0f, 6.0f), -3.0f, unitAttr (" dB", 1)));
        l.add (std::move (g));
    }
    return l;
}
} // namespace wc
