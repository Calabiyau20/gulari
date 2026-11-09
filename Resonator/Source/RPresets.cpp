#include "RPresets.h"
#include "RParams.h"
#include <map>
#include <algorithm>

namespace cyr
{
namespace
{
using KV = std::initializer_list<std::pair<const char*, float>>;

struct Build
{
    FactoryPreset p;
    int slot = 0;
    explicit Build (const char* name) { p.name = name; }
    Build& set (KV kv) { for (auto& e : kv) p.values.push_back ({ e.first, e.second }); return *this; }
    Build& osc (char o, KV kv)
    {
        const juce::String pre = juce::String ("osc") + juce::String::charToString (o) + "_";
        p.values.push_back ({ pre + "on", 1.0f });
        for (auto& e : kv) p.values.push_back ({ pre + e.first, e.second });
        return *this;
    }
    Build& mod (int src, const char* id, float amt)
    {
        const int dst = destIndexOfParam (indexOfR (id));
        jassert (dst > 0);
        const juce::String sid = "mod" + juce::String (slot / 8) + juce::String (slot % 8 + 1);
        p.values.push_back ({ sid + "_src", (float) src });
        p.values.push_back ({ sid + "_dst", (float) dst });
        p.values.push_back ({ sid + "_amt", amt });
        ++slot;
        return *this;
    }
    Build& lfo (int n, KV kv)
    {
        const juce::String pre = "lfo" + juce::String (n) + "_";
        for (auto& e : kv) p.values.push_back ({ pre + e.first, e.second });
        return *this;
    }
    Build& env (int n, float a, float d, float s, float r)
    {
        const juce::String pre = "env" + juce::String (n) + "_";
        p.values.push_back ({ pre + "a", a }); p.values.push_back ({ pre + "d", d });
        p.values.push_back ({ pre + "s", s }); p.values.push_back ({ pre + "r", r });
        return *this;
    }
    Build& amp (float a, float d, float s, float r) { return set ({ { "amp_a", a }, { "amp_d", d }, { "amp_s", s }, { "amp_r", r } }); }
    Build& reverb (float mix, float decay = 3.0f, float size = 0.7f)
    { return set ({ { "fx_rv_on", 1 }, { "fx_rv_mix", mix }, { "fx_rv_decay", decay }, { "fx_rv_size", size } }); }
    Build& delay (float mix, float fb = 0.4f, int div = 8)
    { return set ({ { "fx_dl_on", 1 }, { "fx_dl_mix", mix }, { "fx_dl_fb", fb }, { "fx_dl_div", (float) div } }); }
};

std::vector<FactoryPreset> make()
{
    std::vector<FactoryPreset> v;
    auto add = [&] (Build& b) { v.push_back (b.p); };

    { Build b ("Pad - Quintic Bloom");
      b.osc ('a', { { "n", 5 }, { "k1", 1 }, { "k2", 2 }, { "ru", 0.55f }, { "rv", 0.6f }, { "ratio", 2 }, { "spin", 0.15f }, { "uni", 3 }, { "det", 12 }, { "width", 0.85f }, { "level", 0.7f } })
       .osc ('b', { { "n", 5 }, { "k1", 3 }, { "k2", 4 }, { "ru", 0.4f }, { "rv", 0.5f }, { "ratio", 1 }, { "oct", -1 }, { "level", 0.45f }, { "width", 0.5f } })
       .amp (0.9f, 1.0f, 0.9f, 3.5f).set ({ { "flt_cut", 5200 }, { "flt_res", 0.1f } })
       .lfo (1, { { "rate", 0.11f } }).mod (MS_Lfo1, "osca_alpha", 0.18f)
       .lfo (2, { { "rate", 0.07f }, { "shape", 6 } }).mod (MS_Lfo2, "oscb_rv", 0.25f)
       .reverb (0.38f, 5.0f, 0.85f).set ({ { "mb_ch_on", 1 }, { "mb_ch_mix", 0.3f } });
      add (b); }

    { Build b ("Bass - Fermat Cubic");
      b.osc ('a', { { "n", 3 }, { "ru", 0.9f }, { "rv", 0.2f }, { "ratio", 1 }, { "oct", -1 }, { "width", 0.2f }, { "level", 0.8f }, { "rphase", 0 } })
       .osc ('b', { { "n", 2 }, { "ru", 0.3f }, { "rv", 0.0f }, { "oct", -2 }, { "width", 0.0f }, { "level", 0.55f }, { "rphase", 0 } })
       .set ({ { "fm_ba", 0.35f }, { "poly", 1 }, { "glide", 0.04f }, { "flt_cut", 700 }, { "flt_res", 0.3f }, { "flt_drive", 0.25f } })
       .amp (0.002f, 0.5f, 0.75f, 0.15f).env (1, 0.001f, 0.28f, 0.0f, 0.2f).mod (MS_Env1, "flt_cut", 0.45f).mod (MS_Env1, "fm_ba", 0.3f);
      add (b); }

    { Build b ("Lead - Brane Runner");
      b.osc ('a', { { "n", 4 }, { "k1", 1 }, { "ru", 0.85f }, { "rv", 0.3f }, { "ratio", 3 }, { "uni", 2 }, { "det", 9 }, { "level", 0.75f } })
       .osc ('b', { { "n", 6 }, { "semi", 7 }, { "ru", 0.7f }, { "rv", 0.2f }, { "level", 0.35f } })
       .set ({ { "pd_aa", 0.35f }, { "poly", 1 }, { "glide", 0.06f }, { "flt_cut", 6500 } })
       .amp (0.004f, 0.4f, 0.8f, 0.3f)
       .lfo (1, { { "rate", 5.5f }, { "fade", 0.5f }, { "trig", 1 } }).mod (MS_Lfo1, "osca_fine", 0.06f)
       .mod (MS_ModWheel, "pd_aa", 0.5f).delay (0.22f, 0.35f, 8).reverb (0.18f);
      add (b); }

    { Build b ("Bell - Compactified");
      b.osc ('a', { { "n", 7 }, { "k1", 2 }, { "k2", 5 }, { "ru", 0.4f }, { "rv", 0.9f }, { "ratio", 5 }, { "spin", 2.5f }, { "width", 0.9f } })
       .osc ('b', { { "n", 5 }, { "oct", 1 }, { "ru", 0.2f }, { "rv", 0.3f }, { "level", 0.35f } })
       .set ({ { "fm_ba", 0.3f }, { "flt_on", 0 } })
       .amp (0.002f, 2.8f, 0.0f, 2.6f).env (1, 0.001f, 1.2f, 0.0f, 1.0f).mod (MS_Env1, "fm_ba", 0.45f).mod (MS_Env1, "osca_rv", -0.3f)
       .reverb (0.42f, 4.5f, 0.8f);
      add (b); }

    { Build b ("Pluck - Kahler");
      b.osc ('a', { { "n", 3 }, { "k1", 1 }, { "ru", 0.7f }, { "rv", 0.5f }, { "ratio", 2 }, { "level", 0.8f } })
       .osc ('b', { { "n", 4 }, { "oct", 1 }, { "ru", 0.5f }, { "rv", 0.2f }, { "level", 0.0f } })
       .set ({ { "pd_ba", 0.55f }, { "flt_cut", 500 }, { "flt_res", 0.25f } })
       .amp (0.001f, 0.45f, 0.0f, 0.35f).env (1, 0.001f, 0.22f, 0.0f, 0.2f).mod (MS_Env1, "flt_cut", 0.55f).mod (MS_Env1, "pd_ba", 0.35f)
       .mod (MS_Velocity, "flt_cut", 0.15f).delay (0.28f, 0.45f, 8).reverb (0.2f);
      add (b); }

    { Build b ("Keys - Mirror Symmetry");
      b.osc ('a', { { "n", 4 }, { "k1", 1 }, { "k2", 1 }, { "ru", 0.5f }, { "rv", 0.25f }, { "width", 1.0f }, { "pan", -0.3f }, { "level", 0.6f } })
       .osc ('b', { { "n", 4 }, { "k1", 1 }, { "k2", 1 }, { "ru", 0.5f }, { "rv", 0.25f }, { "alpha", 4.0f }, { "width", 1.0f }, { "pan", 0.3f }, { "level", 0.6f }, { "fine", 6 } })
       .amp (0.006f, 1.4f, 0.35f, 0.7f).set ({ { "flt_cut", 4200 }, { "flt_key", 0.5f } })
       .env (1, 0.001f, 0.6f, 0.0f, 0.5f).mod (MS_Env1, "flt_cut", 0.25f)
       .set ({ { "fx_ph_on", 1 }, { "fx_ph_mix", 0.3f }, { "fx_ph_rate", 0.2f } }).reverb (0.22f);
      add (b); }

    { Build b ("Bass - Psy Brane Growl");
      b.osc ('a', { { "n", 6 }, { "ru", 0.95f }, { "rv", 0.15f }, { "ratio", 1 }, { "oct", -1 }, { "width", 0.15f }, { "rphase", 0 } })
       .set ({ { "poly", 1 }, { "flt_cut", 1300 }, { "flt_res", 0.45f }, { "flt_drive", 0.4f }, { "pd_aa", 0.2f } })
       .amp (0.001f, 0.18f, 0.55f, 0.08f)
       .lfo (1, { { "sync", 1 }, { "div", 4 }, { "shape", 0 } }).mod (MS_Lfo1, "osca_alpha", 0.45f).mod (MS_Lfo1, "flt_cut", 0.25f)
       .env (1, 0.001f, 0.12f, 0.0f, 0.1f).mod (MS_Env1, "pd_aa", 0.4f)
       .set ({ { "mb_tp_on", 1 }, { "mb_tp_drive", 0.35f } });
      add (b); }

    { Build b ("Pad - String Landscape");
      b.osc ('a', { { "n", 3 }, { "ru", 0.6f }, { "rv", 0.5f }, { "ratio", 2 }, { "level", 0.45f }, { "pan", -0.5f } })
       .osc ('b', { { "n", 4 }, { "k1", 1 }, { "ru", 0.5f }, { "rv", 0.6f }, { "ratio", 3 }, { "level", 0.4f }, { "pan", 0.5f }, { "fine", 7 } })
       .osc ('c', { { "n", 5 }, { "k2", 2 }, { "ru", 0.4f }, { "rv", 0.7f }, { "oct", 1 }, { "level", 0.25f }, { "pan", -0.2f } })
       .osc ('d', { { "n", 6 }, { "k1", 3 }, { "ru", 0.7f }, { "rv", 0.3f }, { "oct", -1 }, { "level", 0.35f }, { "pan", 0.2f } })
       .amp (1.6f, 1.0f, 1.0f, 4.5f).set ({ { "flt_cut", 3800 } })
       .lfo (1, { { "rate", 0.07f } }).lfo (2, { { "rate", 0.11f } }).lfo (3, { { "rate", 0.13f } }).lfo (4, { { "rate", 0.17f } })
       .mod (MS_Lfo1, "osca_alpha", 0.25f).mod (MS_Lfo2, "oscb_alpha", 0.25f).mod (MS_Lfo3, "oscc_alpha", 0.25f).mod (MS_Lfo4, "oscd_alpha", 0.25f)
       .reverb (0.45f, 7.0f, 0.95f);
      add (b); }

    { Build b ("Pluck - Hodge Diamond");
      b.osc ('a', { { "n", 5 }, { "k1", 2 }, { "ru", 0.8f }, { "rv", 0.4f }, { "ratio", 4 }, { "width", 0.9f } })
       .amp (0.001f, 0.3f, 0.0f, 0.25f).set ({ { "flt_cut", 2600 }, { "flt_res", 0.35f } })
       .env (1, 0.001f, 0.15f, 0.0f, 0.15f).mod (MS_Env1, "osca_rv", 0.4f).mod (MS_Key, "flt_cut", 0.2f)
       .delay (0.35f, 0.5f, 7).reverb (0.25f);
      add (b); }

    { Build b ("Drone - Orbifold");
      b.osc ('a', { { "n", 7 }, { "ru", 0.6f }, { "rv", 0.7f }, { "ratio", 2 }, { "oct", -2 }, { "spin", 0.3f }, { "level", 0.7f } })
       .osc ('b', { { "n", 5 }, { "k1", 1 }, { "ru", 0.5f }, { "rv", 0.5f }, { "ratio", 3 }, { "oct", -1 }, { "spin", -0.2f }, { "level", 0.5f } })
       .set ({ { "fm_ab", 0.25f } }).amp (3.0f, 1.0f, 1.0f, 6.0f)
       .lfo (5, { { "rate", 0.09f }, { "shape", 6 } }).mod (MS_Lfo5, "osca_alpha", 0.4f).mod (MS_Lfo5, "fm_ab", 0.3f)
       .lfo (6, { { "rate", 0.05f } }).mod (MS_Lfo6, "flt_cut", 0.2f).set ({ { "flt_cut", 2500 } })
       .reverb (0.5f, 10.0f, 1.0f);
      add (b); }

    { Build b ("FX - Tachyon Sweep");
      b.osc ('a', { { "n", 5 }, { "ru", 0.9f }, { "rv", 0.9f }, { "ratio", 6 }, { "spin", 4 }, { "width", 1 } })
       .amp (0.05f, 1.0f, 1.0f, 3.0f).env (1, 3.0f, 2.0f, 0.0f, 2.0f).mod (MS_Env1, "osca_spin", -0.6f).mod (MS_Env1, "osca_semi", 0.5f)
       .mod (MS_Env1, "osca_rv", -0.6f).set ({ { "fx_fl_on", 1 }, { "fx_fl_mix", 0.4f } }).reverb (0.4f, 6.0f).delay (0.3f, 0.6f, 9);
      add (b); }

    { Build b ("Keys - Moduli Organ");
      b.osc ('a', { { "n", 2 }, { "ru", 0.35f }, { "rv", 0.1f }, { "level", 0.6f }, { "rphase", 0 } })
       .osc ('b', { { "n", 3 }, { "ru", 0.3f }, { "rv", 0.15f }, { "oct", 1 }, { "level", 0.4f } })
       .osc ('c', { { "n", 4 }, { "ru", 0.25f }, { "rv", 0.1f }, { "oct", 2 }, { "level", 0.25f } })
       .amp (0.01f, 0.2f, 1.0f, 0.15f).set ({ { "flt_on", 0 }, { "mb_ch_on", 1 }, { "mb_ch_mix", 0.45f }, { "mb_ch_rate", 4.5f } })
       .reverb (0.2f);
      add (b); }

    { Build b ("Bass - Calabi Sub");
      b.osc ('a', { { "n", 2 }, { "ru", 0.5f }, { "rv", 0.0f }, { "oct", -1 }, { "width", 0.0f }, { "rphase", 0 } })
       .set ({ { "poly", 1 }, { "glide", 0.03f }, { "flt_cut", 420 } }).amp (0.003f, 0.6f, 0.9f, 0.12f);
      add (b); }

    { Build b ("Lead - Wormhole PD");
      b.osc ('a', { { "n", 5 }, { "k1", 1 }, { "ru", 0.75f }, { "rv", 0.35f }, { "ratio", 2 }, { "level", 0.8f } })
       .osc ('b', { { "n", 3 }, { "oct", 1 }, { "ru", 0.6f }, { "rv", 0.2f }, { "level", 0.0f } })
       .set ({ { "pd_ba", 0.5f }, { "poly", 1 }, { "glide", 0.08f }, { "flt_cut", 7000 } })
       .lfo (1, { { "rate", 0.35f }, { "shape", 1 } }).mod (MS_Lfo1, "pd_ba", 0.4f)
       .amp (0.01f, 0.3f, 0.85f, 0.35f).delay (0.25f, 0.4f, 9).reverb (0.22f);
      add (b); }

    { Build b ("Pad - Hyperspace Choir");
      b.osc ('a', { { "n", 4 }, { "k1", 2 }, { "ru", 0.45f }, { "rv", 0.55f }, { "ratio", 2 }, { "uni", 4 }, { "det", 22 }, { "width", 1 } })
       .osc ('b', { { "n", 6 }, { "k2", 3 }, { "ru", 0.35f }, { "rv", 0.4f }, { "ratio", 3 }, { "uni", 3 }, { "det", 15 }, { "oct", 1 }, { "level", 0.3f } })
       .amp (1.2f, 1.0f, 1.0f, 3.5f).set ({ { "flt_type", 1 }, { "flt_cut", 1400 }, { "flt_res", 0.35f } })
       .lfo (1, { { "rate", 0.15f }, { "shape", 6 } }).mod (MS_Lfo1, "flt_cut", 0.3f)
       .reverb (0.45f, 6.0f, 0.9f);
      add (b); }

    { Build b ("Bass - Kaluza-Klein");
      b.osc ('a', { { "n", 4 }, { "ru", 0.8f }, { "rv", 0.25f }, { "oct", -1 }, { "width", 0.3f }, { "rphase", 0 } })
       .osc ('c', { { "n", 3 }, { "ru", 0.5f }, { "rv", 0.2f }, { "oct", 0 }, { "semi", 7 }, { "level", 0.0f }, { "rphase", 0 } })
       .set ({ { "fm_ca", 0.25f }, { "poly", 1 }, { "flt_cut", 1600 }, { "flt_res", 0.2f } })
       .env (2, 0.001f, 0.16f, 0.0f, 0.1f).mod (MS_Env2, "fm_ca", 0.55f).amp (0.001f, 0.35f, 0.6f, 0.1f);
      add (b); }

    { Build b ("Stab - Singularity Chord");
      b.osc ('a', { { "n", 5 }, { "ru", 0.7f }, { "rv", 0.4f }, { "level", 0.5f } })
       .osc ('b', { { "n", 5 }, { "ru", 0.7f }, { "rv", 0.4f }, { "semi", 3 }, { "level", 0.45f } })
       .osc ('c', { { "n", 5 }, { "ru", 0.7f }, { "rv", 0.4f }, { "semi", 7 }, { "level", 0.45f } })
       .osc ('d', { { "n", 5 }, { "ru", 0.7f }, { "rv", 0.4f }, { "semi", 10 }, { "level", 0.4f } })
       .amp (0.001f, 0.55f, 0.0f, 0.4f).env (1, 0.001f, 0.3f, 0.0f, 0.3f).mod (MS_Env1, "flt_cut", 0.5f).set ({ { "flt_cut", 900 }, { "flt_res", 0.3f } })
       .delay (0.3f, 0.45f, 9).reverb (0.35f);
      add (b); }

    { Build b ("FX - Dimension Seven");
      b.osc ('a', { { "n", 7 }, { "k1", 3 }, { "k2", 6 }, { "ru", 0.95f }, { "rv", 1.0f }, { "ratio", 7 }, { "spin", 5.5f }, { "uni", 2 }, { "det", 30 } })
       .set ({ { "pd_aa", 0.3f } }).amp (0.4f, 1.0f, 1.0f, 2.5f)
       .lfo (1, { { "rate", 7.0f }, { "shape", 5 } }).mod (MS_Lfo1, "osca_alpha", 0.3f)
       .set ({ { "fx_gr_on", 1 }, { "fx_gr_mix", 0.35f } }).reverb (0.35f);
      add (b); }

    { Build b ("Texture - Fold Space");
      b.osc ('a', { { "n", 6 }, { "k1", 2 }, { "ru", 0.5f }, { "rv", 0.8f }, { "ratio", 3 }, { "spin", 0.6f } })
       .osc ('b', { { "n", 7 }, { "k2", 4 }, { "ru", 0.6f }, { "rv", 0.6f }, { "ratio", 5 }, { "oct", 1 }, { "level", 0.3f }, { "spin", -0.9f } })
       .set ({ { "fm_ab", 0.2f }, { "fm_ba", 0.15f } }).amp (1.5f, 1.0f, 1.0f, 5.0f)
       .lfo (2, { { "rate", 0.2f }, { "shape", 6 } }).mod (MS_Lfo2, "fm_ab", 0.25f)
       .set ({ { "fx_gr_on", 1 }, { "fx_gr_mix", 0.45f }, { "fx_gr_size", 0.7f } }).reverb (0.5f, 8.0f, 0.95f);
      add (b); }

    { Build b ("Lead - Event Horizon");
      b.osc ('a', { { "n", 3 }, { "ru", 0.92f }, { "rv", 0.5f }, { "ratio", 1 }, { "uni", 3 }, { "det", 14 }, { "width", 0.8f } })
       .osc ('b', { { "n", 3 }, { "ru", 0.92f }, { "rv", 0.5f }, { "oct", -1 }, { "level", 0.4f } })
       .set ({ { "poly", 1 }, { "glide", 0.1f }, { "flt_cut", 5000 }, { "flt_drive", 0.3f } })
       .amp (0.005f, 0.5f, 0.85f, 0.4f).mod (MS_Aftertouch, "flt_cut", 0.3f).mod (MS_ModWheel, "osca_alpha", 0.5f)
       .delay (0.2f, 0.35f, 8).reverb (0.2f);
      add (b); }

    { Build b ("Arp - Spin Network");
      b.osc ('a', { { "n", 4 }, { "k1", 1 }, { "k2", 3 }, { "ru", 0.65f }, { "rv", 0.45f }, { "ratio", 2 }, { "spin", 1.5f }, { "level", 1.0f } })
       .osc ('b', { { "n", 3 }, { "ru", 0.8f }, { "rv", 0.3f }, { "oct", 1 }, { "level", 0.4f } })
       .amp (0.001f, 0.32f, 0.15f, 0.25f).set ({ { "flt_cut", 5200 }, { "flt_res", 0.35f } })
       .lfo (1, { { "sync", 1 }, { "div", 12 }, { "shape", 0 } }).mod (MS_Lfo1, "flt_cut", 0.3f)
       .mod (MS_Random, "osca_alpha", 0.3f).delay (0.35f, 0.55f, 6).reverb (0.25f);
      add (b); }

    // ---- SCAN: the whole manifold used as a wavetable ----
    { Build b ("Scan - Manifold Sweep");
      b.osc ('a', { { "mode", 1 }, { "n", 5 }, { "cover", 0.02f }, { "pos", 0.1f }, { "width", 0.8f }, { "uni", 2 }, { "det", 10 } })
       .amp (0.05f, 1.0f, 0.9f, 1.2f).set ({ { "flt_cut", 6000 } })
       .lfo (1, { { "rate", 0.05f }, { "shape", 1 } }).mod (MS_Lfo1, "osca_pos", 0.45f)
       .reverb (0.3f, 4.0f);
      add (b); }

    { Build b ("Scan - Full Coverage Drone");
      b.osc ('a', { { "mode", 1 }, { "n", 3 }, { "cover", 1.0f }, { "oct", -2 }, { "width", 1.0f } })
       .osc ('b', { { "n", 3 }, { "ru", 0.5f }, { "rv", 0.3f }, { "oct", -1 }, { "level", 0.5f } })
       .amp (2.0f, 1.0f, 1.0f, 5.0f).set ({ { "flt_cut", 2400 }, { "flt_res", 0.2f } })
       .lfo (2, { { "rate", 0.04f }, { "shape", 6 } }).mod (MS_Lfo2, "osca_alpha", 0.3f).mod (MS_Lfo2, "flt_cut", 0.15f)
       .reverb (0.45f, 8.0f, 0.95f);
      add (b); }

    { Build b ("Scan - Coverage Pluck");
      b.osc ('a', { { "mode", 1 }, { "n", 4 }, { "cover", 0.0f }, { "pos", 0.4f }, { "width", 0.7f } })
       .amp (0.001f, 0.6f, 0.0f, 0.4f).env (1, 0.001f, 0.35f, 0.0f, 0.3f).mod (MS_Env1, "osca_cover", 0.12f)
       .set ({ { "flt_cut", 7000 } }).delay (0.25f, 0.4f, 8).reverb (0.2f);
      add (b); }

    { Build b ("Scan - Wavetable Bass");
      b.osc ('a', { { "mode", 1 }, { "n", 6 }, { "cover", 0.01f }, { "pos", 0.3f }, { "oct", -1 }, { "width", 0.1f }, { "rphase", 0 } })
       .osc ('b', { { "n", 2 }, { "ru", 0.4f }, { "rv", 0.0f }, { "oct", -2 }, { "level", 0.5f }, { "rphase", 0 } })
       .set ({ { "poly", 1 }, { "glide", 0.04f }, { "flt_cut", 1500 }, { "flt_res", 0.3f } })
       .amp (0.002f, 0.4f, 0.7f, 0.12f).env (1, 0.001f, 0.25f, 0.0f, 0.2f).mod (MS_Env1, "osca_pos", 0.25f).mod (MS_Env1, "flt_cut", 0.35f);
      add (b); }

    // ---- the generated bank (tools/gen_presets.py): 300 presets in 12 categories, loudness already matched ----
   #include "RPresetsBank.inc"

    // loudness match: master trims measured with ResonatorRenderTest (loudest 400 ms at -15 dB RMS)
    static const std::map<juce::String, float> trims {
        { "Arp - Spin Network", 3.3f },
        { "Bass - Calabi Sub", -4.4f },
        { "Bass - Fermat Cubic", -1.2f },
        { "Bass - Kaluza-Klein", 1.2f },
        { "Bass - Psy Brane Growl", -1.7f },
        { "Bell - Compactified", -9.7f },
        { "Drone - Orbifold", -6.9f },
        { "FX - Dimension Seven", -11.0f },
        { "FX - Tachyon Sweep", -3.4f },
        { "Keys - Mirror Symmetry", -5.4f },
        { "Keys - Moduli Organ", -14.0f },
        { "Lead - Brane Runner", 0.2f },
        { "Lead - Event Horizon", -0.2f },
        { "Lead - Wormhole PD", 2.6f },
        { "Pad - Hyperspace Choir", -1.5f },
        { "Pad - Quintic Bloom", 3.3f },
        { "Pad - String Landscape", -2.5f },
        { "Pluck - Hodge Diamond", 0.2f },
        { "Pluck - Kahler", 1.8f },
        { "Scan - Coverage Pluck", -0.2f },
        { "Scan - Full Coverage Drone", -1.4f },
        { "Scan - Manifold Sweep", -0.3f },
        { "Scan - Wavetable Bass", -2.1f },
        { "Stab - Singularity Chord", -12.5f },
        { "Texture - Fold Space", 1.4f },
    };
    for (auto& p : v)
    {
        auto it = trims.find (p.name);
        if (it != trims.end()) p.values.push_back ({ "master", it->second });
    }
    std::sort (v.begin(), v.end(), [] (const FactoryPreset& a, const FactoryPreset& b) { return a.name < b.name; });
    return v;
}
} // namespace

const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> v = make();
    return v;
}
} // namespace cyr
